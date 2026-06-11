#include "app/application.h"

#include <QCoreApplication>

#include "gui/page.h"

namespace cppwiki {

Application::Application(int& argc, char** argv) : qt_application_(argc, argv) {
  QCoreApplication::setApplicationName("CppWiki");
  QCoreApplication::setApplicationVersion("0.1.0");
  QCoreApplication::setOrganizationName("CppWiki");
  QApplication::setQuitOnLastWindowClosed(true);

  main_window_.SetPage(new Page());
}

Application::~Application() = default;

int Application::Run() {
  main_window_.show();
  return qt_application_.exec();
}

}  // namespace cppwiki
