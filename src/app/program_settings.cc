#include "app/program_settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <string_view>
#include <utility>

#include "core/constants.h"
#include "core/qt_string.h"

namespace cppwiki {

namespace {

auto DefaultAppDataDirectory() -> QString {
  auto directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (!directory.isEmpty()) {
    return QDir(directory).filePath(ToQString(constants::kApplicationName));
  }

  directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (!directory.isEmpty()) {
    return QDir(directory).filePath(ToQString(constants::kApplicationName));
  }

  return QDir(QDir::tempPath()).filePath(ToQString(constants::kApplicationName));
}

auto DefaultApplicationFontPointSize() -> int {
  return constants::kDefaultApplicationFontPointSize;
}

auto DefaultBackendBaseUrl() -> QString {
  return QStringLiteral("http://%1:%2")
      .arg(ToQString(constants::kDefaultServerBindHost))
      .arg(constants::kDefaultServerPort);
}

auto SettingsValueOrDefault(const QSettings& settings, std::string_view key,
                            const QString& fallback) -> QString {
  const auto value = settings.value(ToQString(key), fallback).toString();
  return value.isEmpty() ? fallback : value;
}

}  // namespace

ProgramSettings::ProgramSettings(
    QString application_name, QString application_version, QString organization_name,
    QString app_data_directory, QString database_directory, QString editor_dist_directory,
    QString backend_base_url, bool backend_enabled, QString auth_authorization_url,
    QString auth_token_url, QString auth_client_id, QString auth_redirect_uri, bool auth_enabled,
    bool demo_collaboration_enabled, QString demo_collaboration_user_id, bool sync_enabled,
    int application_font_point_size, bool ai_features_enabled, bool ai_autocomplete_enabled,
    bool ai_inline_suggestions_enabled, QString accent_color_key)
    : application_name_(std::move(application_name)),
      application_version_(std::move(application_version)),
      organization_name_(std::move(organization_name)),
      app_data_directory_(std::move(app_data_directory)),
      database_directory_(std::move(database_directory)),
      editor_dist_directory_(std::move(editor_dist_directory)),
      backend_base_url_(std::move(backend_base_url)),
      backend_enabled_(backend_enabled),
      auth_authorization_url_(std::move(auth_authorization_url)),
      auth_token_url_(std::move(auth_token_url)),
      auth_client_id_(std::move(auth_client_id)),
      auth_redirect_uri_(std::move(auth_redirect_uri)),
      auth_enabled_(auth_enabled),
      demo_collaboration_enabled_(demo_collaboration_enabled),
      demo_collaboration_user_id_(std::move(demo_collaboration_user_id)),
      sync_enabled_(sync_enabled),
      application_font_point_size_(application_font_point_size),
      ai_features_enabled_(ai_features_enabled),
      ai_autocomplete_enabled_(ai_autocomplete_enabled),
      ai_inline_suggestions_enabled_(ai_inline_suggestions_enabled),
      accent_color_key_(std::move(accent_color_key)) {}

auto ProgramSettings::FromDefaults() -> ProgramSettings {
  const QSettings settings;
  return FromSettings(settings);
}

auto ProgramSettings::FromSettings(const QSettings& settings) -> ProgramSettings {
  const auto default_app_data_directory = DefaultAppDataDirectory();

  const auto app_data_directory = SettingsValueOrDefault(
      settings, constants::kSettingsAppDataDirectoryKey, default_app_data_directory);

  const auto default_database_directory =
      QDir(app_data_directory).filePath(ToQString(constants::kDatabaseDirectoryName));

  const auto database_directory = SettingsValueOrDefault(
      settings, constants::kSettingsDatabaseDirectoryKey, default_database_directory);

  const auto editor_dist_directory =
      SettingsValueOrDefault(settings, constants::kSettingsEditorDistDirectoryKey,
                             QStringLiteral(CPPWIKI_EDITOR_DIST_DIR));

  const auto backend_base_url = SettingsValueOrDefault(
      settings, constants::kSettingsBackendBaseUrlKey, DefaultBackendBaseUrl());
  const auto backend_enabled =
      settings.value(ToQString(constants::kSettingsBackendEnabledKey), false).toBool();
  const auto auth_authorization_url =
      settings.value(ToQString(constants::kSettingsAuthAuthorizationUrlKey)).toString().trimmed();
  const auto auth_token_url =
      settings.value(ToQString(constants::kSettingsAuthTokenUrlKey)).toString().trimmed();
  const auto auth_client_id =
      settings.value(ToQString(constants::kSettingsAuthClientIdKey)).toString().trimmed();
  const auto auth_redirect_uri =
      SettingsValueOrDefault(settings, constants::kSettingsAuthRedirectUriKey,
                             ToQString(constants::kDefaultAuthRedirectUri));
  const auto auth_enabled =
      settings.value(ToQString(constants::kSettingsAuthEnabledKey), false).toBool();
  const auto demo_collaboration_enabled =
      settings.value(ToQString(constants::kSettingsDemoCollaborationEnabledKey), false).toBool();
  const auto demo_collaboration_user_id =
      settings.value(ToQString(constants::kSettingsDemoCollaborationUserIdKey))
          .toString()
          .trimmed();
  const auto sync_enabled =
      settings.value(ToQString(constants::kSettingsSyncEnabledKey), false).toBool();

  const auto application_font_point_size_value =
      settings
          .value(ToQString(constants::kSettingsApplicationFontPointSizeKey),
                 DefaultApplicationFontPointSize())
          .toInt();
  const auto application_font_point_size = application_font_point_size_value > 0
                                               ? application_font_point_size_value
                                               : DefaultApplicationFontPointSize();

  const auto ai_features_enabled =
      settings.value(ToQString(constants::kSettingsAiFeaturesEnabledKey), false).toBool();
  const auto ai_autocomplete_enabled =
      settings.value(ToQString(constants::kSettingsAiAutocompleteEnabledKey), false).toBool();
  const auto ai_inline_suggestions_enabled =
      settings.value(ToQString(constants::kSettingsAiInlineSuggestionsEnabledKey), false).toBool();

  const auto accent_color_key =
      SettingsValueOrDefault(settings, constants::kSettingsAccentColorKey,
                             ToQString(constants::kDefaultAccentColorKey));

  return ProgramSettings(
      ToQString(constants::kApplicationName), ToQString(constants::kApplicationVersion),
      ToQString(constants::kOrganizationName), app_data_directory, database_directory,
      editor_dist_directory, backend_base_url, backend_enabled, auth_authorization_url,
      auth_token_url, auth_client_id, auth_redirect_uri, auth_enabled, demo_collaboration_enabled,
      demo_collaboration_user_id, sync_enabled, application_font_point_size, ai_features_enabled,
      ai_autocomplete_enabled, ai_inline_suggestions_enabled, accent_color_key);
}

void ProgramSettings::SaveToSettings(QSettings& settings) const {
  settings.setValue(ToQString(constants::kSettingsAppDataDirectoryKey), app_data_directory_);
  settings.setValue(ToQString(constants::kSettingsDatabaseDirectoryKey), database_directory_);
  settings.setValue(ToQString(constants::kSettingsEditorDistDirectoryKey), editor_dist_directory_);
  settings.setValue(ToQString(constants::kSettingsBackendBaseUrlKey), backend_base_url_);
  settings.setValue(ToQString(constants::kSettingsBackendEnabledKey), backend_enabled_);
  settings.setValue(ToQString(constants::kSettingsAuthAuthorizationUrlKey),
                    auth_authorization_url_);
  settings.setValue(ToQString(constants::kSettingsAuthTokenUrlKey), auth_token_url_);
  settings.setValue(ToQString(constants::kSettingsAuthClientIdKey), auth_client_id_);
  settings.setValue(ToQString(constants::kSettingsAuthRedirectUriKey), auth_redirect_uri_);
  settings.setValue(ToQString(constants::kSettingsAuthEnabledKey), auth_enabled_);
  settings.setValue(ToQString(constants::kSettingsDemoCollaborationEnabledKey),
                    demo_collaboration_enabled_);
  settings.setValue(ToQString(constants::kSettingsDemoCollaborationUserIdKey),
                    demo_collaboration_user_id_);
  settings.setValue(ToQString(constants::kSettingsSyncEnabledKey), sync_enabled_);
  settings.setValue(ToQString(constants::kSettingsApplicationFontPointSizeKey),
                    application_font_point_size_);
  settings.setValue(ToQString(constants::kSettingsAiFeaturesEnabledKey), ai_features_enabled_);
  settings.setValue(ToQString(constants::kSettingsAiAutocompleteEnabledKey),
                    ai_autocomplete_enabled_);
  settings.setValue(ToQString(constants::kSettingsAiInlineSuggestionsEnabledKey),
                    ai_inline_suggestions_enabled_);
  settings.setValue(ToQString(constants::kSettingsAccentColorKey), accent_color_key_);
}

auto ProgramSettings::ApplicationName() const -> const QString& {
  return application_name_;
}

auto ProgramSettings::ApplicationVersion() const -> const QString& {
  return application_version_;
}

auto ProgramSettings::OrganizationName() const -> const QString& {
  return organization_name_;
}

auto ProgramSettings::AppDataDirectory() const -> const QString& {
  return app_data_directory_;
}

auto ProgramSettings::DatabaseDirectory() const -> const QString& {
  return database_directory_;
}

auto ProgramSettings::EditorDistDirectory() const -> const QString& {
  return editor_dist_directory_;
}

auto ProgramSettings::BackendBaseUrl() const -> const QString& {
  return backend_base_url_;
}

auto ProgramSettings::BackendEnabled() const -> bool {
  return backend_enabled_;
}

auto ProgramSettings::AuthAuthorizationUrl() const -> const QString& {
  return auth_authorization_url_;
}

auto ProgramSettings::AuthTokenUrl() const -> const QString& {
  return auth_token_url_;
}

auto ProgramSettings::AuthClientId() const -> const QString& {
  return auth_client_id_;
}

auto ProgramSettings::AuthRedirectUri() const -> const QString& {
  return auth_redirect_uri_;
}

auto ProgramSettings::AuthEnabled() const -> bool {
  return auth_enabled_;
}

auto ProgramSettings::DemoCollaborationEnabled() const -> bool {
  return demo_collaboration_enabled_;
}

auto ProgramSettings::DemoCollaborationUserId() const -> const QString& {
  return demo_collaboration_user_id_;
}

auto ProgramSettings::SyncEnabled() const -> bool {
  return sync_enabled_;
}

auto ProgramSettings::ApplicationFontPointSize() const -> int {
  return application_font_point_size_;
}

auto ProgramSettings::AiFeaturesEnabled() const -> bool {
  return ai_features_enabled_;
}

auto ProgramSettings::AiAutocompleteEnabled() const -> bool {
  return ai_autocomplete_enabled_;
}

auto ProgramSettings::AiInlineSuggestionsEnabled() const -> bool {
  return ai_inline_suggestions_enabled_;
}

auto ProgramSettings::AccentColorKey() const -> const QString& {
  return accent_color_key_;
}

}  // namespace cppwiki
