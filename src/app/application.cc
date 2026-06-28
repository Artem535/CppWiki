#include "app/application.h"

#include <oclero/qlementine.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include "core/constants.h"
#include "core/qt_string.h"
#include "app/application_stylesheet.h"
#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "gui/page.h"
#include "storage/repository_factory.h"

namespace cppwiki {

namespace {

auto ResolveThemePath() -> QString {
  const auto theme_path = constants::kQlementineDarkThemePath;

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
  auth_session_manager_ = std::make_unique<auth::AuthSessionManager>(&qt_application_);
  backend_client_ = std::make_unique<backend::BackendClient>(&qt_application_);
  document_sync_service_ = std::make_unique<sync::DocumentSyncService>(&qt_application_);
  QObject::connect(auth_session_manager_.get(), &auth::AuthSessionManager::accessTokenChanged,
                   backend_client_.get(), &backend::BackendClient::SetAccessToken);
  QObject::connect(auth_session_manager_.get(), &auth::AuthSessionManager::accessTokenChanged,
                   document_sync_service_.get(), &sync::DocumentSyncService::SetAccessToken);
  QObject::connect(backend_client_.get(), &backend::BackendClient::syncBootstrapChanged,
                   &qt_application_, [this]() {
                     if (backend_client_ == nullptr || document_sync_service_ == nullptr) {
                       return;
                     }
                     document_sync_service_->SetBackendBootstrap(
                         backend_client_->CurrentSyncBootstrap());
                   });

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
    if (backend_client_) {
      backend_client_->ApplySettings(*settings_);
    }
    if (auth_session_manager_) {
      auth_session_manager_->ApplySettings(*settings_);
    }
    if (document_sync_service_) {
      document_sync_service_->ApplySettings(*settings_);
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
  auth_session_manager_->ApplySettings(*settings_);
  backend_client_->ApplySettings(*settings_);
  document_sync_service_->ApplySettings(*settings_);
  document_sync_service_->SetRepository(repository);

  // Create application context
  context_ = std::make_unique<AppContext>(*settings_, std::move(repository),
                                          backend_client_.get(), auth_session_manager_.get(),
                                          document_sync_service_.get());

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
    const auto theme_path = ResolveThemePath();
    if (!theme_path.isEmpty()) {
      qlementine_style->setThemeJsonPath(theme_path);
    }
  }

  ApplyApplicationStylesheet();
}

}  // namespace cppwiki
