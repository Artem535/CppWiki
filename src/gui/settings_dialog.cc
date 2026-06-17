#include "gui/settings_dialog.h"

#include <oclero/qlementine/widgets/ActionButton.hpp>
#include <oclero/qlementine/widgets/Label.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

#include <QAction>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>

namespace cppwiki::gui {

namespace {

auto MakeDirectoryLineEdit(const QString& value, QWidget* parent)
    -> oclero::qlementine::LineEdit* {
  auto* edit = new oclero::qlementine::LineEdit(parent);
  edit->setText(value);
  edit->setClearButtonEnabled(true);
  edit->setPlaceholderText(QStringLiteral("/path/to/directory"));
  return edit;
}

}  // namespace

SettingsDialog::SettingsDialog(const ProgramSettings& settings, QWidget* parent)
    : QDialog(parent), current_settings_(settings) {
  setWindowTitle(QStringLiteral("Settings"));
  setModal(true);
  resize(720, 280);

  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(16, 16, 16, 16);
  root_layout->setSpacing(14);

  auto* title =
      new oclero::qlementine::Label(QStringLiteral("Application settings"),
                                    oclero::qlementine::TextRole::H2, this);
  root_layout->addWidget(title);

  auto* hint = new oclero::qlementine::Label(
      QStringLiteral("These paths are stored in QSettings and reloaded immediately after save."),
      oclero::qlementine::TextRole::Caption, this);
  hint->setWordWrap(true);
  root_layout->addWidget(hint);

  form_layout_ = new QFormLayout();
  form_layout_->setLabelAlignment(Qt::AlignLeft);
  form_layout_->setFormAlignment(Qt::AlignTop);
  form_layout_->setHorizontalSpacing(12);
  form_layout_->setVerticalSpacing(10);
  root_layout->addLayout(form_layout_);

  app_data_directory_edit_ = MakeDirectoryLineEdit(current_settings_.AppDataDirectory(), this);
  database_directory_edit_ = MakeDirectoryLineEdit(current_settings_.DatabaseDirectory(), this);
  editor_dist_directory_edit_ = MakeDirectoryLineEdit(current_settings_.EditorDistDirectory(), this);

  AddDirectoryRow(QStringLiteral("App data directory"), app_data_directory_edit_,
                  QStringLiteral("Select application data directory"));
  AddDirectoryRow(QStringLiteral("Database directory"), database_directory_edit_,
                  QStringLiteral("Select database directory"));
  AddDirectoryRow(QStringLiteral("Editor dist directory"), editor_dist_directory_edit_,
                  QStringLiteral("Select editor bundle directory"));

  auto* buttons = new QDialogButtonBox(this);
  auto* cancel_button = buttons->addButton(QDialogButtonBox::Cancel);
  auto* save_button = buttons->addButton(QDialogButtonBox::Save);
  save_button->setDefault(true);

  connect(cancel_button, &QPushButton::clicked, this, &QDialog::reject);
  connect(save_button, &QPushButton::clicked, this, &QDialog::accept);
  root_layout->addWidget(buttons);
}

auto SettingsDialog::BuildProgramSettings() const -> ProgramSettings {
  return ProgramSettings(current_settings_.ApplicationName(),
                         current_settings_.ApplicationVersion(),
                         current_settings_.OrganizationName(),
                         app_data_directory_edit_->text().trimmed(),
                         database_directory_edit_->text().trimmed(),
                         editor_dist_directory_edit_->text().trimmed());
}

auto SettingsDialog::BrowseForDirectory(QWidget* parent, const QString& title,
                                        const QString& current_path) -> QString {
  const auto selected = QFileDialog::getExistingDirectory(
      parent, title, current_path, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  return selected.isEmpty() ? current_path : selected;
}

void SettingsDialog::AddDirectoryRow(const QString& label, oclero::qlementine::LineEdit* edit,
                                     const QString& dialog_title) {
  auto* row = new QWidget(this);
  auto* row_layout = new QHBoxLayout(row);
  row_layout->setContentsMargins(0, 0, 0, 0);
  row_layout->setSpacing(8);

  row_layout->addWidget(edit, 1);

  auto* browse_action = new QAction(QIcon::fromTheme(QStringLiteral("folder-open")),
                                    QStringLiteral("Browse"), row);
  browse_action->setToolTip(QStringLiteral("Browse for directory"));

  auto* browse_button = new oclero::qlementine::ActionButton(row);
  browse_button->setAction(browse_action);
  browse_button->setMinimumWidth(100);
  row_layout->addWidget(browse_button, 0);

  connect(browse_action, &QAction::triggered, this, [this, edit, dialog_title]() {
    edit->setText(BrowseForDirectory(this, dialog_title, edit->text().trimmed()));
  });

  form_layout_->addRow(label, row);
}

}  // namespace cppwiki::gui
