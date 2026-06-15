#include "gui/main_window.h"

#include <QStatusBar>
#include <QString>
#include <QWidget>

#include "app/app_context.h"
#include "gui/i_page.h"
#include "gui/page.h"

namespace cppwiki {
namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 800;

}  // namespace

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

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("CppWiki"));
  resize(kInitialWindowWidth, kInitialWindowHeight);
  statusBar()->showMessage(QStringLiteral("Ready"));
}

}  // namespace cppwiki
