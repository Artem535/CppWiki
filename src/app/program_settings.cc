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

ProgramSettings::ProgramSettings(QString application_name, QString application_version,
                                 QString organization_name, QString app_data_directory,
                                 QString database_directory, QString editor_dist_directory,
                                 QString backend_base_url, bool backend_enabled,
                                 int application_font_point_size)
    : application_name_(std::move(application_name)),
      application_version_(std::move(application_version)),
      organization_name_(std::move(organization_name)),
      app_data_directory_(std::move(app_data_directory)),
      database_directory_(std::move(database_directory)),
      editor_dist_directory_(std::move(editor_dist_directory)),
      backend_base_url_(std::move(backend_base_url)),
      backend_enabled_(backend_enabled),
      application_font_point_size_(application_font_point_size) {}

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

  const auto application_font_point_size_value =
      settings.value(ToQString(constants::kSettingsApplicationFontPointSizeKey),
                     DefaultApplicationFontPointSize())
          .toInt();
  const auto application_font_point_size =
      application_font_point_size_value > 0 ? application_font_point_size_value
                                            : DefaultApplicationFontPointSize();

  return ProgramSettings(ToQString(constants::kApplicationName),
                         ToQString(constants::kApplicationVersion),
                         ToQString(constants::kOrganizationName), app_data_directory,
                         database_directory, editor_dist_directory, backend_base_url,
                         backend_enabled,
                         application_font_point_size);
}

void ProgramSettings::SaveToSettings(QSettings& settings) const {
  settings.setValue(ToQString(constants::kSettingsAppDataDirectoryKey), app_data_directory_);
  settings.setValue(ToQString(constants::kSettingsDatabaseDirectoryKey), database_directory_);
  settings.setValue(ToQString(constants::kSettingsEditorDistDirectoryKey), editor_dist_directory_);
  settings.setValue(ToQString(constants::kSettingsBackendBaseUrlKey), backend_base_url_);
  settings.setValue(ToQString(constants::kSettingsBackendEnabledKey), backend_enabled_);
  settings.setValue(ToQString(constants::kSettingsApplicationFontPointSizeKey),
                    application_font_point_size_);
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

auto ProgramSettings::ApplicationFontPointSize() const -> int {
  return application_font_point_size_;
}

}  // namespace cppwiki
