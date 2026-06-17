#ifndef CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
#define CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_

#include <QDialog>

#include "app/program_settings.h"

class QFormLayout;
class QSpinBox;

namespace oclero::qlementine {
class LineEdit;
class Switch;
}  // namespace oclero::qlementine

namespace cppwiki::gui {

class SettingsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(const ProgramSettings& settings, QWidget* parent = nullptr);

  [[nodiscard]] auto BuildProgramSettings() const -> ProgramSettings;

 private:
  ProgramSettings current_settings_;
  QFormLayout* form_layout_ = nullptr;
  QSpinBox* font_size_spinbox_ = nullptr;
  oclero::qlementine::Switch* theme_switch_ = nullptr;
  oclero::qlementine::LineEdit* database_directory_edit_ = nullptr;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
