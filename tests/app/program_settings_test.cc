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
  const auto settings = cppwiki::ProgramSettings::FromDefaults();

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
}

auto TestSettingsOverrides() -> void {
  QTemporaryDir temporary_directory;
  Require(temporary_directory.isValid(), "temporary directory should be valid");

  const auto settings_path = temporary_directory.filePath(QStringLiteral("cppwiki-test.ini"));
  QSettings settings(settings_path, QSettings::IniFormat);

  const auto app_data_directory = temporary_directory.filePath(QStringLiteral("app-data"));
  const auto database_directory = temporary_directory.filePath(QStringLiteral("database-override"));
  const auto editor_dist_directory = temporary_directory.filePath(QStringLiteral("editor-dist"));

  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsAppDataDirectoryKey),
                    app_data_directory);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsDatabaseDirectoryKey),
                    database_directory);
  settings.setValue(cppwiki::ToQString(cppwiki::constants::kSettingsEditorDistDirectoryKey),
                    editor_dist_directory);

  const auto program_settings = cppwiki::ProgramSettings::FromSettings(settings);

  Require(program_settings.AppDataDirectory() == app_data_directory,
          "app data directory should be read from QSettings");
  Require(program_settings.DatabaseDirectory() == database_directory,
          "database directory should be read from QSettings");
  Require(program_settings.EditorDistDirectory() == editor_dist_directory,
          "editor dist directory should be read from QSettings");
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

  spdlog::info("cppwiki_program_settings_tests passed");
  return EXIT_SUCCESS;
}
