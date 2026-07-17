#ifndef CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
#define CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_

#include <QDialog>
#include <memory>

#include "app/accent_color.h"
#include "app/program_settings.h"

class QFormLayout;
class QCheckBox;
class QLabel;
class QSpinBox;
class QStackedWidget;
class QButtonGroup;

namespace oclero::qlementine {
class LineEdit;
class SegmentedControl;
}  // namespace oclero::qlementine

namespace cppwiki::auth {
class AiApiKeyStore;
}

namespace cppwiki::gui {

class SettingsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(const ProgramSettings& settings, QWidget* parent = nullptr);
  ~SettingsDialog() override;

  [[nodiscard]] auto BuildProgramSettings() const -> ProgramSettings;

 private:
  ProgramSettings current_settings_;
  oclero::qlementine::SegmentedControl* section_control_ = nullptr;
  QStackedWidget* section_stack_ = nullptr;
  QSpinBox* font_size_spinbox_ = nullptr;
  // ADR-016 accent-color swatch row (General page): one exclusively-checkable round swatch
  // button per AccentColor preset, keyed by ToAccentColorKey() in the button group's ID.
  QButtonGroup* accent_color_group_ = nullptr;
  QString selected_accent_color_key_;
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
  QCheckBox* ai_features_enabled_checkbox_ = nullptr;
  QCheckBox* ai_autocomplete_enabled_checkbox_ = nullptr;
  // Local-key fallback (ADR-012 addendum): only used/shown when no backend
  // is configured. The key itself is never held in ProgramSettings/QSettings
  // — it's read from and written to the OS keychain via ai_api_key_store_.
  oclero::qlementine::LineEdit* ai_local_api_key_edit_ = nullptr;
  QLabel* ai_local_api_key_hint_ = nullptr;
  std::unique_ptr<auth::AiApiKeyStore> ai_api_key_store_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_SETTINGS_DIALOG_H_
