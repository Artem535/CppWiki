#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_H_

#include <QMainWindow>
#include <QPointer>

#include "gui/workspace_rail_widget.h"

class QGridLayout;
class QFrame;
class QDialog;
class QLabel;
class QStackedWidget;
class QToolButton;
class QWidget;

namespace oclero::qlementine {
class StatusBadgeWidget;
class Switch;
}  // namespace oclero::qlementine

namespace cppwiki::gui {
class PresenceStripWidget;
}  // namespace cppwiki::gui

namespace cppwiki::sync {
enum class DocumentSyncState;
}

namespace cppwiki::gui::merge {
class ConflictMergeDialog;
}

namespace cppwiki {

struct AppContext;
class Page;
class SyncDetailsDialog;

class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;
  ~MainWindow() override;

  // Sets the application context and creates the initial page.
  void SetContext(AppContext* context);
  bool eventFilter(QObject* watched, QEvent* event) override;

  // Re-pins hand-painted qlementine widgets (e.g. editModeSwitch) to the real QlementineStyle.
  // Must be called after Application has set the process-wide style (see GetQlementineStyle()
  // in application.h) — MainWindow is a plain Application member, so it's fully constructed,
  // including BuildUi(), before that happens; pinning from inside BuildUi() itself would only
  // ever see a null style.
  void PinHandPaintedWidgetsToQlementineStyle();

 signals:
  void settingsChanged();

 private:
  void BuildUi();
  void CreateInitialPage();
  void ShowSettingsDialog();
  void UpdateBackendStatus();
  void UpdateSyncStatus();
  void ShowSyncDetailsDialog();
  void UpdateDocumentStatus(const QString& message, bool is_error);
  void UpdateCollaborationStatus(const QString& summary, const QString& details, bool is_warning);
  void UpdateEditModeUi(const QString& label, bool checked, bool enabled);
  void UpdateAuthCollaborationHint();
  void RefreshCollaborationSecondaryText();
  void RefreshSyncDetailsDialog();
  // Opens (or reuses) the standalone, non-modal conflict resolution window for
  // the given conflict. See ADR-013 / doc/CONTEXT.adoc "Conflict resolution
  // window".
  void ShowConflictWindow(const QString& conflict_id);
  // Reopen affordance for a conflict window the user previously closed via its
  // close button: re-raises the existing window if still alive, otherwise
  // opens one for the current document's conflict (or the first pending one).
  void ReopenConflictWindow();
  // Handles a rail selection: swaps the mode-content stack's current page.
  // Does not affect Documents-mode internal state.
  void HandleModeSelected(gui::WorkspaceRailWidget::Mode mode);

  AppContext* context_ = nullptr;
  Page* current_page_ = nullptr;
  gui::WorkspaceRailWidget* workspace_rail_ = nullptr;
  QStackedWidget* mode_stack_ = nullptr;
  QWidget* ai_chat_page_ = nullptr;
  QWidget* code_page_ = nullptr;
  QWidget* shell_widget_ = nullptr;
  QGridLayout* shell_layout_ = nullptr;
  QWidget* current_sidebar_widget_ = nullptr;
  QWidget* current_content_widget_ = nullptr;
  QFrame* collaboration_panel_ = nullptr;
  gui::PresenceStripWidget* presence_strip_widget_ = nullptr;
  QLabel* edit_mode_label_ = nullptr;
  QLabel* save_state_label_ = nullptr;
  oclero::qlementine::Switch* edit_mode_switch_ = nullptr;
  QString save_state_hint_;
  QString collaboration_hint_;
  QString auth_hint_;
  QString fallback_editor_user_id_;
  bool fallback_editor_is_self_ = false;
  QToolButton* backend_refresh_button_ = nullptr;
  QWidget* document_status_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* document_status_badge_ = nullptr;
  QLabel* document_status_label_ = nullptr;
  QWidget* backend_status_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* backend_status_badge_ = nullptr;
  QLabel* backend_status_label_ = nullptr;
  QWidget* sync_status_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* sync_status_badge_ = nullptr;
  QLabel* sync_status_label_ = nullptr;
  QWidget* sync_conflicts_widget_ = nullptr;
  oclero::qlementine::StatusBadgeWidget* sync_conflicts_badge_ = nullptr;
  QLabel* sync_conflicts_label_ = nullptr;
  QPointer<SyncDetailsDialog> sync_details_dialog_;
  QPointer<gui::merge::ConflictMergeDialog> conflict_window_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
