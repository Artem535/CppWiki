#ifndef CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
#define CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_

#include <QDialog>

#include "app/program_settings.h"

class QFormLayout;
class QCheckBox;
class QSpinBox;

namespace oclero::qlementine {
class LineEdit;
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
  QCheckBox* backend_enabled_checkbox_ = nullptr;
  oclero::qlementine::LineEdit* backend_base_url_edit_ = nullptr;
  QCheckBox* auth_enabled_checkbox_ = nullptr;
  oclero::qlementine::LineEdit* auth_authorization_url_edit_ = nullptr;
  oclero::qlementine::LineEdit* auth_token_url_edit_ = nullptr;
  oclero::qlementine::LineEdit* auth_client_id_edit_ = nullptr;
  oclero::qlementine::LineEdit* auth_redirect_uri_edit_ = nullptr;
  QCheckBox* demo_collaboration_enabled_checkbox_ = nullptr;
  oclero::qlementine::LineEdit* demo_collaboration_user_id_edit_ = nullptr;
  QCheckBox* sync_enabled_checkbox_ = nullptr;
  oclero::qlementine::LineEdit* database_directory_edit_ = nullptr;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
