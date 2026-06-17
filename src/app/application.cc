#include "app/application.h"

#include <oclero/qlementine.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/page.h"
#include "storage/repository_factory.h"

namespace cppwiki {

namespace {

auto ResolveDarkThemePath() -> QString {
  const auto candidates = {
      QDir::current().filePath(ToQString(constants::kQlementineDarkThemePath)),
      QDir(QCoreApplication::applicationDirPath()).filePath(
          QStringLiteral("../../") + ToQString(constants::kQlementineDarkThemePath)),
      QDir(QCoreApplication::applicationDirPath()).filePath(
          QStringLiteral("../../../") + ToQString(constants::kQlementineDarkThemePath)),
  };

  for (const auto& candidate : candidates) {
    if (QFileInfo(candidate).exists()) {
      return candidate;
    }
  }
  return {};
}

}  // namespace

Application::Application(int& argc, char** argv) : qt_application_(argc, argv) {
  QCoreApplication::setApplicationName(ToQString(constants::kApplicationName));
  QCoreApplication::setApplicationVersion(ToQString(constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(ToQString(constants::kOrganizationName));
  QApplication::setQuitOnLastWindowClosed(true);
  QApplication::setStyle(new oclero::qlementine::QlementineStyle(&qt_application_));
  if (auto* qlementine_style =
          qobject_cast<oclero::qlementine::QlementineStyle*>(QApplication::style())) {
    const auto theme_path = ResolveDarkThemePath();
    if (!theme_path.isEmpty()) {
      qlementine_style->setThemeJsonPath(theme_path);
    }
  }

  ReloadContext();

  QObject::connect(&main_window_, &MainWindow::settingsChanged, &qt_application_, [this]() {
    ReloadContext();
  });
}

Application::~Application() = default;

int Application::Run() {
  main_window_.show();
  return qt_application_.exec();
}

void Application::ReloadContext() {
  QSettings settings;
  settings_.emplace(ProgramSettings::FromSettings(settings));

  // Create document repository using factory (CBLite if available, otherwise file-based)
  auto repository = storage::RepositoryFactory::Create(*settings_);

  // Create application context
  context_ = std::make_unique<AppContext>(*settings_, std::move(repository));

  main_window_.SetContext(context_.get());
}

}  // namespace cppwiki
