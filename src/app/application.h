#ifndef CPPWIKI_SRC_APP_APPLICATION_H_
#define CPPWIKI_SRC_APP_APPLICATION_H_

#include <QApplication>
#include <memory>
#include <optional>

#include "app/app_context.h"
#include "app/program_settings.h"
#include "auth/auth_session_manager.h"
#include "backend/backend_client.h"
#include "gui/main_window.h"
#include "sync/document_sync_service.h"
#include "sync/sync_service.h"

namespace oclero::qlementine {
class QlementineStyle;
}

namespace cppwiki {

class Application final {
 public:
  Application(int& argc, char** argv);
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  ~Application();

  int Run();

 private:
  void ReloadContext();
  void ApplyAppearanceFromSettings(const ProgramSettings& settings);

  QApplication qt_application_;
  std::optional<ProgramSettings> settings_;
  std::unique_ptr<auth::AuthSessionManager> auth_session_manager_;
  std::unique_ptr<backend::BackendClient> backend_client_;
  std::unique_ptr<sync::SyncService> document_sync_service_;
  std::unique_ptr<AppContext> context_;
  MainWindow main_window_;
};

// Returns the process-wide oclero::qlementine::QlementineStyle instance created by
// Application. Useful for code that wants the real style object directly (e.g. a dialog
// that isn't parented under MainWindow and so never risks having its style() wrapped).
//
// Note this is NOT a general fix for hand-painted qlementine widgets (Switch,
// SegmentedControl, ...) losing their theme colors: once any ancestor up to a widget's
// top-level window has a non-empty styleSheet(), Qt's QStyleSheetStyle wraps that widget's
// style() regardless of any setStyle() call made on it afterwards — setStyle() itself gets
// intercepted by the same mechanism. The real fix is to never give the top-level window
// (MainWindow) a stylesheet in the first place; see
// MainWindow::ApplyStylesheetToSafeDescendants()'s comment for how cppwiki.qss is applied
// instead.
[[nodiscard]] auto GetQlementineStyle() -> oclero::qlementine::QlementineStyle*;

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APPLICATION_H_
