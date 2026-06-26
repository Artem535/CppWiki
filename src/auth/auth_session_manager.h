#ifndef CPPWIKI_SRC_AUTH_AUTH_SESSION_MANAGER_H_
#define CPPWIKI_SRC_AUTH_AUTH_SESSION_MANAGER_H_

#include <QObject>
#include <QString>

class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;

namespace cppwiki {
class ProgramSettings;
}

namespace cppwiki::auth {

class AuthTokenStore;

enum class AuthSessionState {
  kDisabled,
  kSignedOut,
  kAwaitingCallback,
  kRefreshing,
  kAuthenticated,
  kError,
};

class AuthSessionManager final : public QObject {
  Q_OBJECT

 public:
  explicit AuthSessionManager(QObject* parent = nullptr);

  void ApplySettings(const ProgramSettings& settings);
  void StartSignIn();
  void SignOut();

  [[nodiscard]] auto State() const -> AuthSessionState;
  [[nodiscard]] auto Title() const -> const QString&;
  [[nodiscard]] auto Subtitle() const -> const QString&;
  [[nodiscard]] auto ActionLabel() const -> const QString&;
  [[nodiscard]] auto CanStartSignIn() const -> bool;
  [[nodiscard]] auto CanSignOut() const -> bool;

 signals:
  void sessionChanged();
  void accessTokenChanged(const QString& access_token);

 private:
  void HandleStoredTokensLoaded(const QString& access_token, const QString& refresh_token,
                                const QString& id_token);
  void HandleSessionRefreshFailure(const QString& message);
  void UpdateAuthenticatedUi(const QString& message_prefix = QStringLiteral("Signed in."));
  void PersistCurrentTokens();
  void RebuildOAuthFlow();
  void ResetOAuthArtifacts();
  void SetUiState(AuthSessionState state, QString title, QString subtitle, QString action_label,
                  bool can_start_sign_in, bool can_sign_out);
  [[nodiscard]] auto HasRequiredConfig() const -> bool;
  [[nodiscard]] auto IsLoopbackRedirectUri() const -> bool;

  QString authorization_url_;
  QString token_url_;
  QString client_id_;
  QString redirect_uri_;
  QString id_token_;
  QString title_ = QStringLiteral("Auth disabled");
  QString subtitle_ = QStringLiteral("Enable auth in settings to use browser login.");
  QString action_label_ = QStringLiteral("Sign in");
  AuthTokenStore* token_store_ = nullptr;
  QOAuth2AuthorizationCodeFlow* oauth_flow_ = nullptr;
  QOAuthHttpServerReplyHandler* reply_handler_ = nullptr;
  AuthSessionState state_ = AuthSessionState::kDisabled;
  bool auth_enabled_ = false;
  bool can_start_sign_in_ = false;
  bool can_sign_out_ = false;
};

}  // namespace cppwiki::auth

#endif  // CPPWIKI_SRC_AUTH_AUTH_SESSION_MANAGER_H_
