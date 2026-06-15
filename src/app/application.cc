#include "app/application.h"

#include <QCoreApplication>

#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/page.h"
#include "storage/repository_factory.h"

namespace cppwiki {

Application::Application(int& argc, char** argv) : qt_application_(argc, argv) {
  QCoreApplication::setApplicationName(ToQString(constants::kApplicationName));
  QCoreApplication::setApplicationVersion(ToQString(constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(ToQString(constants::kOrganizationName));
  QApplication::setQuitOnLastWindowClosed(true);

  settings_.emplace(ProgramSettings::FromDefaults());

  // Create document repository using factory (CBLite if available, otherwise file-based)
  auto repository = storage::RepositoryFactory::Create(*settings_);

  // Create application context
  context_ = std::make_unique<AppContext>(*settings_, std::move(repository));

  main_window_.SetContext(context_.get());
}

Application::~Application() = default;

int Application::Run() {
  main_window_.show();
  return qt_application_.exec();
}

}  // namespace cppwiki
