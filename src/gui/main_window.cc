#include "gui/main_window.h"

#include <QStatusBar>
#include <QString>
#include <QWidget>

#include "gui/i_page.h"

namespace cppwiki {
namespace {

constexpr int kInitialWindowWidth = 1280;
constexpr int kInitialWindowHeight = 800;

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  BuildUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::SetPage(IPage* page) {
  current_page_ = page;
  if (current_page_ == nullptr) {
    setCentralWidget(nullptr);
    setWindowTitle(QStringLiteral("CppWiki"));
    return;
  }

  setCentralWidget(current_page_->Widget());
  setWindowTitle(QStringLiteral("CppWiki - %1").arg(current_page_->Title()));
}

void MainWindow::BuildUi() {
  setWindowTitle(QStringLiteral("CppWiki"));
  resize(kInitialWindowWidth, kInitialWindowHeight);
  statusBar()->showMessage(QStringLiteral("Ready"));
}

}  // namespace cppwiki
