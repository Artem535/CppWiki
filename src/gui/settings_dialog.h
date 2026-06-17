#ifndef CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
#define CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_

#include <QDialog>

#include "app/program_settings.h"

class QFormLayout;

namespace oclero::qlementine {
class ActionButton;
class Label;
class LineEdit;
}  // namespace oclero::qlementine

namespace cppwiki::gui {

class SettingsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(const ProgramSettings& settings, QWidget* parent = nullptr);

  [[nodiscard]] auto BuildProgramSettings() const -> ProgramSettings;

 private:
  [[nodiscard]] static auto BrowseForDirectory(QWidget* parent, const QString& title,
                                               const QString& current_path) -> QString;
  void AddDirectoryRow(const QString& label, oclero::qlementine::LineEdit* edit,
                       const QString& dialog_title);

  ProgramSettings current_settings_;
  QFormLayout* form_layout_ = nullptr;
  oclero::qlementine::LineEdit* app_data_directory_edit_ = nullptr;
  oclero::qlementine::LineEdit* database_directory_edit_ = nullptr;
  oclero::qlementine::LineEdit* editor_dist_directory_edit_ = nullptr;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
