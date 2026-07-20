#ifndef CPPWIKI_SRC_GUI_PAGE_H_
#define CPPWIKI_SRC_GUI_PAGE_H_

#include <QWidget>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "app/app_context.h"
#include "backend/backend_client.h"
#include "document/document.h"
#include "gui/i_page.h"

class QWebChannel;
class QWebEngineView;
class QTreeView;
class QPushButton;
class QLabel;
class QVBoxLayout;
class QModelIndex;
class QTimer;
class QWidget;

namespace cppwiki::bridge {
class QEditorBridge;
}

namespace cppwiki::gui {
class DocumentTreeModel;
class DocumentTreeView;
}  // namespace cppwiki::gui

namespace cppwiki {

class Page final : public QWidget, public IPage {
  Q_OBJECT

 public:
  explicit Page(const AppContext& context, QWidget* parent = nullptr);

  Page(const Page&) = delete;
  auto operator=(const Page&) -> Page& = delete;

  ~Page() override;

  [[nodiscard]] auto Title() const -> QString override;
  auto Widget() -> QWidget* override;
  [[nodiscard]] auto SidebarWidget() const -> QWidget*;
  [[nodiscard]] auto ContentWidget() const -> QWidget*;
  void ToggleEditMode();
  void SetEditModeEnabled(bool enabled);

  // Re-evaluates the sync-conflict state for the currently open document and
  // re-applies it to the editability gate / tree indicators. Called after a
  // conflict is resolved elsewhere (e.g. from the auto-popup conflict window).
  void RefreshCurrentDocumentConflictState();

  // Native Import/Export entry points (issue #96) for MainWindow's top-level Import/Export
  // controls — replaces the removed in-page FileActionsToolbar (NotebookView.tsx) and inline
  // Excalidraw import/export buttons. Both call QEditorBridge's existing
  // exportTextToFile()/importTextFromFile() (QFileDialog-backed, unchanged) directly instead of
  // routing through the JS/React layer; ImportCurrentDocumentFromFile() reuses
  // OpenDocumentWithAccess() (the same native document-reload path used elsewhere, e.g. after
  // create/delete) to refresh the JS-side view with the imported content rather than inventing a
  // separate refresh mechanism. No-ops when no document is open or the open document is a
  // kWikiPage (wiki pages have no import/export concept); ImportCurrentDocumentFromFile() also
  // no-ops when the open document is not currently editable.
  void ExportCurrentDocumentToFile();
  void ImportCurrentDocumentFromFile();

 signals:
  void settingsRequested();
  void documentStatusChanged(const QString& message, bool is_error);
  void collaborationStatusChanged(const QString& summary, const QString& details, bool is_warning);
  void editModeStateChanged(const QString& label, bool checked, bool enabled);
  // Emitted when the document just opened/loaded has an unresolved sync
  // conflict (ADR-013) — MainWindow uses this to auto-open the standalone
  // conflict resolution window.
  void documentConflictDetected(const QString& page_id, const QString& conflict_id);
  // Emitted whenever the currently open document (or lack thereof) changes in a way that
  // affects the native Import/Export controls (issue #96): on open/close/reload and on
  // editability changes (lock gained/lost, sync conflict). `has_document` is false when no
  // document is selected at all; `kind` is only meaningful when `has_document` is true.
  // MainWindow uses this to show/hide and label its native Import/Export buttons — wiki pages
  // have no import/export concept, so it hides them for `kind == DocumentKind::kWikiPage` too.
  void documentKindStateChanged(document::DocumentKind kind, bool has_document, bool editable);

 private:
  void BuildUi();
  void LoadEditor();
  void InstallWebChannelScript();
  // Rejects/cancels native browser file-picker and download requests the embedded editor bundle
  // (Excalidraw) may trigger, which otherwise crash the app in this embedding (issue #82) — see
  // the doc comment at the definition.
  void InstallNativeFilePickerGuards();
  void PopulatePageList();
  void RebuildWorkspaceTree();
  void CreateNewDocument(const QString& workspace_id,
                         document::DocumentKind kind = document::DocumentKind::kWikiPage);
  void CreateChildDocument(const QModelIndex& parent_index,
                           document::DocumentKind kind = document::DocumentKind::kWikiPage);
  void RenameDocument(const QModelIndex& index);
  void DeleteDocument(const QModelIndex& index);
  void OpenDocumentWithAccess(const QString& page_id);
  void EnterEditMode();
  void ExitEditMode(bool due_to_inactivity = false);
  void ApplyDocumentAccessState(const backend::DocumentAccessState& access_state);
  void UpdateEditModeControls();
  void StartEditInactivityTimer();
  void StopEditInactivityTimer();
  void NoteEditActivity();
  void HandleEditInactivityTimeout();
  void MoveDocument(const QModelIndex& index, int delta);
  void MoveDocumentToPlacement(const QString& source_document_id, const QString& target_parent_id,
                               bool has_parent_id, int target_sort_order,
                               const QString& workspace_id);
  void HandleCreatedDocument(const QVariantMap& response, const QString& workspace_id);
  void SelectDocumentById(const QString& page_id);
  void ExpandWorkspace(const QString& workspace_id);
  void OnTreePressed(const QModelIndex& index);
  void ShowContextMenu(const QPoint& position);
  void HandleDocumentSaved(const QString& page_id, bool success, const QString& message);
  [[nodiscard]] std::set<std::string> CaptureExpandedDocumentIds() const;
  void RestoreExpandedDocumentIds(const std::set<std::string>& expanded_ids);
  void ExpandAncestors(const QString& page_id);
  [[nodiscard]] std::vector<storage::DocumentSummary> FetchAllDocumentSummaries() const;
  [[nodiscard]] std::vector<storage::DocumentSummary> FetchDocumentSummaries(
      const QString& workspace_id) const;
  [[nodiscard]] std::optional<std::string> MapToParentDocumentId(const QModelIndex& index) const;
  [[nodiscard]] std::optional<QString> WorkspaceIdFromIndex(const QModelIndex& index) const;
  void ActivateWorkspace(const QString& workspace_id);

  void ApplyBridgeSessionContext();
  void RefreshSelectedDocumentAccess();
  void UpdateAuthCard();
  void RefreshPageListIfChanged();
  void RefreshWorkspaceHydrationState();
  void ApplyConflictStateForDocument(const QString& page_id);
  void RefreshConflictedDocumentIndicators();
  // Emits documentKindStateChanged() with the current selection/kind/editability. Called
  // wherever those change (document load/load-failure/selection-cleared, and from
  // UpdateEditModeControls() so editability-only changes are also reflected).
  void EmitDocumentKindState();
  void ExportPersistedCurrentDocument();

  const AppContext& context_;
  QWidget* page_panel_ = nullptr;
  gui::DocumentTreeView* workspace_tree_view_ = nullptr;
  std::unique_ptr<gui::DocumentTreeModel> workspace_tree_model_;
  QWidget* content_widget_ = nullptr;
  QPushButton* settings_button_ = nullptr;
  QLabel* profile_avatar_label_ = nullptr;
  QLabel* profile_name_label_ = nullptr;
  QLabel* profile_hint_label_ = nullptr;
  QPushButton* profile_action_button_ = nullptr;
  QWebEngineView* editor_view_ = nullptr;
  QWebChannel* channel_ = nullptr;
  bridge::QEditorBridge* editor_bridge_ = nullptr;
  QString selected_page_id_;
  QString current_workspace_id_{QStringLiteral("default")};
  QStringList available_workspace_ids_{QStringList{QStringLiteral("default")}};
  bool current_document_editable_ = false;
  bool current_document_local_only_ = true;
  // Kind of the currently open document (ADR-017), mirrored from the "kind" field of the
  // documentLoaded payload — see EmitDocumentKindState(). Meaningless while selected_page_id_ is
  // empty (no document open); defaults to kWikiPage, matching PageMetadata's default.
  document::DocumentKind current_document_kind_ = document::DocumentKind::kWikiPage;
  bool export_after_save_ = false;
  bool pending_inactivity_exit_notice_ = false;
  QTimer* edit_inactivity_timer_ = nullptr;
  std::vector<storage::DocumentSummary> last_document_summaries_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_PAGE_H_
