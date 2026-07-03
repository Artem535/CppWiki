#include "auth/auth_session_manager.h"

#include <QDesktopServices>
#include <QAbstractOAuth>
#include <QAbstractOAuth2>
#include <QByteArray>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QSet>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>

#include <limits>

#include "app/program_settings.h"
#include "auth/auth_token_store.h"
#include "core/constants.h"
#include "core/qt_string.h"

namespace cppwiki::auth {

namespace {

constexpr auto kRefreshLeadTime = std::chrono::seconds(60);
constexpr auto kMinimumWatchdogDelay = std::chrono::milliseconds(500);

auto DecodeJwtExpiration(const QString& access_token) -> QDateTime {
  const auto segments = access_token.split(QLatin1Char('.'));
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
  if (!document.isObject()) {
    return {};
  }

  const auto expiration = document.object().value(QStringLiteral("exp"));
  if (!expiration.isDouble()) {
    return {};
  }

  return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(expiration.toDouble()),
                                       QTimeZone::UTC);
}

auto FormatTokenExpirySubtitle(const QString& prefix, const QString& access_token) -> QString {
  const auto expiration = DecodeJwtExpiration(access_token);
  if (!expiration.isValid()) {
    return prefix;
  }

  return QStringLiteral("%1 Access token expires at %2.")
      .arg(prefix,
           expiration.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")));
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

auto FirstNonEmptyClaim(const QJsonObject& object,
                        std::initializer_list<const char*> keys) -> QString {
  for (const char* key : keys) {
    const auto value = object.value(QLatin1StringView(key));
    if (value.isString()) {
      const auto text = value.toString().trimmed();
      if (!text.isEmpty()) {
        return text;
      }
    }
  }
  return {};
}

auto MakeAvatarText(const QString& profile_name) -> QString {
  const auto trimmed = profile_name.trimmed();
  if (trimmed.isEmpty()) {
    return QStringLiteral("A");
  }
  return trimmed.left(1).toUpper();
}

}  // namespace

AuthSessionManager::AuthSessionManager(QObject* parent) : QObject(parent) {
  token_store_ = new AuthTokenStore(ToQString(constants::kApplicationName), this);
  token_watchdog_ = new QTimer(this);
  token_watchdog_->setSingleShot(true);
  connect(token_watchdog_, &QTimer::timeout, this,
          [this]() { HandleTokenWatchdogTimeout(); });

  connect(token_store_, &AuthTokenStore::tokensLoaded, this,
          [this](const QString& access_token, const QString& refresh_token,
                 const QString& id_token) {
            HandleStoredTokensLoaded(access_token, refresh_token, id_token);
          });

  connect(token_store_, &AuthTokenStore::tokensMissing, this, [this]() {
    if (!auth_enabled_ || oauth_flow_ == nullptr) {
      return;
    }

    ResetProfile();
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
  StopTokenWatchdog();
  emit accessTokenChanged(QString{});

  if (!auth_enabled_) {
    ResetProfile();
    SetUiState(AuthSessionState::kDisabled, QStringLiteral("Auth disabled"),
               QStringLiteral("Enable auth in settings to use browser login."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!HasRequiredConfig()) {
    ResetProfile();
    SetUiState(AuthSessionState::kError, QStringLiteral("Auth not configured"),
               QStringLiteral("Set authorization URL, token URL, client ID and redirect URI in settings."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!IsLoopbackRedirectUri()) {
    ResetProfile();
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
    ResetProfile();
    SetUiState(AuthSessionState::kDisabled, QStringLiteral("Auth disabled"),
               QStringLiteral("Enable auth in settings to use browser login."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!HasRequiredConfig()) {
    ResetProfile();
    SetUiState(AuthSessionState::kError, QStringLiteral("Auth not configured"),
               QStringLiteral("Set authorization URL, token URL, client ID and redirect URI in settings."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!IsLoopbackRedirectUri()) {
    ResetProfile();
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
  StopTokenWatchdog();
  id_token_.clear();
  ResetProfile();
  token_store_->Clear();
  emit accessTokenChanged(QString{});

  if (!auth_enabled_) {
    SetUiState(AuthSessionState::kDisabled, QStringLiteral("Auth disabled"),
               QStringLiteral("Enable auth in settings to use browser login."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!HasRequiredConfig()) {
    ResetProfile();
    SetUiState(AuthSessionState::kError, QStringLiteral("Auth not configured"),
               QStringLiteral("Set authorization URL, token URL, client ID and redirect URI in settings."),
               QStringLiteral("Sign in"), false, false);
    return;
  }

  if (!IsLoopbackRedirectUri()) {
    ResetProfile();
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

auto AuthSessionManager::ProfileName() const -> const QString& {
  return profile_name_;
}

auto AuthSessionManager::ProfileHint() const -> const QString& {
  return profile_hint_;
}

auto AuthSessionManager::ProfileAvatarText() const -> const QString& {
  return profile_avatar_text_;
}

void AuthSessionManager::HandleStoredTokensLoaded(const QString& access_token,
                                                  const QString& refresh_token,
                                                  const QString& id_token) {
  if (!auth_enabled_ || oauth_flow_ == nullptr) {
    return;
  }

  if (access_token.trimmed().isEmpty()) {
    ResetProfile();
    SetUiState(AuthSessionState::kError, QStringLiteral("Stored session is invalid"),
               QStringLiteral("The restored auth session does not contain an access token."),
               QStringLiteral("Sign in"), true, false);
    return;
  }

  oauth_flow_->setToken(access_token.trimmed());
  oauth_flow_->setRefreshToken(refresh_token.trimmed());
  id_token_ = id_token.trimmed();

  const auto expiration = DecodeJwtExpiration(access_token);
  if (expiration.isValid() && expiration <= QDateTime::currentDateTimeUtc()) {
    if (refresh_token.trimmed().isEmpty()) {
      HandleSessionRefreshFailure(
          QStringLiteral("Stored desktop session expired and no refresh token is available."));
      return;
    }

    SetUiState(AuthSessionState::kRefreshing, QStringLiteral("Refreshing session"),
               QStringLiteral("Stored desktop session expired. Requesting a fresh access token."),
               QStringLiteral("Sign out"), false, true);
    oauth_flow_->refreshTokens();
    return;
  }

  emit accessTokenChanged(access_token.trimmed());
  UpdateAuthenticatedUi(QStringLiteral("Restored desktop auth session from the system keyring."));
}

void AuthSessionManager::HandleSessionRefreshFailure(const QString& message) {
  StopTokenWatchdog();
  id_token_.clear();
  ResetProfile();
  token_store_->Clear();
  emit accessTokenChanged(QString{});

  if (!auth_enabled_ || !HasRequiredConfig() || !IsLoopbackRedirectUri()) {
    SetUiState(AuthSessionState::kError, QStringLiteral("Authentication failed"), message,
               QStringLiteral("Sign in"), true, false);
    return;
  }

  RebuildOAuthFlow();
  SetUiState(AuthSessionState::kSignedOut, QStringLiteral("Signed out"), message,
             QStringLiteral("Sign in"), true, false);
}

void AuthSessionManager::ScheduleTokenWatchdog() {
  if (token_watchdog_ == nullptr || oauth_flow_ == nullptr) {
    return;
  }

  token_watchdog_->stop();
  const auto expiration = DecodeJwtExpiration(oauth_flow_->token().trimmed());
  if (!expiration.isValid()) {
    return;
  }

  const auto refresh_at = expiration.addSecs(-std::chrono::duration_cast<std::chrono::seconds>(
                                                 kRefreshLeadTime)
                                                 .count());
  qint64 delay = QDateTime::currentDateTimeUtc().msecsTo(refresh_at);
  if (delay <= 0) {
    delay = static_cast<qint64>(kMinimumWatchdogDelay.count());
  }

  token_watchdog_->start(static_cast<int>(std::min<qint64>(
      delay, std::numeric_limits<int>::max())));
}

void AuthSessionManager::StopTokenWatchdog() {
  if (token_watchdog_ != nullptr) {
    token_watchdog_->stop();
  }
}

void AuthSessionManager::HandleTokenWatchdogTimeout() {
  if (oauth_flow_ == nullptr) {
    return;
  }

  const auto expiration = DecodeJwtExpiration(oauth_flow_->token().trimmed());
  if (!expiration.isValid()) {
    return;
  }

  if (oauth_flow_->refreshToken().trimmed().isEmpty()) {
    HandleSessionRefreshFailure(
        QStringLiteral("Desktop access token expired and no refresh token is available."));
    return;
  }

  if (expiration <= QDateTime::currentDateTimeUtc()) {
    SetUiState(AuthSessionState::kRefreshing, QStringLiteral("Refreshing session"),
               QStringLiteral("Access token expired. Requesting a fresh access token."),
               QStringLiteral("Sign out"), false, true);
  } else {
    SetUiState(AuthSessionState::kRefreshing, QStringLiteral("Refreshing session"),
               QStringLiteral("Access token is about to expire. Refreshing it in the background."),
               QStringLiteral("Sign out"), false, true);
  }
  oauth_flow_->refreshTokens();
}

void AuthSessionManager::ResetProfile() {
  profile_name_ = auth_enabled_ ? QStringLiteral("Signed out") : QStringLiteral("Auth unavailable");
  profile_hint_ = auth_enabled_ ? QStringLiteral("Desktop browser login is not active.")
                                : QStringLiteral("Auth session is not available.");
  profile_avatar_text_ = QStringLiteral("A");
}

void AuthSessionManager::UpdateProfileFromTokens() {
  if (oauth_flow_ == nullptr) {
    ResetProfile();
    return;
  }

  const auto id_payload = DecodeJwtObject(id_token_);
  const auto access_payload = DecodeJwtObject(oauth_flow_->token().trimmed());

  const auto preferred_name = FirstNonEmptyClaim(
      id_payload, {"preferred_username", "name", "email", "sub"});
  const auto fallback_name = FirstNonEmptyClaim(
      access_payload, {"preferred_username", "name", "email", "sub"});
  profile_name_ = !preferred_name.isEmpty() ? preferred_name : fallback_name;
  if (profile_name_.isEmpty()) {
    profile_name_ = QStringLiteral("Signed in");
  }

  const auto email = FirstNonEmptyClaim(id_payload, {"email"});
  const auto access_email = FirstNonEmptyClaim(access_payload, {"email"});
  const auto subject = FirstNonEmptyClaim(access_payload, {"sub"});

  if (!email.isEmpty()) {
    profile_hint_ = email;
  } else if (!access_email.isEmpty()) {
    profile_hint_ = access_email;
  } else if (!subject.isEmpty()) {
    profile_hint_ = QStringLiteral("sub: %1").arg(subject);
  } else {
    profile_hint_ = QStringLiteral("Authenticated desktop session");
  }

  profile_avatar_text_ = MakeAvatarText(profile_name_);
}

void AuthSessionManager::UpdateAuthenticatedUi(const QString& message_prefix) {
  if (oauth_flow_ == nullptr) {
    return;
  }

  UpdateProfileFromTokens();
  ScheduleTokenWatchdog();
  SetUiState(AuthSessionState::kAuthenticated, QStringLiteral("Signed in"),
             FormatTokenExpirySubtitle(message_prefix, oauth_flow_->token().trimmed()),
             QStringLiteral("Sign out"), false, true);
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
  oauth_flow_->setAutoRefresh(true);
  oauth_flow_->setRefreshLeadTime(kRefreshLeadTime);
  oauth_flow_->setRequestedScopeTokens(
      QSet<QByteArray>{QByteArrayLiteral("openid"), QByteArrayLiteral("profile"),
                       QByteArrayLiteral("email"), QByteArrayLiteral("groups"),
                       QByteArrayLiteral("roles"), QByteArrayLiteral("offline_access")});

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
    UpdateAuthenticatedUi(QStringLiteral("Access token acquired through Qt NetworkAuth."));
  });

  connect(oauth_flow_, &QAbstractOAuth::tokenChanged, this, [this](const QString& access_token) {
    if (access_token.trimmed().isEmpty()) {
      return;
    }
    PersistCurrentTokens();
    emit accessTokenChanged(access_token.trimmed());
    if (state_ == AuthSessionState::kRefreshing || state_ == AuthSessionState::kAuthenticated) {
      UpdateAuthenticatedUi(QStringLiteral("Desktop auth session is active."));
    }
  });

  connect(oauth_flow_, &QAbstractOAuth2::refreshTokenChanged, this, [this](const QString&) {
    PersistCurrentTokens();
  });

  connect(oauth_flow_, &QAbstractOAuth2::idTokenChanged, this, [this](const QString& id_token) {
    id_token_ = id_token.trimmed();
    PersistCurrentTokens();
  });

  connect(oauth_flow_, &QAbstractOAuth2::accessTokenAboutToExpire, this, [this]() {
    SetUiState(AuthSessionState::kRefreshing, QStringLiteral("Refreshing session"),
               QStringLiteral("Access token is about to expire. Refreshing it in the background."),
               QStringLiteral("Sign out"), false, true);
  });

  connect(oauth_flow_, &QAbstractOAuth2::expirationAtChanged, this, [this](const QDateTime&) {
    if (state_ == AuthSessionState::kAuthenticated) {
      UpdateAuthenticatedUi(QStringLiteral("Desktop auth session is active."));
    }
  });

  connect(oauth_flow_, &QAbstractOAuth::requestFailed, this,
          [this](QAbstractOAuth::Error error) {
            if (state_ == AuthSessionState::kRefreshing ||
                state_ == AuthSessionState::kAuthenticated) {
              HandleSessionRefreshFailure(
                  QStringLiteral("Stored desktop session expired and refresh failed with Qt OAuth error code %1.")
                      .arg(static_cast<int>(error)));
              return;
            }

            ResetProfile();
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
            if (state_ == AuthSessionState::kRefreshing ||
                state_ == AuthSessionState::kAuthenticated) {
              HandleSessionRefreshFailure(
                  details.isEmpty()
                      ? QStringLiteral("Stored desktop session expired and refresh failed.")
                      : QStringLiteral("Stored desktop session expired and refresh failed: %1")
                            .arg(details));
              return;
            }

            ResetProfile();
            SetUiState(AuthSessionState::kError, QStringLiteral("Authentication failed"),
                       details.isEmpty() ? QStringLiteral("Identity provider reported an error.")
                                         : details,
                       QStringLiteral("Sign in"), true, false);
          });
}

void AuthSessionManager::ResetOAuthArtifacts() {
  StopTokenWatchdog();
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
