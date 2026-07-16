#include "gui/document_tree_model.h"

#include <QIcon>
#include <QMimeData>
#include <algorithm>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <string_view>
#include <utility>

#include "core/constants.h"

namespace cppwiki::gui {

// DocumentTreeItem implementation
DocumentTreeItem::DocumentTreeItem(const storage::DocumentSummary& summary,
                                   DocumentTreeItem* parent)
    : kind_(Kind::kDocument),
      id_(summary.id),
      title_(summary.title),
      workspace_id_(summary.workspace_id),
      sort_order_(summary.sort_order),
      parent_(parent) {}

DocumentTreeItem::DocumentTreeItem(Kind kind, std::string id, std::string title,
                                   DocumentTreeItem* parent)
    : kind_(kind),
      id_(std::move(id)),
      title_(std::move(title)),
      workspace_id_(kind_ == Kind::kWorkspace ? id_ : std::string{}),
      parent_(parent) {}

DocumentTreeItem::DocumentTreeItem(DocumentTreeItem* parent) : parent_(parent) {}

DocumentTreeItem::~DocumentTreeItem() = default;

void DocumentTreeItem::addChild(std::unique_ptr<DocumentTreeItem> child) {
  child->parent_ = this;
  children_.push_back(std::move(child));
}

void DocumentTreeItem::insertChild(int row, std::unique_ptr<DocumentTreeItem> child) {
  child->parent_ = this;
  const auto insert_it = children_.begin() + std::clamp(row, 0, static_cast<int>(children_.size()));
  children_.insert(insert_it, std::move(child));
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

int DocumentTreeModel::columnCount(const QModelIndex& /*parent*/) const {
  return 1;
}

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
    case Qt::EditRole: {
      const auto base_title = QString::fromStdString(item->title());
      if (item->isWorkspace()) {
        const auto decoration =
            workspace_decorations_.value(QString::fromStdString(item->id()), QString{});
        if (!decoration.isEmpty()) {
          return base_title + decoration;
        }
      }
      return base_title;
    }

    case Qt::DecorationRole:
      if (item->isAction()) {
        return QIcon::fromTheme("document-new");
      }
      if (item->isWorkspace()) {
        return QIcon::fromTheme(QStringLiteral("folder-remote"),
                                QIcon::fromTheme(QStringLiteral("folder-network"),
                                                 QIcon::fromTheme(QStringLiteral("folder"))));
      }
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

    case DocumentTreeModel::kIsWorkspaceRole:
      return item->isWorkspace();

    case DocumentTreeModel::kIsLockedRole:
      return item->isDocument() &&
             locked_document_ids_.contains(QString::fromStdString(item->id()));

    case DocumentTreeModel::kIsConflictedRole:
      return item->isDocument() &&
             conflicted_document_ids_.contains(QString::fromStdString(item->id()));

    case DocumentTreeModel::kWorkspaceIdRole:
      return QString::fromStdString(item->workspaceId());

    case Qt::UserRole:
      return QString::fromStdString(item->id());

    default:
      return QVariant();
  }
}

Qt::ItemFlags DocumentTreeModel::flags(const QModelIndex& index) const {
  if (!index.isValid()) {
    return Qt::NoItemFlags;
  }

  auto flags = QAbstractItemModel::flags(index) | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  if (item != nullptr && !item->isAction()) {
    flags |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  }
  return flags;
}

QStringList DocumentTreeModel::mimeTypes() const {
  return {QString::fromLatin1(kDocumentIdMimeType)};
}

QMimeData* DocumentTreeModel::mimeData(const QModelIndexList& indexes) const {
  auto* mime_data = new QMimeData();
  for (const auto& index : indexes) {
    const auto doc_id = documentId(index);
    if (!doc_id) {
      continue;
    }
    mime_data->setData(QString::fromLatin1(kDocumentIdMimeType),
                       QByteArray::fromStdString(*doc_id));
    break;
  }
  return mime_data;
}

bool DocumentTreeModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                        int column, const QModelIndex& parent) const {
  Q_UNUSED(row)
  Q_UNUSED(column)

  if (action != Qt::MoveAction || data == nullptr ||
      !data->hasFormat(QString::fromLatin1(kDocumentIdMimeType))) {
    return false;
  }

  if (!parent.isValid()) {
    return false;
  }

  if (parent.isValid()) {
    const auto* item = static_cast<DocumentTreeItem*>(parent.internalPointer());
    if (item == nullptr || item->isAction()) {
      return false;
    }
  }

  return true;
}

bool DocumentTreeModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
                                     int column, const QModelIndex& parent) {
  Q_UNUSED(column)

  if (!canDropMimeData(data, action, row, column, parent)) {
    return false;
  }

  const auto raw_id = data->data(QString::fromLatin1(kDocumentIdMimeType));
  const auto source_document_id = QString::fromUtf8(raw_id);
  if (source_document_id.isEmpty()) {
    return false;
  }

  QString target_parent_id;
  bool has_parent_id = false;
  int target_sort_order = row;
  QString workspace_id;

  if (parent.isValid()) {
    const auto* item = static_cast<DocumentTreeItem*>(parent.internalPointer());
    if (item == nullptr || item->isAction()) {
      return false;
    }

    workspace_id = QString::fromStdString(item->workspaceId());
    if (item->isDocument()) {
      target_parent_id = QString::fromStdString(item->id());
      has_parent_id = true;
    }

    if (row < 0) {
      target_sort_order = item->rowCount();
    }
  }

  if (workspace_id.isEmpty()) {
    return false;
  }

  emit documentMoveRequested(source_document_id, target_parent_id, has_parent_id, target_sort_order,
                             workspace_id);
  return true;
}

Qt::DropActions DocumentTreeModel::supportedDropActions() const {
  return Qt::MoveAction;
}

void DocumentTreeModel::setWorkspaces(const QStringList& workspace_ids) {
  QStringList normalized_workspace_ids;
  for (const auto& workspace_id : workspace_ids) {
    const auto normalized =
        workspace_id.trimmed().isEmpty() ? QStringLiteral("default") : workspace_id.trimmed();
    if (!normalized_workspace_ids.contains(normalized)) {
      normalized_workspace_ids.push_back(normalized);
    }
  }

  if (normalized_workspace_ids.isEmpty()) {
    normalized_workspace_ids.push_back(QStringLiteral("default"));
  }

  workspace_ids_ = std::move(normalized_workspace_ids);
}

void DocumentTreeModel::setDocuments(const std::vector<storage::DocumentSummary>& documents) {
  beginResetModel();
  buildTree(documents);
  endResetModel();
}

void DocumentTreeModel::appendDocument(const storage::DocumentSummary& document) {
  auto* workspace_item = findWorkspaceItem(document.workspace_id);
  if (workspace_item == nullptr) {
    beginResetModel();
    root_item_->addChild(std::make_unique<DocumentTreeItem>(
        DocumentTreeItem::Kind::kWorkspace, document.workspace_id, document.workspace_id,
        root_item_.get()));
    root_item_->sortChildren();
    workspace_item = findWorkspaceItem(document.workspace_id);
    endResetModel();
  }

  auto* parent_item = workspace_item;
  QModelIndex parent_index;

  if (document.parent_id.has_value() && !document.parent_id->empty()) {
    parent_item =
        workspace_item != nullptr ? workspace_item->findItemById(*document.parent_id) : nullptr;
    if (parent_item == nullptr) {
      parent_item = workspace_item;
    } else {
      parent_index = indexForItem(parent_item);
    }
  } else if (workspace_item != nullptr) {
    parent_index = indexForItem(workspace_item);
  }

  const auto insert_row = [&]() -> int {
    int row = 0;
    for (const auto& child : parent_item->children()) {
      if (child->kind() != DocumentTreeItem::Kind::kDocument) {
        continue;
      }
      if (child->sortOrder() > document.sort_order) {
        break;
      }
      if (child->sortOrder() == document.sort_order && child->title() > document.title) {
        break;
      }
      ++row;
    }
    return row;
  }();

  beginInsertRows(parent_index, insert_row, insert_row);
  parent_item->insertChild(insert_row, std::make_unique<DocumentTreeItem>(document, parent_item));
  endInsertRows();
}

void DocumentTreeModel::buildTree(const std::vector<storage::DocumentSummary>& documents) {
  root_item_ = std::make_unique<DocumentTreeItem>(nullptr);

  const auto normalizedParentId =
      [](const std::optional<std::string>& parent_id) -> std::optional<std::string> {
    if (!parent_id.has_value() || parent_id->empty()) {
      return std::nullopt;
    }
    return parent_id;
  };

  std::map<std::string, std::vector<storage::DocumentSummary>> workspace_documents;
  for (const auto& workspace_id : workspace_ids_) {
    const auto normalized =
        workspace_id.trimmed().isEmpty() ? QStringLiteral("default") : workspace_id.trimmed();
    workspace_documents.try_emplace(normalized.toStdString());
  }

  for (const auto& doc : documents) {
    if (doc.id.empty()) {
      continue;
    }
    const auto workspace_id = doc.workspace_id.empty() ? std::string{"default"} : doc.workspace_id;
    workspace_documents[workspace_id].push_back(doc);
  }

  for (auto& [workspace_id, workspace_docs] : workspace_documents) {
    auto workspace_root = std::make_unique<DocumentTreeItem>(
        DocumentTreeItem::Kind::kWorkspace, workspace_id, workspace_id, root_item_.get());

    std::map<std::string, storage::DocumentSummary> document_map;
    for (const auto& doc : workspace_docs) {
      document_map[doc.id] = doc;
    }

    std::map<std::optional<std::string>, std::vector<const storage::DocumentSummary*>> children_map;
    std::map<std::string, std::unique_ptr<DocumentTreeItem>> detached_items;

    for (const auto& [id, summary] : document_map) {
      detached_items.emplace(id, std::make_unique<DocumentTreeItem>(summary, nullptr));
      children_map[normalizedParentId(summary.parent_id)].push_back(&summary);
    }

    const auto sortByPlacement = [](auto& children) {
      std::ranges::sort(children, [](const auto* lhs, const auto* rhs) {
        if (lhs->sort_order != rhs->sort_order) {
          return lhs->sort_order < rhs->sort_order;
        }
        return lhs->title < rhs->title;
      });
    };

    std::set<std::string> recursion_stack;
    std::function<void(DocumentTreeItem*, const std::optional<std::string>&)> attachChildren =
        [&](DocumentTreeItem* parent_item, const std::optional<std::string>& parent_id) {
          auto it = children_map.find(parent_id);
          if (it == children_map.end()) {
            return;
          }

          auto children = it->second;
          sortByPlacement(children);

          for (const auto* child_summary : children) {
            if (child_summary == nullptr || recursion_stack.contains(child_summary->id)) {
              continue;
            }

            auto node = detached_items.extract(child_summary->id);
            if (node.empty()) {
              continue;
            }

            auto child = std::move(node.mapped());
            auto* child_item = child.get();
            parent_item->addChild(std::move(child));

            recursion_stack.insert(child_summary->id);
            attachChildren(child_item, child_summary->id);
            recursion_stack.erase(child_summary->id);
          }
        };

    attachChildren(workspace_root.get(), std::nullopt);

    while (!detached_items.empty()) {
      auto node = detached_items.extract(detached_items.begin());
      auto* child_item = node.mapped().get();
      const auto child_id = node.key();
      workspace_root->addChild(std::move(node.mapped()));

      recursion_stack.insert(child_id);
      attachChildren(child_item, child_id);
      recursion_stack.erase(child_id);
    }

    workspace_root->sortChildren();
    root_item_->addChild(std::move(workspace_root));
  }

  root_item_->sortChildren();
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

  const auto row = parent->childRow(item);
  if (row < 0) {
    return QModelIndex();
  }

  return createIndex(row, 0, const_cast<DocumentTreeItem*>(item));
}

std::optional<std::string> DocumentTreeModel::documentId(const QModelIndex& index) const {
  if (!index.isValid()) {
    return std::nullopt;
  }
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  if (item->isAction() || !item->isDocument()) {
    return std::nullopt;
  }
  if (item->id().empty()) {
    return std::nullopt;
  }
  return item->id();
}

std::optional<std::string> DocumentTreeModel::workspaceId(const QModelIndex& index) const {
  if (!index.isValid()) {
    return std::nullopt;
  }
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  if (item == nullptr || item->workspaceId().empty()) {
    return std::nullopt;
  }
  return item->workspaceId();
}

QModelIndex DocumentTreeModel::indexForDocumentId(std::string_view id) const {
  auto* item = root_item_->findItemById(id);
  if (!item) {
    return QModelIndex();
  }
  return indexForItem(item);
}

QModelIndex DocumentTreeModel::indexForWorkspaceId(std::string_view workspace_id) const {
  auto* item = findWorkspaceItem(workspace_id);
  return indexForItem(item);
}

bool DocumentTreeModel::isContainer(const QModelIndex& index) const {
  if (!index.isValid()) {
    return false;
  }
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  return item->isContainer();
}

bool DocumentTreeModel::isWorkspace(const QModelIndex& index) const {
  if (!index.isValid()) {
    return false;
  }
  auto* item = static_cast<DocumentTreeItem*>(index.internalPointer());
  return item != nullptr && item->isWorkspace();
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

DocumentTreeItem* DocumentTreeModel::findWorkspaceItem(std::string_view workspace_id) const {
  if (root_item_ == nullptr) {
    return nullptr;
  }

  for (const auto& child : root_item_->children()) {
    if (child != nullptr && child->isWorkspace() && child->id() == workspace_id) {
      return child.get();
    }
  }
  return nullptr;
}

void DocumentTreeModel::setWorkspaceDecoration(const QString& workspace_id,
                                               const QString& decoration) {
  const auto had_decoration = !workspace_decorations_.value(workspace_id, QString{}).isEmpty();
  if (decoration.isEmpty()) {
    workspace_decorations_.remove(workspace_id);
  } else {
    workspace_decorations_[workspace_id] = decoration;
  }

  if (had_decoration == decoration.isEmpty()) {
    // Emit dataChanged for the workspace row so the view re-renders the title.
    if (auto* workspace = findWorkspaceItem(workspace_id.toStdString()); workspace != nullptr) {
      const auto index = indexForItem(workspace);
      if (index.isValid()) {
        emit dataChanged(index, index, {Qt::DisplayRole});
      }
    }
  }
}

QString DocumentTreeModel::workspaceDecoration(const QString& workspace_id) const {
  return workspace_decorations_.value(workspace_id, QString{});
}

void DocumentTreeModel::setLockedDocumentIds(const QSet<QString>& document_ids) {
  if (locked_document_ids_ == document_ids) {
    return;
  }
  locked_document_ids_ = document_ids;
  emit layoutChanged();
}

void DocumentTreeModel::setConflictedDocumentIds(const QSet<QString>& document_ids) {
  if (conflicted_document_ids_ == document_ids) {
    return;
  }
  conflicted_document_ids_ = document_ids;
  emit layoutChanged();
}

}  // namespace cppwiki::gui
