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
// Application, bypassing QApplication::style(). This is needed because once a non-empty
// application-wide stylesheet is applied (see ApplyApplicationStylesheet()), Qt wraps
// QApplication::style() in an internal QStyleSheetStyle proxy; qobject_cast<QlementineStyle*>
// against that proxy fails, breaking any qlementine widget (e.g.
// oclero::qlementine::SegmentedControl) that reads theme colors/fonts by downcasting its own
// style() directly. Widgets affected by this should call widget->setStyle(GetQlementineStyle())
// explicitly to keep getting the real QlementineStyle regardless of the app-wide stylesheet.
[[nodiscard]] auto GetQlementineStyle() -> oclero::qlementine::QlementineStyle*;

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APPLICATION_H_
