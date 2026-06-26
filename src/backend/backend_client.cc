#include "backend/backend_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <utility>

#include "app/program_settings.h"

namespace cppwiki::backend {

namespace {

constexpr int kHealthRefreshIntervalMs = 15000;
constexpr int kHealthRequestTimeoutMs = 2000;
constexpr int kLockHeartbeatIntervalMs = 10000;
constexpr int kLockRequestTimeoutMs = 3000;

auto MakeRequest(QUrl url, int timeout_ms, const QString& access_token) -> QNetworkRequest {
  QNetworkRequest request{std::move(url)};
  request.setTransferTimeout(timeout_ms);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  if (!access_token.trimmed().isEmpty()) {
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(access_token.trimmed()).toUtf8());
  }
  return request;
}

auto ReplyErrorText(QNetworkReply* reply) -> QString {
  if (reply == nullptr) {
    return QStringLiteral("Unknown network error");
  }

  const auto status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (reply->error() == QNetworkReply::NoError && status_code >= 200 && status_code < 300) {
    return {};
  }

  if (reply->error() != QNetworkReply::NoError) {
    return reply->errorString();
  }

  return QStringLiteral("HTTP %1").arg(status_code);
}

auto ParseJsonObject(QNetworkReply* reply) -> QJsonObject {
  if (reply == nullptr) {
    return {};
  }

  const auto document = QJsonDocument::fromJson(reply->readAll());
  return document.isObject() ? document.object() : QJsonObject{};
}

auto EnvelopeResult(const QJsonObject& object) -> QJsonObject {
  return object.value(QStringLiteral("result")).toObject();
}

}  // namespace

BackendClient::BackendClient(QObject* parent)
    : QObject(parent),
      network_manager_(new QNetworkAccessManager(this)),
      refresh_timer_(new QTimer(this)),
      heartbeat_timer_(new QTimer(this)) {
  refresh_timer_->setInterval(kHealthRefreshIntervalMs);
  connect(refresh_timer_, &QTimer::timeout, this, &BackendClient::RefreshHealth);

  heartbeat_timer_->setInterval(kLockHeartbeatIntervalMs);
  connect(heartbeat_timer_, &QTimer::timeout, this, &BackendClient::SendHeartbeat);
}

void BackendClient::ApplySettings(const ProgramSettings& settings) {
  enabled_ = settings.BackendEnabled();
  base_url_ = settings.BackendBaseUrl().trimmed();

  if (!enabled_) {
    refresh_timer_->stop();
    AbortInFlightRequest();
    CloseDocumentSession();
    SetStatus(BackendConnectionState::kLocalOnly, QStringLiteral("Backend: local only"));
    return;
  }

  if (base_url_.isEmpty()) {
    refresh_timer_->stop();
    AbortInFlightRequest();
    CloseDocumentSession();
    SetStatus(BackendConnectionState::kUnavailable, QStringLiteral("Backend: URL missing"));
    return;
  }

  RefreshHealth();
  if (!refresh_timer_->isActive()) {
    refresh_timer_->start();
  }
}

void BackendClient::RefreshHealth() {
  if (!enabled_) {
    SetStatus(BackendConnectionState::kLocalOnly, QStringLiteral("Backend: local only"));
    return;
  }

  if (base_url_.isEmpty()) {
    SetStatus(BackendConnectionState::kUnavailable, QStringLiteral("Backend: URL missing"));
    return;
  }

  AbortInFlightRequest();
  SetStatus(BackendConnectionState::kChecking,
            QStringLiteral("Backend: checking %1").arg(base_url_));

  const auto request = MakeRequest(QUrl{HealthUrl()}, kHealthRequestTimeoutMs, access_token_);
  in_flight_reply_ = network_manager_->get(request);

  connect(in_flight_reply_, &QNetworkReply::finished, this, [this]() {
    if (in_flight_reply_ == nullptr) {
      return;
    }

    auto* reply = in_flight_reply_;
    in_flight_reply_ = nullptr;
    const auto status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok =
        reply->error() == QNetworkReply::NoError && status_code >= 200 && status_code < 300;

    if (ok) {
      SetStatus(BackendConnectionState::kReachable,
                QStringLiteral("Backend: reachable at %1").arg(base_url_));
    } else {
      const auto error_text = ReplyErrorText(reply);
      SetStatus(BackendConnectionState::kUnavailable,
                QStringLiteral("Backend: unavailable (%1)").arg(error_text));
    }

    reply->deleteLater();
  });
}

void BackendClient::SetAccessToken(QString access_token) {
  access_token_ = std::move(access_token);
}

void BackendClient::OpenDocumentSession(const QString& document_id,
                                        std::function<void(DocumentAccessState)> callback) {
  ++session_request_id_;
  const auto request_id = session_request_id_;

  if (document_id.trimmed().isEmpty()) {
    callback(DocumentAccessState{
        .editable = true,
        .local_only = true,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: local only"),
    });
    return;
  }

  if (!enabled_ || state_ == BackendConnectionState::kLocalOnly) {
    CloseDocumentSession();
    callback(DocumentAccessState{
        .editable = true,
        .local_only = true,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: local-only editing"),
    });
    return;
  }

  if (state_ != BackendConnectionState::kReachable) {
    CloseDocumentSession();
    callback(DocumentAccessState{
        .editable = true,
        .local_only = true,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: backend unavailable, editing locally"),
    });
    return;
  }

  if (active_document_id_ == document_id && heartbeat_timer_->isActive()) {
    callback(DocumentAccessState{
        .editable = true,
        .local_only = false,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: editing with backend lock"),
    });
    return;
  }

  auto continue_with_acquire = [this, document_id, callback = std::move(callback),
                                request_id]() mutable {
    AcquireDocumentLock(document_id, std::move(callback), request_id);
  };

  if (!active_document_id_.isEmpty() && active_document_id_ != document_id) {
    ReleaseDocumentLock(active_document_id_, std::move(continue_with_acquire));
    return;
  }

  continue_with_acquire();
}

void BackendClient::CloseDocumentSession() {
  ++session_request_id_;
  StopHeartbeat();
  AbortSessionRequest();
  AbortHeartbeatRequest();

  if (active_document_id_.isEmpty()) {
    return;
  }

  const auto document_id = active_document_id_;
  active_document_id_.clear();
  ReleaseDocumentLock(document_id, []() {});
}

auto BackendClient::State() const -> BackendConnectionState {
  return state_;
}

auto BackendClient::BaseUrl() const -> const QString& {
  return base_url_;
}

auto BackendClient::StatusText() const -> const QString& {
  return status_text_;
}

void BackendClient::SetStatus(BackendConnectionState state, QString status_text) {
  if (state_ == state && status_text_ == status_text) {
    return;
  }

  state_ = state;
  status_text_ = std::move(status_text);
  emit statusChanged(state_, status_text_);
}

void BackendClient::AbortInFlightRequest() {
  if (in_flight_reply_ == nullptr) {
    return;
  }

  disconnect(in_flight_reply_, nullptr, this, nullptr);
  in_flight_reply_->abort();
  in_flight_reply_->deleteLater();
  in_flight_reply_ = nullptr;
}

void BackendClient::AbortSessionRequest() {
  if (session_reply_ == nullptr) {
    return;
  }

  disconnect(session_reply_, nullptr, this, nullptr);
  session_reply_->abort();
  session_reply_->deleteLater();
  session_reply_ = nullptr;
}

void BackendClient::AbortHeartbeatRequest() {
  if (heartbeat_reply_ == nullptr) {
    return;
  }

  disconnect(heartbeat_reply_, nullptr, this, nullptr);
  heartbeat_reply_->abort();
  heartbeat_reply_->deleteLater();
  heartbeat_reply_ = nullptr;
}

void BackendClient::ReleaseDocumentLock(const QString& document_id,
                                        std::function<void()> continuation) {
  AbortSessionRequest();

  if (document_id.trimmed().isEmpty() || state_ != BackendConnectionState::kReachable) {
    continuation();
    return;
  }

  const auto request =
      MakeRequest(QUrl{ApiUrl(QStringLiteral("/api/v1/locks/%1").arg(document_id))},
                  kLockRequestTimeoutMs, access_token_);
  session_reply_ = network_manager_->sendCustomRequest(request, "DELETE", QByteArrayLiteral("{}"));

  connect(session_reply_, &QNetworkReply::finished, this,
          [this, continuation = std::move(continuation)]() mutable {
            if (session_reply_ == nullptr) {
              continuation();
              return;
            }

            auto* reply = session_reply_;
            session_reply_ = nullptr;
            reply->deleteLater();
            continuation();
          });
}

void BackendClient::AcquireDocumentLock(const QString& document_id,
                                        std::function<void(DocumentAccessState)> callback,
                                        std::uint64_t request_id) {
  AbortSessionRequest();

  const auto request =
      MakeRequest(QUrl{ApiUrl(QStringLiteral("/api/v1/locks/%1").arg(document_id))},
                  kLockRequestTimeoutMs, access_token_);
  session_reply_ = network_manager_->post(request, QByteArrayLiteral("{}"));

  connect(session_reply_, &QNetworkReply::finished, this,
          [this, document_id, callback = std::move(callback), request_id]() mutable {
            if (session_reply_ == nullptr) {
              return;
            }

            auto* reply = session_reply_;
            session_reply_ = nullptr;

            if (request_id != session_request_id_) {
              reply->deleteLater();
              return;
            }

            const auto error_text = ReplyErrorText(reply);
            if (!error_text.isEmpty()) {
              reply->deleteLater();
              callback(DocumentAccessState{
                  .editable = false,
                  .local_only = false,
                  .lock_owner = {},
                  .status_text = QStringLiteral("Document: lock request failed (%1)")
                                     .arg(error_text),
              });
              return;
            }

            const auto payload = ParseJsonObject(reply);
            reply->deleteLater();

            if (!payload.value(QStringLiteral("ok")).toBool()) {
              const auto error_message =
                  payload.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
              callback(DocumentAccessState{
                  .editable = false,
                  .local_only = false,
                  .lock_owner = {},
                  .status_text = error_message.isEmpty()
                                     ? QStringLiteral("Document: lock request rejected")
                                     : QStringLiteral("Document: %1").arg(error_message),
              });
              return;
            }

            const auto result = EnvelopeResult(payload);
            const bool acquired = result.value(QStringLiteral("acquired")).toBool();
            const auto owner = result.value(QStringLiteral("owner")).toString();

            if (acquired) {
              active_document_id_ = document_id;
              StartHeartbeat();
              callback(DocumentAccessState{
                  .editable = true,
                  .local_only = false,
                  .lock_owner = owner,
                  .status_text = QStringLiteral("Document: editing with backend lock"),
              });
              return;
            }

            callback(DocumentAccessState{
                .editable = false,
                .local_only = false,
                .lock_owner = owner,
                .status_text = owner.trimmed().isEmpty()
                                   ? QStringLiteral("Document: read-only, lock unavailable")
                                   : QStringLiteral("Document: read-only, locked by %1").arg(owner),
            });
          });
}

void BackendClient::StartHeartbeat() {
  if (!heartbeat_timer_->isActive()) {
    heartbeat_timer_->start();
  }
}

void BackendClient::StopHeartbeat() {
  heartbeat_timer_->stop();
}

void BackendClient::SendHeartbeat() {
  if (active_document_id_.isEmpty() || state_ != BackendConnectionState::kReachable) {
    return;
  }

  AbortHeartbeatRequest();
  const auto document_id = active_document_id_;
  const auto request =
      MakeRequest(QUrl{ApiUrl(QStringLiteral("/api/v1/locks/%1").arg(document_id))},
                  kLockRequestTimeoutMs, access_token_);
  heartbeat_reply_ = network_manager_->sendCustomRequest(request, "PUT", QByteArrayLiteral("{}"));

  connect(heartbeat_reply_, &QNetworkReply::finished, this, [this, document_id]() {
    if (heartbeat_reply_ == nullptr) {
      return;
    }

    auto* reply = heartbeat_reply_;
    heartbeat_reply_ = nullptr;

    if (active_document_id_ != document_id) {
      reply->deleteLater();
      return;
    }

    const auto error_text = ReplyErrorText(reply);
    if (!error_text.isEmpty()) {
      reply->deleteLater();
      active_document_id_.clear();
      StopHeartbeat();
      emit documentAccessInvalidated(
          document_id, QString{},
          QStringLiteral("Document: read-only, lock heartbeat failed (%1)").arg(error_text));
      return;
    }

    const auto payload = ParseJsonObject(reply);
    reply->deleteLater();

    if (!payload.value(QStringLiteral("ok")).toBool()) {
      active_document_id_.clear();
      StopHeartbeat();
      const auto error_message =
          payload.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
      emit documentAccessInvalidated(
          document_id, QString{},
          error_message.isEmpty() ? QStringLiteral("Document: read-only, lock heartbeat rejected")
                                  : QStringLiteral("Document: %1").arg(error_message));
      return;
    }

    const auto result = EnvelopeResult(payload);
    const bool heartbeat_ok = result.value(QStringLiteral("heartbeat")).toBool();
    const auto owner = result.value(QStringLiteral("owner")).toString();
    if (!heartbeat_ok) {
      active_document_id_.clear();
      StopHeartbeat();
      emit documentAccessInvalidated(
          document_id, owner,
          owner.trimmed().isEmpty()
              ? QStringLiteral("Document: read-only, lock lost")
              : QStringLiteral("Document: read-only, locked by %1").arg(owner));
    }
  });
}

auto BackendClient::HealthUrl() const -> QString {
  return ApiUrl(QStringLiteral("/api/v1/health"));
}

auto BackendClient::ApiUrl(const QString& path) const -> QString {
  auto trimmed = base_url_;
  while (trimmed.endsWith('/')) {
    trimmed.chop(1);
  }
  return trimmed + path;
}

}  // namespace cppwiki::backend
