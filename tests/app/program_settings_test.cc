#include "app/program_settings.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdlib>
#include <string_view>

#include "core/constants.h"
#include "core/qt_string.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto TestDefaultSettings() -> void {
  QTemporaryDir temporary_directory;
  Require(temporary_directory.isValid(), "temporary directory should be valid");

  const auto settings_path = temporary_directory.filePath(QStringLiteral("cppwiki-defaults.ini"));
  QSettings settings_file(settings_path, QSettings::IniFormat);
  const auto settings = cppwiki::ProgramSettings::FromSettings(settings_file);

  Require(settings.ApplicationName() == cppwiki::ToQString(cppwiki::constants::kApplicationName),
          "application name should come from constants");
  Require(
      settings.ApplicationVersion() == cppwiki::ToQString(cppwiki::constants::kApplicationVersion),
      "application version should come from constants");
  Require(settings.OrganizationName() == cppwiki::ToQString(cppwiki::constants::kOrganizationName),
          "organization name should come from constants");
  Require(!settings.AppDataDirectory().isEmpty(), "app data directory should not be empty");
  Require(settings.DatabaseDirectory().endsWith(
              cppwiki::ToQString(cppwiki::constants::kDatabaseDirectoryName)),
          "database directory should end with configured directory name");
  Require(settings.EditorDistDirectory().endsWith(QStringLiteral("frontend/editor/dist")),
          "editor dist directory should point to frontend/editor/dist");
  Require(settings.BackendBaseUrl() == QStringLiteral("http://127.0.0.1:8080"),
          "backend url should default to the local server endpoint");
  Require(!settings.BackendEnabled(), "backend should be disabled by default");
  Require(settings.AuthAuthorizationUrl().isEmpty(),
          "auth authorization url should be empty by default");
  Require(settings.AuthTokenUrl().isEmpty(), "auth token url should be empty by default");
  Require(settings.AuthClientId().isEmpty(), "auth client id should be empty by default");
  Require(settings.AuthRedirectUri() == QStringLiteral("http://127.0.0.1:38080/auth/callback"),
          "auth redirect uri should default to the localhost callback");
  Require(!settings.AuthEnabled(), "auth should be disabled by default");
  Require(!settings.SyncEnabled(), "sync should be disabled by default");
  Require(settings.ApplicationFontPointSize() > 0, "font size should be positive");
}

auto TestSettingsOverrides() -> void {
  QTemporaryDir temporary_directory;
  Require(temporary_directory.isValid(), "temporary directory should be valid");

  const auto settings_path = temporary_directory.filePath(QStringLiteral("cppwiki-test.ini"));
  QSettings settings(settings_path, QSettings::IniFormat);

  const auto app_data_directory = temporary_directory.filePath(QStringLiteral("app-data"));
  const auto database_directory = temporary_directory.filePath(QStringLiteral("database-override"));
  const auto editor_dist_directory = temporary_directory.filePath(QStringLiteral("editor-dist"));
  const auto backend_base_url = QStringLiteral("http://localhost:9000");
  const auto auth_authorization_url =
      QStringLiteral("https://auth.example/application/o/authorize/");
  const auto auth_token_url = QStringLiteral("https://auth.example/application/o/token/");
  const auto auth_client_id = QStringLiteral("cppwiki-desktop");
  const auto auth_redirect_uri = QStringLiteral("http://127.0.0.1:38080/auth/callback");

  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAppDataDirectoryKey),
                    app_data_directory);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsDatabaseDirectoryKey),
                    database_directory);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsEditorDistDirectoryKey),
                    editor_dist_directory);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsBackendBaseUrlKey),
                    backend_base_url);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsBackendEnabledKey), true);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAuthAuthorizationUrlKey),
                    auth_authorization_url);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAuthTokenUrlKey),
                    auth_token_url);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAuthClientIdKey),
                    auth_client_id);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAuthRedirectUriKey),
                    auth_redirect_uri);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAuthEnabledKey), true);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsSyncEnabledKey), true);
  settings.setValue(
      cppwiki::ToQString(cppwiki::constants::kSettingsApplicationFontPointSizeKey), 15);

  const auto program_settings = cppwiki::ProgramSettings::FromSettings(settings);

  Require(program_settings.AppDataDirectory() == app_data_directory,
          "app data directory should be read from QSettings");
  Require(program_settings.DatabaseDirectory() == database_directory,
          "database directory should be read from QSettings");
  Require(program_settings.EditorDistDirectory() == editor_dist_directory,
          "editor dist directory should be read from QSettings");
  Require(program_settings.BackendBaseUrl() == backend_base_url,
          "backend base url should be read from QSettings");
  Require(program_settings.BackendEnabled(), "backend enabled flag should be read from QSettings");
  Require(program_settings.AuthAuthorizationUrl() == auth_authorization_url,
          "auth authorization url should be read from QSettings");
  Require(program_settings.AuthTokenUrl() == auth_token_url,
          "auth token url should be read from QSettings");
  Require(program_settings.AuthClientId() == auth_client_id,
          "auth client id should be read from QSettings");
  Require(program_settings.AuthRedirectUri() == auth_redirect_uri,
          "auth redirect uri should be read from QSettings");
  Require(program_settings.AuthEnabled(), "auth enabled flag should be read from QSettings");
  Require(program_settings.SyncEnabled(), "sync enabled flag should be read from QSettings");
  Require(program_settings.ApplicationFontPointSize() == 15,
          "font size should be read from QSettings");
}

auto TestSettingsRoundTrip() -> void {
  QTemporaryDir temporary_directory;
  Require(temporary_directory.isValid(), "temporary directory should be valid");

  const auto settings_path = temporary_directory.filePath(QStringLiteral("cppwiki-roundtrip.ini"));
  QSettings settings(settings_path, QSettings::IniFormat);

  const auto app_data_directory = temporary_directory.filePath(QStringLiteral("app-data-roundtrip"));
  const auto database_directory = temporary_directory.filePath(QStringLiteral("database-roundtrip"));
  const auto editor_dist_directory =
      temporary_directory.filePath(QStringLiteral("editor-dist-roundtrip"));
  const auto backend_base_url = QStringLiteral("https://cppwiki.internal:9443");
  const auto auth_authorization_url =
      QStringLiteral("https://auth.internal/application/o/authorize/");
  const auto auth_token_url = QStringLiteral("https://auth.internal/application/o/token/");
  const auto auth_client_id = QStringLiteral("cppwiki-desktop");
  const auto auth_redirect_uri = QStringLiteral("http://127.0.0.1:38080/auth/callback");

  const cppwiki::ProgramSettings program_settings(
      cppwiki::ToQString(cppwiki::constants::kApplicationName),
      cppwiki::ToQString(cppwiki::constants::kApplicationVersion),
      cppwiki::ToQString(cppwiki::constants::kOrganizationName), app_data_directory,
      database_directory, editor_dist_directory, backend_base_url, true,
      auth_authorization_url, auth_token_url, auth_client_id, auth_redirect_uri, true, false,
      QString(), true, 15);
  program_settings.SaveToSettings(settings);
  settings.sync();

  const auto reloaded = cppwiki::ProgramSettings::FromSettings(settings);
  Require(reloaded.AppDataDirectory() == app_data_directory,
          "saved app data directory should round-trip through QSettings");
  Require(reloaded.DatabaseDirectory() == database_directory,
          "saved database directory should round-trip through QSettings");
  Require(reloaded.EditorDistDirectory() == editor_dist_directory,
          "saved editor dist directory should round-trip through QSettings");
  Require(reloaded.BackendBaseUrl() == backend_base_url,
          "saved backend base url should round-trip through QSettings");
  Require(reloaded.BackendEnabled(), "saved backend enabled flag should round-trip through QSettings");
  Require(reloaded.AuthAuthorizationUrl() == auth_authorization_url,
          "saved auth authorization url should round-trip through QSettings");
  Require(reloaded.AuthTokenUrl() == auth_token_url,
          "saved auth token url should round-trip through QSettings");
  Require(reloaded.AuthClientId() == auth_client_id,
          "saved auth client id should round-trip through QSettings");
  Require(reloaded.AuthRedirectUri() == auth_redirect_uri,
          "saved auth redirect uri should round-trip through QSettings");
  Require(reloaded.AuthEnabled(), "saved auth enabled flag should round-trip through QSettings");
  Require(reloaded.SyncEnabled(), "saved sync enabled flag should round-trip through QSettings");
  Require(reloaded.ApplicationFontPointSize() == 15,
          "saved font size should round-trip through QSettings");
}

}  // namespace

auto main(int argc, char* argv[]) -> int {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(cppwiki::ToQString(cppwiki::constants::kApplicationName));
  QCoreApplication::setApplicationVersion(
      cppwiki::ToQString(cppwiki::constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(cppwiki::ToQString(cppwiki::constants::kOrganizationName));

  TestDefaultSettings();
  TestSettingsOverrides();
  TestSettingsRoundTrip();

  spdlog::info("cppwiki_program_settings_tests passed");
  return EXIT_SUCCESS;
}
