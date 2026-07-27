#include "app/application.h"

#include <oclero/qlementine.hpp>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <optional>

#include "core/constants.h"
#include "core/qt_string.h"
#include "app/accent_color.h"
#include "app/application_stylesheet.h"
#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "gui/page.h"
#include "storage/repository_factory.h"

namespace cppwiki {

namespace {

// Non-owning; owned by QApplication (parented to it). Kept outside the class as a stable
// accessor for code that needs the real QlementineStyle instance (e.g. sizing hints computed
// before a widget is fully parented). See MainWindow::ApplyStylesheetToSafeDescendants()'s
// comment for why main_window_ is never given its own stylesheet: doing so would wrap the
// style() of any qlementine widget nested under it in a QStyleSheetStyle proxy, breaking that
// widget's own qobject_cast<QlementineStyle*>(style()) — a wrap that a subsequent explicit
// setStyle() call cannot undo, since it too gets intercepted while any ancestor is styled.
oclero::qlementine::QlementineStyle* g_qlementine_style = nullptr;

// QlementineStyle does not expose a getter for the currently applied theme path, so the last
// value we set is tracked here to avoid reapplying it (and re-churning the style's caches) when
// nothing actually changed.
QString g_applied_theme_path;

// Tracks the last-applied ADR-016 accent color so ApplyAppearanceFromSettings() only reapplies
// the (main_window-scoped, not app-wide) stylesheet when the accent actually changed.
std::optional<AccentColor> g_applied_accent_color;

auto ResolveThemePath() -> QString {
  // A Qt resource path (":/...", see constants::kQlementineDarkThemePath) is embedded in the
  // binary itself, so it resolves the same way regardless of CWD or install layout -- no
  // candidate-path guessing needed (unlike ResolveEditorFallbackHtmlPath()/
  // ResolveDefaultEditorDistDirectory(), which guess because their targets are real filesystem
  // trees that can't be embedded as Qt resources).
  const auto theme_path = ToQString(constants::kQlementineDarkThemePath);
  return QFileInfo(theme_path).exists() ? theme_path : QString{};
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
  // main_window_ (the top-level window) is deliberately never given a stylesheet — see
  // MainWindow::ApplyStylesheetToSafeDescendants()'s comment for why: Qt's QStyleSheetStyle
  // would wrap every descendant's style(), including hand-painted qlementine widgets like
  // edit_mode_switch_, breaking their internal qobject_cast<QlementineStyle*>(style()). Instead
  // MainWindow applies cppwiki.qss (and the ADR-016 accent tint, via MainWindow::
  // ApplyAccentColor()) piecemeal, to specific descendants that aren't on the path up to such
  // widgets. Settings aren't loaded yet at this point — ReloadContext() below populates
  // settings_ and calls ApplyAppearanceFromSettings(), which applies the user's actual accent
  // choice (or the kBlue default for a fresh install) via that path.
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
  // Note: cppwiki.qss (ApplyApplicationStylesheet(), called piecemeal from MainWindow — see
  // MainWindow::ApplyStylesheetToSafeDescendants()) is intentionally NOT reapplied here: it's
  // static content unrelated to ProgramSettings, applied once when MainWindow builds its UI.
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

  // Unlike the static app/cppwiki.qss content, the ADR-016 accent tint depends on
  // ProgramSettings, so (unlike the app-wide-stylesheet note above) it does need to be
  // reapplied when it changes. MainWindow::ApplyAccentColor() only re-styles the specific safe
  // descendants (see ApplyStylesheetToSafeDescendants()) and repaints collaboration_panel_'s
  // "viewing" tint in code — main_window_ itself is never given a stylesheet, so this can't
  // re-wrap QApplication::style() in a fresh QStyleSheetStyle proxy.
  const auto accent_color = AccentColorFromKey(settings.AccentColorKey());
  if (!g_applied_accent_color.has_value() || *g_applied_accent_color != accent_color) {
    main_window_.ApplyAccentColor(accent_color);
    g_applied_accent_color = accent_color;
  }
}

}  // namespace cppwiki
