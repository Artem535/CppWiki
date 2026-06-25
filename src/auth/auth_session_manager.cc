#include "auth/auth_session_manager.h"

#include <QDesktopServices>
#include <QAbstractOAuth>
#include <QAbstractOAuth2>
#include <QHostAddress>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QSet>
#include <QUrl>

#include "app/program_settings.h"
#include "auth/auth_token_store.h"
#include "core/constants.h"
#include "core/qt_string.h"

namespace cppwiki::auth {

AuthSessionManager::AuthSessionManager(QObject* parent) : QObject(parent) {
  token_store_ = new AuthTokenStore(ToQString(constants::kApplicationName), this);

  connect(token_store_, &AuthTokenStore::tokensLoaded, this,
          [this](const QString& access_token, const QString& refresh_token,
                 const QString& id_token) {
            if (!auth_enabled_ || oauth_flow_ == nullptr) {
              return;
            }

            if (access_token.trimmed().isEmpty()) {
              SetUiState(AuthSessionState::kError, QStringLiteral("Stored session is invalid"),
                         QStringLiteral("The restored auth session does not contain an access token."),
                         QStringLiteral("Sign in"), true, false);
              return;
            }

            oauth_flow_->setToken(access_token);
            oauth_flow_->setRefreshToken(refresh_token);
            id_token_ = id_token;
            emit accessTokenChanged(access_token);
            SetUiState(AuthSessionState::kAuthenticated, QStringLiteral("Signed in"),
                       QStringLiteral("Restored desktop auth session from the system keyring."),
                       QStringLiteral("Sign out"), false, true);
          });

  connect(token_store_, &AuthTokenStore::tokensMissing, this, [this]() {
    if (!auth_enabled_ || oauth_flow_ == nullptr) {
      return;
    }

    SetUiState(AuthSessionState::kSignedOut, QStringLiteral("Signed out"),
               QStringLiteral("Ready to start desktop browser login."),
               QStringLiteral("Sign in"), true, false);
  });

  connect(token_store_, &AuthTokenStore::storageError, this,
          [this](const QString& operation, const QString& message) {
            if (operation == QStringLiteral("save")) {
              if (state_ == AuthSessionState::kAuthenticated) {
                subtitle_ = QStringLiteral("Signed in, but keyring save failed: %1").arg(message);
                emit sessionChanged();
              }
              return;
            }

            if (operation == QStringLiteral("clear")) {
              return;
            }

            SetUiState(AuthSessionState::kError, QStringLiteral("Keyring error"),
                       QStringLiteral("Could not %1 the desktop auth session: %2")
                           .arg(operation, message),
                       QStringLiteral("Sign in"), true, false);
          });
}

void AuthSessionManager::ApplySettings(const ProgramSettings& settings) {
  auth_enabled_ = settings.AuthEnabled();
  authorization_url_ = settings.AuthAuthorizationUrl().trimmed();
  token_url_ = settings.AuthTokenUrl().trimmed();
  client_id_ = settings.AuthClientId().trimmed();
  redirect_uri_ = settings.AuthRedirectUri().trimmed();
  id_token_.clear();
  ResetOAuthArtifacts();
  emit accessTokenChanged(QString{});

  if (!auth_enabled_) {
    SetUiState(AuthSessionState::kDisabled, QStringLiteral("Auth disabled"),
               QStringLiteral("Enable auth in settings to use browser login."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!HasRequiredConfig()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Auth not configured"),
               QStringLiteral("Set authorization URL, token URL, client ID and redirect URI in settings."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!IsLoopbackRedirectUri()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Callback strategy not supported"),
               QStringLiteral("Current desktop spike implements only localhost callback handling."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  RebuildOAuthFlow();
  if (oauth_flow_ == nullptr || reply_handler_ == nullptr) {
    return;
  }

  SetUiState(AuthSessionState::kSignedOut, QStringLiteral("Signed out"),
             QStringLiteral("Ready to start desktop browser login. Stored session will be checked in the background."),
             QStringLiteral("Sign in"), true, false);
  token_store_->Load();
}

void AuthSessionManager::StartSignIn() {
  if (!auth_enabled_) {
    SetUiState(AuthSessionState::kDisabled, QStringLiteral("Auth disabled"),
               QStringLiteral("Enable auth in settings to use browser login."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!HasRequiredConfig()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Auth not configured"),
               QStringLiteral("Set authorization URL, token URL, client ID and redirect URI in settings."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!IsLoopbackRedirectUri()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Callback strategy not supported"),
               QStringLiteral("Current desktop spike implements only localhost callback handling."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (oauth_flow_ == nullptr || reply_handler_ == nullptr) {
    RebuildOAuthFlow();
  }

  if (oauth_flow_ == nullptr || reply_handler_ == nullptr) {
    return;
  }

  SetUiState(AuthSessionState::kAwaitingCallback, QStringLiteral("Browser login started"),
             QStringLiteral("Waiting for browser callback and token exchange."),
             QStringLiteral("Cancel"), false, true);
  oauth_flow_->grant();
}

void AuthSessionManager::SignOut() {
  ResetOAuthArtifacts();
  id_token_.clear();
  token_store_->Clear();
  emit accessTokenChanged(QString{});

  if (!auth_enabled_) {
    SetUiState(AuthSessionState::kDisabled, QStringLiteral("Auth disabled"),
               QStringLiteral("Enable auth in settings to use browser login."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!HasRequiredConfig()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Auth not configured"),
               QStringLiteral("Set authorization URL, token URL, client ID and redirect URI in settings."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!IsLoopbackRedirectUri()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Callback strategy not supported"),
               QStringLiteral("Current desktop spike implements only localhost callback handling."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  RebuildOAuthFlow();
  SetUiState(AuthSessionState::kSignedOut, QStringLiteral("Signed out"),
             QStringLiteral("Ready to start desktop browser login."),
             QStringLiteral("Sign in"), true, false);
}

auto AuthSessionManager::State() const -> AuthSessionState {
  return state_;
}

auto AuthSessionManager::Title() const -> const QString& {
  return title_;
}

auto AuthSessionManager::Subtitle() const -> const QString& {
  return subtitle_;
}

auto AuthSessionManager::ActionLabel() const -> const QString& {
  return action_label_;
}

auto AuthSessionManager::CanStartSignIn() const -> bool {
  return can_start_sign_in_;
}

auto AuthSessionManager::CanSignOut() const -> bool {
  return can_sign_out_;
}

void AuthSessionManager::SetUiState(AuthSessionState state, QString title, QString subtitle,
                                    QString action_label, bool can_start_sign_in,
                                    bool can_sign_out) {
  state_ = state;
  title_ = std::move(title);
  subtitle_ = std::move(subtitle);
  action_label_ = std::move(action_label);
  can_start_sign_in_ = can_start_sign_in;
  can_sign_out_ = can_sign_out;
  emit sessionChanged();
}

auto AuthSessionManager::HasRequiredConfig() const -> bool {
  return !authorization_url_.isEmpty() && !token_url_.isEmpty() && !client_id_.isEmpty() &&
         !redirect_uri_.isEmpty();
}

void AuthSessionManager::PersistCurrentTokens() {
  if (token_store_ == nullptr || oauth_flow_ == nullptr) {
    return;
  }

  const auto access_token = oauth_flow_->token().trimmed();
  if (access_token.isEmpty()) {
    return;
  }

  token_store_->Save(AuthTokenBundle{
      .access_token = access_token.toStdString(),
      .refresh_token = oauth_flow_->refreshToken().trimmed().toStdString(),
      .id_token = id_token_.trimmed().toStdString(),
  });
}

void AuthSessionManager::RebuildOAuthFlow() {
  ResetOAuthArtifacts();

  const QUrl redirect_url{redirect_uri_};
  const auto host = redirect_url.host().trimmed().toLower();
  const auto port = redirect_url.port();
  if (port <= 0 ||
      !(host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost"))) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Invalid redirect URI"),
               QStringLiteral("Loopback callback requires a localhost redirect URI with an explicit port."),
               QStringLiteral("Sign in"), true, false);
    return;
  }

  reply_handler_ = new QOAuthHttpServerReplyHandler(static_cast<quint16>(port), this);
  reply_handler_->setCallbackHost(redirect_url.host());
  reply_handler_->setCallbackPath(redirect_url.path());
  reply_handler_->setCallbackText(
      QStringLiteral("<html><body><h3>CppWiki</h3><p>Desktop login finished. You can close this window.</p></body></html>"));

  if (!reply_handler_->isListening()) {
    const auto error_message = QStringLiteral(
                                   "Could not bind %1:%2 for the desktop callback listener.")
                                   .arg(redirect_url.host())
                                   .arg(port);
    ResetOAuthArtifacts();
    SetUiState(AuthSessionState::kError, QStringLiteral("Failed to start callback listener"),
               error_message, QStringLiteral("Sign in"), true, false);
    return;
  }

  oauth_flow_ = new QOAuth2AuthorizationCodeFlow(this);
  oauth_flow_->setAuthorizationUrl(QUrl{authorization_url_});
  oauth_flow_->setTokenUrl(QUrl{token_url_});
  oauth_flow_->setClientIdentifier(client_id_);
  oauth_flow_->setReplyHandler(reply_handler_);
  oauth_flow_->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
  oauth_flow_->setRequestedScopeTokens(
      QSet<QByteArray>{QByteArrayLiteral("openid"), QByteArrayLiteral("profile"),
                       QByteArrayLiteral("email"), QByteArrayLiteral("offline_access")});

  connect(oauth_flow_, &QAbstractOAuth::authorizeWithBrowser, this, [this](const QUrl& url) {
    if (!QDesktopServices::openUrl(url)) {
      SetUiState(AuthSessionState::kError, QStringLiteral("Failed to open browser"),
                 QStringLiteral("System browser login could not be started."),
                 QStringLiteral("Try again"), true, false);
    }
  });

  connect(oauth_flow_, &QAbstractOAuth2::authorizationCallbackReceived, this,
          [this](const QVariantMap&) {
            SetUiState(AuthSessionState::kAwaitingCallback,
                       QStringLiteral("Authorization code received"),
                       QStringLiteral("Exchanging the callback code for access tokens."),
                       QStringLiteral("Cancel"), false, true);
          });

  connect(oauth_flow_, &QAbstractOAuth::granted, this, [this]() {
    id_token_ = oauth_flow_->idToken().trimmed();
    PersistCurrentTokens();
    emit accessTokenChanged(oauth_flow_->token().trimmed());
    SetUiState(AuthSessionState::kAuthenticated, QStringLiteral("Signed in"),
               QStringLiteral("Access token acquired through Qt NetworkAuth."),
               QStringLiteral("Sign out"), false, true);
  });

  connect(oauth_flow_, &QAbstractOAuth::tokenChanged, this, [this](const QString& access_token) {
    if (access_token.trimmed().isEmpty()) {
      return;
    }
    PersistCurrentTokens();
    emit accessTokenChanged(access_token.trimmed());
  });

  connect(oauth_flow_, &QAbstractOAuth2::refreshTokenChanged, this, [this](const QString&) {
    PersistCurrentTokens();
  });

  connect(oauth_flow_, &QAbstractOAuth2::idTokenChanged, this, [this](const QString& id_token) {
    id_token_ = id_token.trimmed();
    PersistCurrentTokens();
  });

  connect(oauth_flow_, &QAbstractOAuth::requestFailed, this,
          [this](QAbstractOAuth::Error error) {
            SetUiState(AuthSessionState::kError, QStringLiteral("Authentication failed"),
                       QStringLiteral("Qt OAuth request failed with error code %1.")
                           .arg(static_cast<int>(error)),
                       QStringLiteral("Sign in"), true, false);
          });

  connect(oauth_flow_, &QAbstractOAuth2::serverReportedErrorOccurred, this,
          [this](const QString& error, const QString& description, const QUrl&) {
            const auto details = description.trimmed().isEmpty()
                                     ? error.trimmed()
                                     : QStringLiteral("%1: %2").arg(error.trimmed(),
                                                                    description.trimmed());
            SetUiState(AuthSessionState::kError, QStringLiteral("Authentication failed"),
                       details.isEmpty() ? QStringLiteral("Identity provider reported an error.")
                                         : details,
                       QStringLiteral("Sign in"), true, false);
          });
}

void AuthSessionManager::ResetOAuthArtifacts() {
  if (oauth_flow_ != nullptr) {
    oauth_flow_->setToken(QString{});
    oauth_flow_->setRefreshToken(QString{});
    oauth_flow_->deleteLater();
    oauth_flow_ = nullptr;
  }

  if (reply_handler_ != nullptr) {
    reply_handler_->close();
    reply_handler_->deleteLater();
    reply_handler_ = nullptr;
  }
}

auto AuthSessionManager::IsLoopbackRedirectUri() const -> bool {
  const QUrl redirect_url{redirect_uri_};
  if (!redirect_url.isValid()) {
    return false;
  }

  const auto host = redirect_url.host().trimmed().toLower();
  return redirect_url.scheme() == QStringLiteral("http") &&
         (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost")) &&
         redirect_url.port() > 0;
}

}  // namespace cppwiki::auth
