#include "gui/settings_dialog.h"

#include <oclero/qlementine/widgets/ActionButton.hpp>
#include <oclero/qlementine/widgets/Label.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <QAction>
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
  resize(640, 220);

  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(16, 16, 16, 16);
  root_layout->setSpacing(14);

  auto* title =
      new oclero::qlementine::Label(QStringLiteral("Application settings"),
                                    oclero::qlementine::TextRole::H2, this);
  root_layout->addWidget(title);

  auto* hint = new oclero::qlementine::Label(
      QStringLiteral("Adjust editor typography, theme, and open the local database folder."),
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

  auto* theme_row = new QWidget(this);
  auto* theme_layout = new QHBoxLayout(theme_row);
  theme_layout->setContentsMargins(0, 0, 0, 0);
  theme_layout->setSpacing(10);
  auto* theme_label =
      new oclero::qlementine::Label(QStringLiteral("Dark theme"), oclero::qlementine::TextRole::Default,
                                    theme_row);
  theme_switch_ = new oclero::qlementine::Switch(theme_row);
  theme_switch_->setChecked(current_settings_.ThemeModeValue() == ProgramSettings::ThemeMode::kDark);
  theme_layout->addWidget(theme_label);
  theme_layout->addWidget(theme_switch_);
  theme_layout->addStretch(1);
  form_layout_->addRow(QStringLiteral("Theme"), theme_row);

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
  return ProgramSettings(
      current_settings_.ApplicationName(), current_settings_.ApplicationVersion(),
      current_settings_.OrganizationName(), current_settings_.AppDataDirectory(),
      current_settings_.DatabaseDirectory(), current_settings_.EditorDistDirectory(),
      font_size_spinbox_->value(),
      theme_switch_->isChecked() ? ProgramSettings::ThemeMode::kDark
                                 : ProgramSettings::ThemeMode::kLight);
}

}  // namespace cppwiki::gui
