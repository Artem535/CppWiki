#ifndef CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
#define CPPWIKI_SRC_GUI_MAIN_WINDOW_H_

#include <QMainWindow>
#include <QPointer>

#include "app/accent_color.h"
#include "document/document.h"
#include "gui/workspace_rail_widget.h"

class QGridLayout;
class QFrame;
class QDialog;
class QAction;
class QLabel;
class QMenu;
class QStackedWidget;
class QToolBar;
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

  // ADR-016: re-styles the safe-descendant widgets with the new accent color (see
  // ApplyStylesheetToSafeDescendants() below) and repaints collaboration_panel_'s
  // "viewing"-state tint, which can't go through QSS since collaboration_panel_ is an
  // ancestor of edit_mode_switch_ and never gets a stylesheet at all. Called by
  // Application::ApplyAppearanceFromSettings() whenever the user's chosen accent changes.
  void ApplyAccentColor(AccentColor accent_color);

 signals:
  void settingsChanged();

 private:
  void BuildUi();
  void CreateInitialPage();

  // Applies the app-wide cppwiki.qss to every widget below MainWindow that needs CSS-driven
  // styling AND is not an ancestor of edit_mode_switch_ (an oclero::qlementine::Switch).
  //
  // Switch paints itself by qobject_cast<QlementineStyle*>(this->style()) and falls back to
  // plain colors when that fails. Qt's QStyleSheetStyle wraps a widget's style() — even one
  // explicitly assigned via setStyle() — the moment ANY ancestor up to the top-level window
  // has a non-empty styleSheet(); there is no public API to exempt one descendant from that.
  // A prior fix (PR #36) tried pinning edit_mode_switch_'s style() after the fact; it didn't
  // stick because setStyle() itself gets intercepted and re-wrapped by the same mechanism as
  // long as MainWindow (the top-level ancestor) carries a stylesheet.
  //
  // So MainWindow itself is never given a stylesheet. Instead, each widget that needs
  // cppwiki.qss rules is styled individually, as long as it isn't on the path from
  // edit_mode_switch_ up to MainWindow. collaboration_panel_ and header_row ARE on that path
  // (collaboration_panel_ -> edit_mode_widget -> edit_mode_switch_), so collaboration_panel_
  // paints its own background/border in code (see CollaborationPanelFrame) instead of via
  // QSS, and header_row simply relies on QWidget's default (transparent) painting.
  void ApplyStylesheetToSafeDescendants(AccentColor accent_color);
  void ShowSettingsDialog();
  void UpdateBackendStatus();
  void UpdateSyncStatus();
  void ShowSyncDetailsDialog();
  void UpdateDocumentStatus(const QString& message, bool is_error);
  void UpdateCollaborationStatus(const QString& summary, const QString& details, bool is_warning);
  void UpdateEditModeUi(const QString& label, bool checked, bool enabled);
  // Shows/hides and labels the native Export control (issue #96) per
  // Page::documentKindStateChanged(): visible only while a Jupyter notebook or Excalidraw canvas
  // document is open (never for wiki pages or no selection). Import (issue #102 follow-up)
  // always creates a brand new document rather than acting on the open one, so it isn't gated
  // here at all — see BuildUi(). The slot signature drops documentKindStateChanged()'s trailing
  // `editable` argument (Qt allows connecting to a slot with fewer parameters than the signal).
  void UpdateFileActionsUi(document::DocumentKind kind, bool has_document);
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
  // Standard native document-tools toolbar. It is deliberately separate from the collaboration
  // status strip: that strip reports state, whereas these are application actions.
  QToolBar* document_tools_toolbar_ = nullptr;
  // File menu button (issue #102): Import/Export live under a "File" dropdown on the toolbar
  // rather than as two flat toolbar buttons.
  QToolButton* file_menu_button_ = nullptr;
  QMenu* file_menu_ = nullptr;
  QAction* import_action_ = nullptr;
  QAction* export_action_ = nullptr;
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
  AccentColor current_accent_color_ = AccentColor::kBlue;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_MAIN_WINDOW_H_
