#ifndef CPPWIKI_SRC_GUI_PAGE_H_
#define CPPWIKI_SRC_GUI_PAGE_H_

#include <QWidget>

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "app/app_context.h"
#include "gui/i_page.h"

class QWebChannel;
class QWebEngineView;
class QTreeView;
class QPushButton;
class QModelIndex;
class QWidget;

namespace cppwiki::bridge {
class QEditorBridge;
}

namespace cppwiki::gui {
class DocumentTreeModel;
}

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

 signals:
  void settingsRequested();
  void documentStatusChanged(const QString& message, bool is_error);

 private:
  void BuildUi();
  void LoadEditor();
  void InstallWebChannelScript();
  void PopulatePageList();
  void CreateNewDocument();
  void CreateChildDocument(const QModelIndex& parent_index);
  void RenameDocument(const QModelIndex& index);
  void DeleteDocument(const QModelIndex& index);
  void MoveDocument(const QModelIndex& index, int delta);
  void MoveDocumentToPlacement(const QString& source_document_id, const QString& target_parent_id,
                               bool has_parent_id, int target_sort_order);
  void HandleCreatedDocument(const QVariantMap& response);
  void SelectDocumentById(const QString& page_id);
  void SetupTreeView();
  void OnTreePressed(const QModelIndex& index);
  void ShowContextMenu(const QPoint& position);
  void HandleDocumentSaved(const QString& page_id, bool success, const QString& message);
  [[nodiscard]] std::set<std::string> CaptureExpandedDocumentIds() const;
  void RestoreExpandedDocumentIds(const std::set<std::string>& expanded_ids);
  void ExpandAncestors(const QString& page_id);
  [[nodiscard]] std::vector<storage::DocumentSummary> FetchDocumentSummaries() const;
  [[nodiscard]] std::optional<std::string> MapToParentDocumentId(const QModelIndex& index) const;
  [[nodiscard]] storage::DocumentSummary SummaryFromVariantMap(const QVariantMap& document) const;

  const AppContext& context_;
  QWidget* page_panel_ = nullptr;
  QPushButton* new_document_button_ = nullptr;
  QPushButton* settings_button_ = nullptr;
  QWebEngineView* editor_view_ = nullptr;
  QTreeView* page_tree_ = nullptr;
  std::unique_ptr<gui::DocumentTreeModel> tree_model_;
  QWebChannel* channel_ = nullptr;
  bridge::QEditorBridge* editor_bridge_ = nullptr;
  QString selected_page_id_;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_PAGE_H_
