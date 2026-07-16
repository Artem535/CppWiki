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

// Non-owning; owned by QApplication (parented to it). Kept outside the class so it can be
// recovered even after QApplication::style() gets wrapped by a QStyleSheetStyle proxy once a
// non-empty app-wide stylesheet is applied (see ApplyApplicationStylesheet()).
oclero::qlementine::QlementineStyle* g_qlementine_style = nullptr;

// QlementineStyle does not expose a getter for the currently applied theme path, so the last
// value we set is tracked here to avoid reapplying it (and re-churning the style's caches) when
// nothing actually changed.
QString g_applied_theme_path;

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

auto GetQlementineStyle() -> oclero::qlementine::QlementineStyle* { return g_qlementine_style; }

Application::Application(int& argc, char** argv) : qt_application_(argc, argv) {
  QCoreApplication::setApplicationName(ToQString(constants::kApplicationName));
  QCoreApplication::setApplicationVersion(ToQString(constants::kApplicationVersion));
  QCoreApplication::setOrganizationName(ToQString(constants::kOrganizationName));
  QApplication::setQuitOnLastWindowClosed(true);
  auto* qlementine_style = new oclero::qlementine::QlementineStyle(&qt_application_);
  g_qlementine_style = qlementine_style;
  QApplication::setStyle(qlementine_style);
  ApplyApplicationStylesheet(&main_window_);
  auth_session_manager_ = std::make_unique<auth::AuthSessionManager>(&qt_application_);
  backend_client_ = std::make_unique<backend::BackendClient>(&qt_application_);
  document_sync_service_ = std::make_unique<sync::DocumentSyncService>(&qt_application_);
  QObject::connect(auth_session_manager_.get(), &auth::AuthSessionManager::accessTokenChanged,
                   backend_client_.get(), &backend::BackendClient::SetAccessToken);
  QObject::connect(auth_session_manager_.get(), &auth::AuthSessionManager::accessTokenChanged,
                   document_sync_service_.get(), &sync::SyncService::SetAccessToken);
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
  // Only touch QApplication::setFont()/QlementineStyle::setThemeJsonPath() when the value
  // actually changes. Both operations invalidate Qt's font/style caches; calling them
  // unconditionally on every settings save (even when nothing about appearance changed)
  // repeatedly churns those caches for no reason and has been a source of teardown crashes.
  //
  // Note: the application-wide stylesheet (ApplyApplicationStylesheet()) is intentionally NOT
  // reapplied here — it is static content unrelated to ProgramSettings and is applied once, in
  // the constructor. Calling qApp->setStyleSheet() repeatedly wraps QApplication::style() in a
  // fresh QStyleSheetStyle proxy each time, which both breaks qobject_cast<QlementineStyle*>
  // (see GetQlementineStyle()) and adds unnecessary style-object churn during shutdown.
  auto font = QApplication::font();
  if (settings.ApplicationFontPointSize() > 0 &&
      font.pointSize() != settings.ApplicationFontPointSize()) {
    font.setPointSize(settings.ApplicationFontPointSize());
    QApplication::setFont(font);
  }

  if (auto* qlementine_style = GetQlementineStyle()) {
    const auto theme_path = ResolveThemePath();
    if (!theme_path.isEmpty() && theme_path != g_applied_theme_path) {
      qlementine_style->setThemeJsonPath(theme_path);
      g_applied_theme_path = theme_path;
    }
  }
}

}  // namespace cppwiki
