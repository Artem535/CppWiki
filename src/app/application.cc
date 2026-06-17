#include "app/application.h"

#include <oclero/qlementine.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QSettings>

#include "core/constants.h"
#include "core/qt_string.h"
#include "gui/page.h"
#include "storage/repository_factory.h"

namespace cppwiki {

namespace {

auto ResolveThemePath(ProgramSettings::ThemeMode theme_mode) -> QString {
  const auto theme_path = theme_mode == ProgramSettings::ThemeMode::kLight
                              ? constants::kQlementineLightThemePath
                              : constants::kQlementineDarkThemePath;

  const auto candidates = {
      QDir::current().filePath(ToQString(theme_path)),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") +
                                                           ToQString(theme_path)),
      QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../") +
                                                           ToQString(theme_path)),
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

  ReloadContext();

  QObject::connect(&main_window_, &MainWindow::settingsChanged, &qt_application_, [this]() {
    if (!settings_) {
      return;
    }

    const QSettings settings;
    settings_.emplace(ProgramSettings::FromSettings(settings));
    if (context_) {
      context_->settings = *settings_;
    }
    ApplyAppearanceFromSettings(*settings_);
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
  ApplyAppearanceFromSettings(*settings_);

  // Create document repository using factory (CBLite if available, otherwise file-based)
  auto repository = storage::RepositoryFactory::Create(*settings_);

  // Create application context
  context_ = std::make_unique<AppContext>(*settings_, std::move(repository));

  main_window_.SetContext(context_.get());
}

void Application::ApplyAppearanceFromSettings(const ProgramSettings& settings) {
  auto font = QApplication::font();
  if (settings.ApplicationFontPointSize() > 0) {
    font.setPointSize(settings.ApplicationFontPointSize());
    QApplication::setFont(font);
  }

  if (auto* qlementine_style =
          qobject_cast<oclero::qlementine::QlementineStyle*>(QApplication::style())) {
    const auto theme_path = ResolveThemePath(settings.ThemeModeValue());
    if (!theme_path.isEmpty()) {
      qlementine_style->setThemeJsonPath(theme_path);
    }
  }
}

}  // namespace cppwiki
