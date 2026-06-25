#include "gui/settings_dialog.h"

#include <oclero/qlementine/widgets/ActionButton.hpp>
#include <oclero/qlementine/widgets/Label.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

namespace cppwiki::gui {

namespace {

auto MakeReadOnlyPathLineEdit(const QString& value, QWidget* parent)
    -> oclero::qlementine::LineEdit* {
  auto* edit = new oclero::qlementine::LineEdit(parent);
  edit->setText(value);
  edit->setReadOnly(true);
  edit->setClearButtonEnabled(false);
  return edit;
}

}  // namespace

SettingsDialog::SettingsDialog(const ProgramSettings& settings, QWidget* parent)
    : QDialog(parent), current_settings_(settings) {
  setWindowTitle(QStringLiteral("Settings"));
  setModal(true);
  resize(640, 280);

  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(16, 16, 16, 16);
  root_layout->setSpacing(14);

  auto* title =
      new oclero::qlementine::Label(QStringLiteral("Application settings"),
                                    oclero::qlementine::TextRole::H2, this);
  root_layout->addWidget(title);

  auto* hint = new oclero::qlementine::Label(
      QStringLiteral("Adjust desktop appearance, inspect local storage and prepare optional backend connection settings."),
      oclero::qlementine::TextRole::Caption, this);
  hint->setWordWrap(true);
  root_layout->addWidget(hint);

  form_layout_ = new QFormLayout();
  form_layout_->setLabelAlignment(Qt::AlignLeft);
  form_layout_->setFormAlignment(Qt::AlignTop);
  form_layout_->setHorizontalSpacing(12);
  form_layout_->setVerticalSpacing(10);
  root_layout->addLayout(form_layout_);

  font_size_spinbox_ = new QSpinBox(this);
  font_size_spinbox_->setRange(8, 24);
  font_size_spinbox_->setSuffix(QStringLiteral(" pt"));
  font_size_spinbox_->setValue(current_settings_.ApplicationFontPointSize());
  form_layout_->addRow(QStringLiteral("Font size"), font_size_spinbox_);

  backend_enabled_checkbox_ = new QCheckBox(QStringLiteral("Use backend when available"), this);
  backend_enabled_checkbox_->setChecked(current_settings_.BackendEnabled());
  form_layout_->addRow(QStringLiteral("Backend"), backend_enabled_checkbox_);

  backend_base_url_edit_ = new oclero::qlementine::LineEdit(this);
  backend_base_url_edit_->setText(current_settings_.BackendBaseUrl());
  backend_base_url_edit_->setPlaceholderText(QStringLiteral("http://127.0.0.1:8080"));
  backend_base_url_edit_->setEnabled(current_settings_.BackendEnabled());
  connect(backend_enabled_checkbox_, &QCheckBox::toggled, backend_base_url_edit_,
          &QWidget::setEnabled);
  form_layout_->addRow(QStringLiteral("Backend URL"), backend_base_url_edit_);

  database_directory_edit_ = MakeReadOnlyPathLineEdit(current_settings_.DatabaseDirectory(), this);
  auto* open_folder_action =
      new QAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                  QStringLiteral("Open database folder"), this);
  auto* open_folder_button = new oclero::qlementine::ActionButton(this);
  open_folder_button->setAction(open_folder_action);
  open_folder_button->setMinimumWidth(160);
  connect(open_folder_action, &QAction::triggered, this, [this]() {
    const auto path = database_directory_edit_->text().trimmed();
    if (!path.isEmpty()) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
  });
  auto* folder_row = new QWidget(this);
  auto* folder_layout = new QHBoxLayout(folder_row);
  folder_layout->setContentsMargins(0, 0, 0, 0);
  folder_layout->setSpacing(8);
  folder_layout->addWidget(database_directory_edit_, 1);
  folder_layout->addWidget(open_folder_button, 0);
  form_layout_->addRow(QStringLiteral("Database folder"), folder_row);

  auto* buttons = new QDialogButtonBox(this);
  auto* cancel_button = buttons->addButton(QDialogButtonBox::Cancel);
  auto* save_button = buttons->addButton(QDialogButtonBox::Save);
  save_button->setDefault(true);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(save_button, &QPushButton::clicked, this, &QDialog::accept);
  root_layout->addWidget(buttons);
}

auto SettingsDialog::BuildProgramSettings() const -> ProgramSettings {
  const auto backend_base_url = backend_base_url_edit_->text().trimmed().isEmpty()
                                    ? current_settings_.BackendBaseUrl()
                                    : backend_base_url_edit_->text().trimmed();

  return ProgramSettings(
      current_settings_.ApplicationName(), current_settings_.ApplicationVersion(),
      current_settings_.OrganizationName(), current_settings_.AppDataDirectory(),
      current_settings_.DatabaseDirectory(), current_settings_.EditorDistDirectory(),
      backend_base_url, backend_enabled_checkbox_->isChecked(),
      font_size_spinbox_->value());
}

}  // namespace cppwiki::gui
