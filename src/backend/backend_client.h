#ifndef CPPWIKI_SRC_BACKEND_BACKEND_CLIENT_H_
#define CPPWIKI_SRC_BACKEND_BACKEND_CLIENT_H_

#include <functional>
#include <QObject>
#include <QString>
#include <QStringList>

#include <cstdint>

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

struct DocumentAccessState final {
  bool editable = true;
  bool local_only = false;
  QString lock_owner;
  QString status_text;
};

class BackendClient final : public QObject {
  Q_OBJECT

 public:
  explicit BackendClient(QObject* parent = nullptr);

  void ApplySettings(const ProgramSettings& settings);
  void RefreshHealth();
  void SetAccessToken(QString access_token);
  void OpenDocumentViewSession(const QString& document_id,
                               std::function<void(DocumentAccessState)> callback);
  void EnterDocumentEditSession(const QString& document_id,
                                std::function<void(DocumentAccessState)> callback);
  void ExitDocumentEditSession(const QString& document_id,
                           std::function<void(DocumentAccessState)> callback);
  void CloseDocumentSession();

  [[nodiscard]] auto State() const -> BackendConnectionState;
  [[nodiscard]] auto BaseUrl() const -> const QString&;
  [[nodiscard]] auto StatusText() const -> const QString&;

signals:
  void statusChanged(cppwiki::backend::BackendConnectionState state, const QString& status_text);
  void documentAccessInvalidated(const QString& document_id, const QString& lock_owner,
                                 const QString& status_text);
  void presenceUpdated(const QString& editor_user_id, bool editor_is_self,
                       const QStringList& viewer_user_ids);

 private:
  void StartPresence(const QString& document_id, bool editable);
  void StopPresence();
  void SendPresenceHeartbeat();
  void HandlePresencePayload(const QJsonObject& payload);
  [[nodiscard]] auto CurrentPresenceUserId() const -> QString;
  [[nodiscard]] auto CurrentCollaborationUserId() const -> QString;
  void SetStatus(BackendConnectionState state, QString status_text);
  void AbortInFlightRequest();
  void AbortSessionRequest();
  void AbortHeartbeatRequest();
  void AbortPresenceRequest();
  void ReleaseDocumentLock(const QString& document_id, std::function<void()> continuation);
  void AcquireDocumentLock(const QString& document_id,
                           std::function<void(DocumentAccessState)> callback,
                           std::uint64_t request_id);
  void StartHeartbeat();
  void StopHeartbeat();
  void SendHeartbeat();
  [[nodiscard]] auto HealthUrl() const -> QString;
  [[nodiscard]] auto ApiUrl(const QString& path) const -> QString;

  QNetworkAccessManager* network_manager_ = nullptr;
  QTimer* refresh_timer_ = nullptr;
  QTimer* heartbeat_timer_ = nullptr;
  QTimer* presence_timer_ = nullptr;
  QNetworkReply* in_flight_reply_ = nullptr;
  QNetworkReply* session_reply_ = nullptr;
  QNetworkReply* heartbeat_reply_ = nullptr;
  QNetworkReply* presence_reply_ = nullptr;
  QString access_token_;
  QString base_url_;
  QString active_document_id_;
  QString demo_collaboration_user_id_;
  QString presence_document_id_;
  QString presence_scope_;
  QString status_text_ = QStringLiteral("Backend: local only");
  BackendConnectionState state_ = BackendConnectionState::kLocalOnly;
  std::uint64_t session_request_id_ = 0;
  bool enabled_ = false;
};

}  // namespace cppwiki::backend

#endif  // CPPWIKI_SRC_BACKEND_BACKEND_CLIENT_H_
