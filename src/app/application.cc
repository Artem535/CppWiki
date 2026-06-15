#include "app/application.h"

#include <QCoreApplication>

#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/page.h"

namespace cppwiki {

Application::Application(int& argc, char** argv) : qt_application_(argc, argv) {
  QCoreApplication::setApplicationName(ToQString(constants::kApplicationName));
  QCoreApplication::setApplicationVersion(ToQString(constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(ToQString(constants::kOrganizationName));
  QApplication::setQuitOnLastWindowClosed(true);

  settings_.emplace(ProgramSettings::FromDefaults());
  main_window_.SetPage(new Page(*settings_));
}

Application::~Application() = default;

int Application::Run() {
  main_window_.show();
  return qt_application_.exec();
}

}  // namespace cppwiki
