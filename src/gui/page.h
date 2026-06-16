#ifndef CPPWIKI_SRC_GUI_PAGE_H_
#define CPPWIKI_SRC_GUI_PAGE_H_

#include <QWidget>

#include <memory>

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
 public:
  explicit Page(const AppContext& context, QWidget* parent = nullptr);

  Page(const Page&) = delete;
  auto operator=(const Page&) -> Page& = delete;

  ~Page() override;

  [[nodiscard]] auto Title() const -> QString override;
  auto Widget() -> QWidget* override;

 private:
  void BuildUi();
  void LoadEditor();
  void InstallWebChannelScript();
  void PopulatePageList();
  void OpenPage(const QModelIndex& index);
  void CreateNewDocument();
  void CreateChildDocument(const QModelIndex& parent_index);
  void HandleCreatedDocument(const QVariantMap& response);
  void SelectDocumentById(const QString& page_id);
  void SetupTreeView();
  void OnTreePressed(const QModelIndex& index);
  [[nodiscard]] std::optional<std::string> MapToParentDocumentId(const QModelIndex& index) const;
  [[nodiscard]] storage::DocumentSummary SummaryFromVariantMap(const QVariantMap& document) const;

  const AppContext& context_;
  QWidget* page_panel_ = nullptr;
  QPushButton* new_document_button_ = nullptr;
  QWebEngineView* editor_view_ = nullptr;
  QTreeView* page_tree_ = nullptr;
  std::unique_ptr<gui::DocumentTreeModel> tree_model_;
  QWebChannel* channel_ = nullptr;
  bridge::QEditorBridge* editor_bridge_ = nullptr;
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_GUI_PAGE_H_
