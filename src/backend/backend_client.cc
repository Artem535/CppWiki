#include "backend/backend_client.h"

#include <QJsonDocument>
#include <QJsonArray>
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
constexpr int kPresenceHeartbeatIntervalMs = 10000;
constexpr int kPresenceRequestTimeoutMs = 3000;

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

auto StringArray(const QJsonObject& object, const QString& key) -> QStringList {
  QStringList result;
  const auto values = object.value(key).toArray();
  result.reserve(values.size());
  for (const auto& value : values) {
    if (!value.isString()) {
      continue;
    }
    const auto text = value.toString().trimmed();
    if (!text.isEmpty()) {
      result.append(text);
    }
  }
  return result;
}

auto MakeSyncBootstrapStatus(QString text) -> QString {
  const auto trimmed = text.trimmed();
  if (trimmed.isEmpty()) {
    return QStringLiteral("Sync: bootstrap unavailable");
  }
  if (trimmed.startsWith(QStringLiteral("Sync:"), Qt::CaseInsensitive)) {
    return trimmed;
  }
  return QStringLiteral("Sync: %1").arg(trimmed);
}

auto MakeStatusOnlyBootstrap(QString status_text) -> sync::SyncBootstrap {
  sync::SyncBootstrap bootstrap;
  bootstrap.status_text = std::move(status_text);
  return bootstrap;
}

auto DecodeJwtObject(const QString& token) -> QJsonObject {
  const auto segments = token.split(QLatin1Char('.'));
  if (segments.size() != 3) {
    return {};
  }

  auto payload = segments.at(1).toUtf8();
  payload.replace('-', '+');
  payload.replace('_', '/');
  while (payload.size() % 4 != 0) {
    payload.append('=');
  }

  const auto payload_json = QByteArray::fromBase64(payload, QByteArray::Base64Encoding);
  const auto document = QJsonDocument::fromJson(payload_json);
  return document.isObject() ? document.object() : QJsonObject{};
}

auto FirstNonEmptyString(const QJsonObject& object,
                         std::initializer_list<const char*> keys) -> QString {
  for (const char* key : keys) {
    const auto value = object.value(QLatin1StringView(key));
    if (!value.isString()) {
      continue;
    }
    const auto text = value.toString().trimmed();
    if (!text.isEmpty()) {
      return text;
    }
  }
  return {};
}

}  // namespace

BackendClient::BackendClient(QObject* parent)
    : QObject(parent),
      network_manager_(new QNetworkAccessManager(this)),
      refresh_timer_(new QTimer(this)),
      heartbeat_timer_(new QTimer(this)),
      presence_timer_(new QTimer(this)) {
  refresh_timer_->setInterval(kHealthRefreshIntervalMs);
  connect(refresh_timer_, &QTimer::timeout, this, &BackendClient::RefreshHealth);

  heartbeat_timer_->setInterval(kLockHeartbeatIntervalMs);
  connect(heartbeat_timer_, &QTimer::timeout, this, &BackendClient::SendHeartbeat);

  presence_timer_->setInterval(kPresenceHeartbeatIntervalMs);
  connect(presence_timer_, &QTimer::timeout, this, &BackendClient::SendPresenceHeartbeat);
}

void BackendClient::ApplySettings(const ProgramSettings& settings) {
  enabled_ = settings.BackendEnabled();
  base_url_ = settings.BackendBaseUrl().trimmed();
  demo_collaboration_user_id_ = settings.DemoCollaborationEnabled()
                                    ? settings.DemoCollaborationUserId().trimmed()
                                    : QString{};

  if (!enabled_) {
    refresh_timer_->stop();
    AbortInFlightRequest();
    AbortSyncBootstrapRequest();
    CloseDocumentSession();
    StopPresence();
    SetSyncBootstrap(MakeStatusOnlyBootstrap(QStringLiteral("Sync: backend disabled")));
    SetStatus(BackendConnectionState::kLocalOnly, QStringLiteral("Backend: local only"));
    return;
  }

  if (base_url_.isEmpty()) {
    refresh_timer_->stop();
    AbortInFlightRequest();
    AbortSyncBootstrapRequest();
    CloseDocumentSession();
    StopPresence();
    SetSyncBootstrap(MakeStatusOnlyBootstrap(QStringLiteral("Sync: backend URL missing")));
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
      RefreshSyncBootstrap();
    } else {
      const auto error_text = ReplyErrorText(reply);
      SetSyncBootstrap(
          MakeStatusOnlyBootstrap(QStringLiteral("Sync: waiting for backend (%1)").arg(error_text)));
      SetStatus(BackendConnectionState::kUnavailable,
                QStringLiteral("Backend: unavailable (%1)").arg(error_text));
    }

    reply->deleteLater();
  });
}

void BackendClient::SetAccessToken(QString access_token) {
  access_token_ = std::move(access_token);
  RefreshSyncBootstrap();
}

void BackendClient::OpenDocumentViewSession(const QString& document_id,
                                            std::function<void(DocumentAccessState)> callback) {
  ++session_request_id_;
  const auto request_id = session_request_id_;

  if (document_id.trimmed().isEmpty()) {
    StopPresence();
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
    StopPresence();
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
    StopPresence();
    callback(DocumentAccessState{
        .editable = true,
        .local_only = true,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: backend unavailable, editing locally"),
    });
    return;
  }

  auto continue_with_view = [this, document_id, callback = std::move(callback), request_id]() mutable {
    if (request_id != session_request_id_) {
      return;
    }
    StartPresence(document_id, false);
    callback(DocumentAccessState{
        .editable = false,
        .local_only = false,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: viewing"),
    });
  };

  if (!active_document_id_.isEmpty()) {
    const auto document_id_to_release = active_document_id_;
    StopHeartbeat();
    active_document_id_.clear();
    ReleaseDocumentLock(document_id_to_release, std::move(continue_with_view));
    return;
  }

  continue_with_view();
}

void BackendClient::EnterDocumentEditSession(const QString& document_id,
                                             std::function<void(DocumentAccessState)> callback) {
  ++session_request_id_;
  const auto request_id = session_request_id_;

  if (document_id.trimmed().isEmpty()) {
    StopPresence();
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
    StopPresence();
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
    StopPresence();
    callback(DocumentAccessState{
        .editable = true,
        .local_only = true,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: backend unavailable, editing locally"),
    });
    return;
  }

  if (active_document_id_ == document_id && heartbeat_timer_->isActive()) {
    StartPresence(document_id, true);
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

void BackendClient::ExitDocumentEditSession(const QString& document_id,
                                            std::function<void(DocumentAccessState)> callback) {
  ++session_request_id_;
  const auto request_id = session_request_id_;

  if (document_id.trimmed().isEmpty()) {
    StopPresence();
    callback(DocumentAccessState{
        .editable = false,
        .local_only = false,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: viewing"),
    });
    return;
  }

  if (!enabled_ || state_ == BackendConnectionState::kLocalOnly) {
    callback(DocumentAccessState{
        .editable = true,
        .local_only = true,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: local-only editing"),
    });
    return;
  }

  auto continue_with_view = [this, document_id, callback = std::move(callback), request_id]() mutable {
    if (request_id != session_request_id_) {
      return;
    }
    StartPresence(document_id, false);
    callback(DocumentAccessState{
        .editable = false,
        .local_only = false,
        .lock_owner = {},
        .status_text = QStringLiteral("Document: viewing"),
    });
  };

  if (active_document_id_ == document_id) {
    StopHeartbeat();
    active_document_id_.clear();
    ReleaseDocumentLock(document_id, std::move(continue_with_view));
    return;
  }

  continue_with_view();
}

void BackendClient::CloseDocumentSession() {
  ++session_request_id_;
  StopHeartbeat();
  AbortSessionRequest();
  AbortHeartbeatRequest();
  StopPresence();

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

auto BackendClient::CurrentSyncBootstrap() const -> const sync::SyncBootstrap& {
  return sync_bootstrap_;
}

void BackendClient::SetSyncBootstrap(sync::SyncBootstrap bootstrap) {
  if (sync_bootstrap_.available == bootstrap.available &&
      sync_bootstrap_.enabled == bootstrap.enabled &&
      sync_bootstrap_.gateway_url == bootstrap.gateway_url &&
      sync_bootstrap_.database_name == bootstrap.database_name &&
      sync_bootstrap_.auth_mode == bootstrap.auth_mode &&
      sync_bootstrap_.token_passthrough == bootstrap.token_passthrough &&
      sync_bootstrap_.principal_subject == bootstrap.principal_subject &&
      sync_bootstrap_.principal_username == bootstrap.principal_username &&
      sync_bootstrap_.principal_email == bootstrap.principal_email &&
      sync_bootstrap_.principal_roles == bootstrap.principal_roles &&
      sync_bootstrap_.principal_groups == bootstrap.principal_groups &&
      sync_bootstrap_.channels == bootstrap.channels &&
      sync_bootstrap_.status_text == bootstrap.status_text) {
    return;
  }

  sync_bootstrap_ = std::move(bootstrap);
  emit syncBootstrapChanged();
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

void BackendClient::AbortPresenceRequest() {
  if (presence_reply_ == nullptr) {
    return;
  }

  disconnect(presence_reply_, nullptr, this, nullptr);
  presence_reply_->abort();
  presence_reply_->deleteLater();
  presence_reply_ = nullptr;
}

void BackendClient::AbortSyncBootstrapRequest() {
  if (sync_bootstrap_reply_ == nullptr) {
    return;
  }

  disconnect(sync_bootstrap_reply_, nullptr, this, nullptr);
  sync_bootstrap_reply_->abort();
  sync_bootstrap_reply_->deleteLater();
  sync_bootstrap_reply_ = nullptr;
}

void BackendClient::RefreshSyncBootstrap() {
  AbortSyncBootstrapRequest();

  if (!enabled_) {
    SetSyncBootstrap(MakeStatusOnlyBootstrap(QStringLiteral("Sync: backend disabled")));
    return;
  }

  if (state_ != BackendConnectionState::kReachable) {
    SetSyncBootstrap(MakeStatusOnlyBootstrap(QStringLiteral("Sync: waiting for backend")));
    return;
  }

  if (access_token_.trimmed().isEmpty()) {
    SetSyncBootstrap(
        MakeStatusOnlyBootstrap(QStringLiteral("Sync: waiting for authenticated session")));
    return;
  }

  const auto request =
      MakeRequest(QUrl{ApiUrl(QStringLiteral("/api/v1/sync/config"))}, kLockRequestTimeoutMs,
                  access_token_);
  sync_bootstrap_reply_ = network_manager_->get(request);

  connect(sync_bootstrap_reply_, &QNetworkReply::finished, this, [this]() {
    if (sync_bootstrap_reply_ == nullptr) {
      return;
    }

    auto* reply = sync_bootstrap_reply_;
    sync_bootstrap_reply_ = nullptr;

    const auto status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error_text = ReplyErrorText(reply);
    if (!error_text.isEmpty()) {
      reply->deleteLater();
      if (status_code == 401 || status_code == 403) {
        SetSyncBootstrap(MakeStatusOnlyBootstrap(
            QStringLiteral("Sync: authenticated session is not accepted by backend")));
      } else {
        SetSyncBootstrap(MakeStatusOnlyBootstrap(
            QStringLiteral("Sync: bootstrap request failed (%1)").arg(error_text)));
      }
      return;
    }

    const auto payload = ParseJsonObject(reply);
    reply->deleteLater();
    if (!payload.value(QStringLiteral("ok")).toBool()) {
      const auto error_message =
          payload.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
      SetSyncBootstrap(MakeStatusOnlyBootstrap(
          error_message.isEmpty() ? QStringLiteral("Sync: bootstrap rejected")
                                  : MakeSyncBootstrapStatus(error_message)));
      return;
    }

    const auto result = EnvelopeResult(payload);
    const auto auth = result.value(QStringLiteral("auth")).toObject();
    const auto principal = result.value(QStringLiteral("principal")).toObject();
    SetSyncBootstrap(sync::SyncBootstrap{
        .available = result.value(QStringLiteral("available")).toBool(),
        .enabled = result.value(QStringLiteral("enabled")).toBool(),
        .gateway_url = result.value(QStringLiteral("gatewayUrl")).toString(),
        .database_name = result.value(QStringLiteral("databaseName")).toString(),
        .auth_mode = auth.value(QStringLiteral("mode")).toString(),
        .token_passthrough = auth.value(QStringLiteral("tokenPassthrough")).toBool(),
        .principal_subject = principal.value(QStringLiteral("subject")).toString(),
        .principal_username = principal.value(QStringLiteral("preferredUsername")).toString(),
        .principal_email = principal.value(QStringLiteral("email")).toString(),
        .principal_roles = StringArray(principal, QStringLiteral("roles")),
        .principal_groups = StringArray(principal, QStringLiteral("groups")),
        .channels = StringArray(result, QStringLiteral("channels")),
        .status_text = result.value(QStringLiteral("statusText")).toString(),
    });
  });
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
  QJsonObject body;
  const auto owner = CurrentCollaborationUserId();
  if (!owner.isEmpty()) {
    body.insert(QStringLiteral("owner"), owner);
  }
  session_reply_ = network_manager_->sendCustomRequest(
      request, "DELETE", QJsonDocument(body).toJson(QJsonDocument::Compact));

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
  QJsonObject body;
  const auto owner = CurrentCollaborationUserId();
  if (!owner.isEmpty()) {
    body.insert(QStringLiteral("owner"), owner);
  }
  session_reply_ = network_manager_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

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
              StartPresence(document_id, true);
              callback(DocumentAccessState{
                  .editable = true,
                  .local_only = false,
                  .lock_owner = owner,
                  .status_text = QStringLiteral("Document: editing with backend lock"),
              });
              return;
            }

            StartPresence(document_id, false);
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

void BackendClient::StartPresence(const QString& document_id, bool editable) {
  if (document_id.trimmed().isEmpty() || state_ != BackendConnectionState::kReachable) {
    StopPresence();
    return;
  }

  presence_document_id_ = document_id;
  presence_scope_ = editable ? QStringLiteral("edit") : QStringLiteral("view");
  SendPresenceHeartbeat();
  if (!presence_timer_->isActive()) {
    presence_timer_->start();
  }
}

void BackendClient::StopPresence() {
  presence_timer_->stop();
  AbortPresenceRequest();
  presence_document_id_.clear();
  presence_scope_.clear();
  emit presenceUpdated(QString{}, false, {});
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
  QJsonObject body;
  const auto owner = CurrentCollaborationUserId();
  if (!owner.isEmpty()) {
    body.insert(QStringLiteral("owner"), owner);
  }
  heartbeat_reply_ = network_manager_->sendCustomRequest(
      request, "PUT", QJsonDocument(body).toJson(QJsonDocument::Compact));

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

void BackendClient::SendPresenceHeartbeat() {
  if (presence_document_id_.isEmpty() || state_ != BackendConnectionState::kReachable) {
    return;
  }

  AbortPresenceRequest();

  QJsonObject body;
  const auto user_id = CurrentCollaborationUserId();
  if (!user_id.isEmpty()) {
    body.insert(QStringLiteral("userId"), user_id);
  }
  if (!presence_scope_.trimmed().isEmpty()) {
    body.insert(QStringLiteral("scope"), presence_scope_);
  }

  const auto request =
      MakeRequest(QUrl{ApiUrl(QStringLiteral("/api/v1/presence/%1").arg(presence_document_id_))},
                  kPresenceRequestTimeoutMs, access_token_);
  presence_reply_ = network_manager_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

  connect(presence_reply_, &QNetworkReply::finished, this, [this]() {
    if (presence_reply_ == nullptr) {
      return;
    }

    auto* reply = presence_reply_;
    presence_reply_ = nullptr;
    const auto error_text = ReplyErrorText(reply);
    if (!error_text.isEmpty()) {
      reply->deleteLater();
      emit presenceUpdated(QString{}, false, {});
      return;
    }

    const auto payload = ParseJsonObject(reply);
    reply->deleteLater();
    HandlePresencePayload(payload);
  });
}

void BackendClient::HandlePresencePayload(const QJsonObject& payload) {
  if (!payload.value(QStringLiteral("ok")).toBool()) {
    emit presenceUpdated(QString{}, false, {});
    return;
  }

  const auto result = EnvelopeResult(payload);
  const auto users = result.value(QStringLiteral("users")).toArray();
  const auto self_user_id = CurrentPresenceUserId();

  QString editor_user_id;
  bool editor_is_self = false;
  QStringList viewer_user_ids;

  for (const auto& user_value : users) {
    const auto user = user_value.toObject();
    const auto user_id = user.value(QStringLiteral("userId")).toString().trimmed();
    const auto scope = user.value(QStringLiteral("scope")).toString().trimmed();
    if (user_id.isEmpty()) {
      continue;
    }

    const bool is_self = !self_user_id.isEmpty() && user_id == self_user_id;
    if (scope == QStringLiteral("edit")) {
      editor_user_id = user_id;
      editor_is_self = is_self;
      continue;
    }

    if (!is_self) {
      viewer_user_ids.push_back(user_id);
    }
  }

  emit presenceUpdated(editor_user_id, editor_is_self, viewer_user_ids);
}

auto BackendClient::CurrentPresenceUserId() const -> QString {
  if (!demo_collaboration_user_id_.isEmpty()) {
    return demo_collaboration_user_id_;
  }

  if (access_token_.trimmed().isEmpty()) {
    return {};
  }

  const auto payload = DecodeJwtObject(access_token_);
  return FirstNonEmptyString(payload, {"preferred_username", "email", "sub"});
}

auto BackendClient::CurrentCollaborationUserId() const -> QString {
  return CurrentPresenceUserId();
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
