#include "gui/document_tree_model.h"

#include <QIcon>

#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <string_view>
#include <utility>

#include "core/constants.h"

namespace cppwiki::gui {

// DocumentTreeItem implementation
DocumentTreeItem::DocumentTreeItem(const storage::DocumentSummary& summary, DocumentTreeItem* parent)
    : kind_(Kind::kDocument),
      id_(summary.id),
      title_(summary.title),
      sort_order_(summary.sort_order),
      parent_(parent) {}

DocumentTreeItem::DocumentTreeItem(Kind kind, std::string id, std::string title, DocumentTreeItem* parent)
    : kind_(kind), id_(std::move(id)), title_(std::move(title)), parent_(parent) {}

DocumentTreeItem::DocumentTreeItem(DocumentTreeItem* parent) : parent_(parent) {}

DocumentTreeItem::~DocumentTreeItem() = default;

void DocumentTreeItem::addChild(std::unique_ptr<DocumentTreeItem> child) {
  child->parent_ = this;
  children_.push_back(std::move(child));
}

void DocumentTreeItem::sortChildren() {
  std::ranges::sort(children_, [](const auto& lhs, const auto& rhs) {
    if (lhs->isAction() != rhs->isAction()) {
      return lhs->isAction();
    }
    if (lhs->sortOrder() != rhs->sortOrder()) {
      return lhs->sortOrder() < rhs->sortOrder();
    }
    return lhs->title() < rhs->title();
  });
}

int DocumentTreeItem::childRow(const DocumentTreeItem* child) const {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i].get() == child) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

DocumentTreeItem* DocumentTreeItem::childAt(int row) const {
  if (row < 0 || row >= static_cast<int>(children_.size())) {
    return nullptr;
  }
  return children_[static_cast<size_t>(row)].get();
}

DocumentTreeItem* DocumentTreeItem::findItemById(std::string_view id) {
  if (id_ == id && kind_ == Kind::kDocument) {
    return this;
  }
  for (auto& child : children_) {
    if (auto* found = child->findItemById(id); found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

const DocumentTreeItem* DocumentTreeItem::findItemById(std::string_view id) const {
  if (id_ == id && kind_ == Kind::kDocument) {
    return this;
  }
  for (const auto& child : children_) {
    if (auto* found = child->findItemById(id); found != nullptr) {
      return found;
    }
  }
  return nullptr;
}

// DocumentTreeModel implementation
DocumentTreeModel::DocumentTreeModel(QObject* parent) : QAbstractItemModel(parent) {
  root_item_ = std::make_unique<DocumentTreeItem>(nullptr);
}

DocumentTreeModel::~DocumentTreeModel() = default;

QModelIndex DocumentTreeModel::index(int row, int column, const QModelIndex& parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  DocumentTreeItem* parent_item = itemFromIndex(parent);
  if (!parent_item) {
    return QModelIndex();
  }

  DocumentTreeItem* child_item = parent_item->childAt(row);
  if (child_item) {
    return createIndex(row, column, child_item);
  }

  return QModelIndex();
}

QModelIndex DocumentTreeModel::parent(const QModelIndex& index) const {
  if (!index.isValid()) {
    return QModelIndex();
  }

  auto* child_item = static_cast<DocumentTreeItem*>(index.internalPointer());
  DocumentTreeItem* parent_item = child_item->parent();

  if (!parent_item || parent_item == root_item_.get()) {
    return QModelIndex();
  }

  return createIndex(parent_item->parent()->childRow(parent_item), 0, parent_item);
}

int DocumentTreeModel::rowCount(const QModelIndex& parent) const {
  if (parent.column() > 0) {
    return 0;
  }

  DocumentTreeItem* parent_item = itemFromIndex(parent);
  return parent_item ? parent_item->rowCount() : 0;
}

int DocumentTreeModel::columnCount(const QModelIndex& /*parent*/) const { return 1; }

bool DocumentTreeModel::hasChildren(const QModelIndex& parent) const {
  if (parent.column() > 0) {
    return false;
  }

  const auto* parent_item = itemFromIndex(parent);
  return parent_item != nullptr && parent_item->rowCount() > 0;
}

QVariant DocumentTreeModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());

  switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
      return QString::fromStdString(item->title());

    case Qt::DecorationRole:
      if (item->isAction()) {
        return QIcon::fromTheme("document-new");
      }
      // Return icon based on whether it's a container
      if (item->isContainer()) {
        return QIcon::fromTheme("folder");
      }
      return QIcon::fromTheme("text-x-generic");

    case DocumentTreeModel::kIsContainerRole:
      return item->isContainer();

    case DocumentTreeModel::kIsActionRole:
      return item->isAction();

    case DocumentTreeModel::kAddChildActionRole:
      return item->isAddChildAction();

    case Qt::UserRole:
      // Store document ID for retrieval
      return QString::fromStdString(item->id());

    default:
      return QVariant();
  }
}

Qt::ItemFlags DocumentTreeModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }

  return QAbstractItemModel::flags(index) | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void DocumentTreeModel::setDocuments(const std::vector<storage::DocumentSummary>& documents) {
  beginResetModel();
  buildTree(documents);
  endResetModel();
}

void DocumentTreeModel::appendDocument(const storage::DocumentSummary& document) {
  // Rebuild the tree to keep action items and sort order consistent.
  // A full reset is acceptable here because the document list is small.
  std::vector<storage::DocumentSummary> current_documents;
  std::queue<DocumentTreeItem*> queue;
  queue.push(root_item_.get());
  while (!queue.empty()) {
    auto* item = queue.front();
    queue.pop();
    for (const auto& child : item->children()) {
      if (child->kind() == DocumentTreeItem::Kind::kDocument) {
        storage::DocumentSummary summary;
        summary.id = child->id();
        summary.title = child->title();
        summary.sort_order = child->sortOrder();
        if (auto* parent = child->parent(); parent != nullptr && parent != root_item_.get()) {
          summary.parent_id = parent->id();
        }
        current_documents.push_back(std::move(summary));
      }
      queue.push(child.get());
    }
  }
  current_documents.push_back(document);
  setDocuments(current_documents);
}

void DocumentTreeModel::buildTree(const std::vector<storage::DocumentSummary>& documents) {
  root_item_ = std::make_unique<DocumentTreeItem>(nullptr);

  const auto normalizedParentId = [](const std::optional<std::string>& parent_id)
      -> std::optional<std::string> {
    if (!parent_id.has_value() || parent_id->empty()) {
      return std::nullopt;
    }
    return parent_id;
  };

  // Map document IDs to their summaries, keeping them in input order but
  // keyed for parent lookup.
  std::map<std::string, storage::DocumentSummary> document_map;
  for (const auto& doc : documents) {
    document_map[doc.id] = doc;
  }

  // Map parent_id -> list of children summaries
  std::map<std::optional<std::string>, std::vector<const storage::DocumentSummary*>> children_map;
  std::map<std::string, std::unique_ptr<DocumentTreeItem>> detached_items;

  // Single root-level "New note" action. A separate toolbar button above the
  // tree was removed because it duplicated this first-row action.
  root_item_->addChild(std::make_unique<DocumentTreeItem>(
      DocumentTreeItem::Kind::kRootAction,
      std::string(constants::kNewDocumentActionId),
      std::string(constants::kNewDocumentActionTitle),
      root_item_.get()));

  // Create document items (not attached yet) preserving key order
  for (const auto& [id, summary] : document_map) {
    auto item = std::make_unique<DocumentTreeItem>(summary, nullptr);
    detached_items.emplace(id, std::move(item));
  }

  // Build parent-child map
  for (const auto& [id, summary] : document_map) {
    children_map[normalizedParentId(summary.parent_id)].push_back(&summary);
  }

  // Recursive helper that places items under a parent and decorates each
  // document with an "add child" action item.
  std::function<void(DocumentTreeItem*, const std::optional<std::string>&)> attachChildren =
      [&](DocumentTreeItem* parent_item, const std::optional<std::string>& parent_id) {
        auto it = children_map.find(parent_id);
        if (it == children_map.end()) {
          return;
        }

        std::vector<const storage::DocumentSummary*> children = it->second;
        std::ranges::sort(children, [](const auto* lhs, const auto* rhs) {
          if (lhs->sort_order != rhs->sort_order) {
            return lhs->sort_order < rhs->sort_order;
          }
          return lhs->title < rhs->title;
        });

        for (const auto* child_summary : children) {
          auto node = detached_items.extract(child_summary->id);
          if (node.empty()) {
            continue;
          }

          auto child = std::move(node.mapped());
          auto* child_item = child.get();
          parent_item->addChild(std::move(child));

          // Recursively attach children of this document
          attachChildren(child_item, child_summary->id);
        }
      };

  attachChildren(root_item_.get(), std::nullopt);
}

DocumentTreeItem* DocumentTreeModel::itemFromIndex(const QModelIndex& index) const {
  if (!index.isValid()) {
    return root_item_.get();
  }
  return static_cast<DocumentTreeItem*>(index.internalPointer());
}

QModelIndex DocumentTreeModel::indexForItem(const DocumentTreeItem* item) const {
  if (!item || item == root_item_.get()) {
    return QModelIndex();
  }
  auto* parent = item->parent();
  if (!parent) {
    return QModelIndex();
  }
  return createIndex(parent->childRow(item), 0, const_cast<DocumentTreeItem*>(item));
}

std::optional<std::string> DocumentTreeModel::documentId(const QModelIndex& index) const {
  if (!index.isValid()) {
    return std::nullopt;
  }
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  if (item->isAction()) {
    return std::nullopt;
  }
  if (item->id().empty()) {
    return std::nullopt;
  }
  return item->id();
}

QModelIndex DocumentTreeModel::indexForDocumentId(std::string_view id) const {
  auto* item = root_item_->findItemById(id);
  if (!item) {
    return QModelIndex();
  }
  return indexForItem(item);
}

bool DocumentTreeModel::isContainer(const QModelIndex& index) const {
  if (!index.isValid()) {
    return false;
  }
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  return item->isContainer();
}

void DocumentTreeModel::requestAddChild(const QModelIndex& parent_document_index) {
  if (!parent_document_index.isValid()) {
    return;
  }

  auto* item = static_cast<DocumentTreeItem*>(parent_document_index.internalPointer());
  if (item == nullptr || item->isAction()) {
    return;
  }

  emit addChildRequested(indexForItem(item));
}

}  // namespace cppwiki::gui
