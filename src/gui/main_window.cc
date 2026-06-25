#include "gui/main_window.h"

#include <QAction>
#include <QDialog>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QString>
#include <QWidget>
#include <QSettings>

#include "app/app_context.h"
#include "app/program_settings.h"
#include "core/constants.h"
#include "gui/i_page.h"
#include "gui/settings_dialog.h"
#include "gui/page.h"

namespace cppwiki {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  BuildUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::SetContext(AppContext* context) {
  context_ = context;
  CreateInitialPage();
  UpdateBackendStatus();
}

void MainWindow::CreateInitialPage() {
  if (context_ == nullptr) {
    setCentralWidget(nullptr);
    setWindowTitle(QStringLiteral("CppWiki"));
    return;
  }

  // Create the main page with the application context
  auto* page = new Page(*context_, this);
  connect(page, &Page::settingsRequested, this, [this]() { ShowSettingsDialog(); });
  current_page_ = page;

  setCentralWidget(current_page_->Widget());
  setWindowTitle(QStringLiteral("CppWiki - %1").arg(current_page_->Title()));
}

void MainWindow::ShowSettingsDialog() {
  if (context_ == nullptr) {
    return;
  }

  gui::SettingsDialog dialog(context_->settings, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto updated_settings = dialog.BuildProgramSettings();
  QSettings settings;
  updated_settings.SaveToSettings(settings);
  settings.sync();

  context_->settings = updated_settings;
  UpdateBackendStatus();
  statusBar()->showMessage(QStringLiteral("Settings saved."), 3000);
  emit settingsChanged();
}

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("CppWiki"));
  resize(constants::kInitialWindowWidth, constants::kInitialWindowHeight);
  statusBar()->showMessage(QStringLiteral("Ready"));
  backend_status_label_ = new QLabel(QStringLiteral("Backend: local only"), this);
  statusBar()->addPermanentWidget(backend_status_label_);
  menuBar()->hide();
}

void MainWindow::UpdateBackendStatus() {
  if (backend_status_label_ == nullptr) {
    return;
  }

  if (context_ == nullptr || !context_->settings.BackendEnabled()) {
    backend_status_label_->setText(QStringLiteral("Backend: local only"));
    return;
  }

  backend_status_label_->setText(
      QStringLiteral("Backend: %1").arg(context_->settings.BackendBaseUrl()));
}

}  // namespace cppwiki
