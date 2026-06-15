#ifndef CPPWIKI_SRC_APP_PROGRAM_SETTINGS_H_
#define CPPWIKI_SRC_APP_PROGRAM_SETTINGS_H_

#include <QString>

class QSettings;

namespace cppwiki {

class ProgramSettings final {
 public:
  ProgramSettings(QString application_name, QString application_version, QString organization_name,
                  QString app_data_directory, QString database_directory,
                  QString editor_dist_directory);

  [[nodiscard]] static auto FromDefaults() -> ProgramSettings;
  [[nodiscard]] static auto FromSettings(const QSettings& settings) -> ProgramSettings;

  [[nodiscard]] auto ApplicationName() const -> const QString&;
  [[nodiscard]] auto ApplicationVersion() const -> const QString&;
  [[nodiscard]] auto OrganizationName() const -> const QString&;
  [[nodiscard]] auto AppDataDirectory() const -> const QString&;
  [[nodiscard]] auto DatabaseDirectory() const -> const QString&;
  [[nodiscard]] auto EditorDistDirectory() const -> const QString&;

 private:
  QString application_name_;
  QString application_version_;
  QString organization_name_;
  QString app_data_directory_;
  QString database_directory_;
  QString editor_dist_directory_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_PROGRAM_SETTINGS_H_
