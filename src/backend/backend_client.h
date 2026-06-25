#ifndef CPPWIKI_SRC_BACKEND_BACKEND_CLIENT_H_
#define CPPWIKI_SRC_BACKEND_BACKEND_CLIENT_H_

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace cppwiki {
class ProgramSettings;
}

namespace cppwiki::backend {

enum class BackendConnectionState {
  kLocalOnly,
  kChecking,
  kReachable,
  kUnavailable,
};

class BackendClient final : public QObject {
  Q_OBJECT

 public:
  explicit BackendClient(QObject* parent = nullptr);

  void ApplySettings(const ProgramSettings& settings);
  void RefreshHealth();
  void SetAccessToken(QString access_token);

  [[nodiscard]] auto State() const -> BackendConnectionState;
  [[nodiscard]] auto BaseUrl() const -> const QString&;
  [[nodiscard]] auto StatusText() const -> const QString&;

 signals:
  void statusChanged(cppwiki::backend::BackendConnectionState state, const QString& status_text);

 private:
  void SetStatus(BackendConnectionState state, QString status_text);
  void AbortInFlightRequest();
  [[nodiscard]] auto HealthUrl() const -> QString;

  QNetworkAccessManager* network_manager_ = nullptr;
  QTimer* refresh_timer_ = nullptr;
  QNetworkReply* in_flight_reply_ = nullptr;
  QString access_token_;
  QString base_url_;
  QString status_text_ = QStringLiteral("Backend: local only");
  BackendConnectionState state_ = BackendConnectionState::kLocalOnly;
  bool enabled_ = false;
};

}  // namespace cppwiki::backend

#endif  // CPPWIKI_SRC_BACKEND_BACKEND_CLIENT_H_
