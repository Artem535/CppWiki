#include "gui/main_window.h"

#include <QAction>
#include <QDialog>
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
}

void MainWindow::CreateInitialPage() {
  if (context_ == nullptr) {
    setCentralWidget(nullptr);
    setWindowTitle(QStringLiteral("CppWiki"));
    return;
  }

  // Create the main page with the application context
  auto* page = new Page(*context_, this);
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

  statusBar()->showMessage(QStringLiteral("Settings saved."), 3000);
  emit settingsChanged();
}

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("CppWiki"));
  resize(constants::kInitialWindowWidth, constants::kInitialWindowHeight);
  statusBar()->showMessage(QStringLiteral("Ready"));
  statusBar()->setStyleSheet(QStringLiteral(R"(
    QStatusBar {
      background-color: palette(window);
    }
  )"));

  auto* settings_menu = menuBar()->addMenu(QStringLiteral("Settings"));
  auto* preferences_action =
      settings_menu->addAction(QIcon::fromTheme(QStringLiteral("settings-configure")),
                               QStringLiteral("Preferences..."));
  preferences_action->setShortcut(QKeySequence::Preferences);
  connect(preferences_action, &QAction::triggered, this, [this]() { ShowSettingsDialog(); });
}

}  // namespace cppwiki
