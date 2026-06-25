#include "backend/backend_client.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "app/program_settings.h"

namespace cppwiki::backend {

namespace {

constexpr int kHealthRefreshIntervalMs = 15000;
constexpr int kHealthRequestTimeoutMs = 2000;

}  // namespace

BackendClient::BackendClient(QObject* parent) : QObject(parent) {
  network_manager_ = new QNetworkAccessManager(this);
  refresh_timer_ = new QTimer(this);
  refresh_timer_->setInterval(kHealthRefreshIntervalMs);
  connect(refresh_timer_, &QTimer::timeout, this, &BackendClient::RefreshHealth);
}

void BackendClient::ApplySettings(const ProgramSettings& settings) {
  enabled_ = settings.BackendEnabled();
  base_url_ = settings.BackendBaseUrl().trimmed();

  if (!enabled_) {
    refresh_timer_->stop();
    AbortInFlightRequest();
    SetStatus(BackendConnectionState::kLocalOnly, QStringLiteral("Backend: local only"));
    return;
  }

  if (base_url_.isEmpty()) {
    refresh_timer_->stop();
    AbortInFlightRequest();
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

  QNetworkRequest request{QUrl{HealthUrl()}};
  request.setTransferTimeout(kHealthRequestTimeoutMs);
  in_flight_reply_ = network_manager_->get(request);

  connect(in_flight_reply_, &QNetworkReply::finished, this, [this]() {
    if (in_flight_reply_ == nullptr) {
      return;
    }

    auto* reply = in_flight_reply_;
    in_flight_reply_ = nullptr;
    const auto status_code =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = reply->error() == QNetworkReply::NoError && status_code >= 200 &&
                    status_code < 300;

    if (ok) {
      SetStatus(BackendConnectionState::kReachable,
                QStringLiteral("Backend: reachable at %1").arg(base_url_));
    } else {
      const auto error_text =
          reply->error() == QNetworkReply::NoError ? QStringLiteral("HTTP %1").arg(status_code)
                                                   : reply->errorString();
      SetStatus(BackendConnectionState::kUnavailable,
                QStringLiteral("Backend: unavailable (%1)").arg(error_text));
    }

    reply->deleteLater();
  });
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

auto BackendClient::HealthUrl() const -> QString {
  auto trimmed = base_url_;
  while (trimmed.endsWith('/')) {
    trimmed.chop(1);
  }
  return trimmed + QStringLiteral("/api/v1/health");
}

}  // namespace cppwiki::backend
