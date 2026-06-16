#ifndef CPPWIKI_SRC_GUI_DOCUMENT_TREE_MODEL_H_
#define CPPWIKI_SRC_GUI_DOCUMENT_TREE_MODEL_H_

#include <QAbstractItemModel>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "storage/local_document_repository.h"

namespace cppwiki::gui {

// Internal tree item class for document tree
class DocumentTreeItem {
 public:
  enum class Kind {
    kDocument,
    kRootAction,      // "New note" action at the root level
    kAddChildAction,  // "+" action attached to a document
  };

  explicit DocumentTreeItem(const storage::DocumentSummary& summary, DocumentTreeItem* parent = nullptr);
  explicit DocumentTreeItem(Kind kind, std::string id, std::string title, DocumentTreeItem* parent = nullptr);
  explicit DocumentTreeItem(DocumentTreeItem* parent = nullptr);
  ~DocumentTreeItem();

  [[nodiscard]] const std::string& id() const { return id_; }
  [[nodiscard]] const std::string& title() const { return title_; }
  [[nodiscard]] int sortOrder() const { return sort_order_; }
  [[nodiscard]] Kind kind() const { return kind_; }

  [[nodiscard]] DocumentTreeItem* parent() const { return parent_; }
  [[nodiscard]] const std::vector<std::unique_ptr<DocumentTreeItem>>& children() const {
    return children_;
  }

  void addChild(std::unique_ptr<DocumentTreeItem> child);
  void sortChildren();

  [[nodiscard]] int childRow(const DocumentTreeItem* child) const;
  [[nodiscard]] DocumentTreeItem* childAt(int row) const;
  [[nodiscard]] int rowCount() const { return static_cast<int>(children_.size()); }
  [[nodiscard]] bool isContainer() const { return !children_.empty(); }
  [[nodiscard]] bool isAction() const { return kind_ != Kind::kDocument; }
  [[nodiscard]] bool isAddChildAction() const { return kind_ == Kind::kAddChildAction; }
  [[nodiscard]] bool isRootAction() const { return kind_ == Kind::kRootAction; }

  // Iterative lookup helpers
  [[nodiscard]] DocumentTreeItem* findItemById(std::string_view id);
  [[nodiscard]] const DocumentTreeItem* findItemById(std::string_view id) const;

 private:
  Kind kind_ = Kind::kDocument;
  std::string id_;
  std::string title_;
  int sort_order_ = 0;
  DocumentTreeItem* parent_ = nullptr;
  std::vector<std::unique_ptr<DocumentTreeItem>> children_;
};

// Model for displaying hierarchical document tree
class DocumentTreeModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  explicit DocumentTreeModel(QObject* parent = nullptr);
  ~DocumentTreeModel() override;

  // QAbstractItemModel interface
  [[nodiscard]] QModelIndex index(int row, int column,
                                  const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
  [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

  // Custom interface
  void setDocuments(const std::vector<storage::DocumentSummary>& documents);
  void appendDocument(const storage::DocumentSummary& document);
  [[nodiscard]] std::optional<std::string> documentId(const QModelIndex& index) const;
  [[nodiscard]] QModelIndex indexForDocumentId(std::string_view id) const;
  [[nodiscard]] bool isContainer(const QModelIndex& index) const;
  void requestAddChild(const QModelIndex& parent_document_index);

  // Icon roles
  static constexpr int kIsContainerRole = Qt::UserRole + 1;
  static constexpr int kIsLockedRole = Qt::UserRole + 2;
  static constexpr int kSyncStatusRole = Qt::UserRole + 3;
  static constexpr int kIsActionRole = Qt::UserRole + 4;
  static constexpr int kAddChildActionRole = Qt::UserRole + 5;

 signals:
  void addChildRequested(const QModelIndex& parent_document_index);

 private:
  void buildTree(const std::vector<storage::DocumentSummary>& documents);
  [[nodiscard]] DocumentTreeItem* itemFromIndex(const QModelIndex& index) const;
  [[nodiscard]] QModelIndex indexForItem(const DocumentTreeItem* item) const;

  std::unique_ptr<DocumentTreeItem> root_item_;
};

}  // namespace cppwiki::gui

#endif  // CPPWIKI_SRC_GUI_DOCUMENT_TREE_MODEL_H_
