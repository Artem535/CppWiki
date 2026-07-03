#include "gui/merge/conflict_merge_model.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QVariant>

#include <algorithm>

#include "core/uuid.h"
#include "document/document_validator.h"

namespace cppwiki::gui::merge {

namespace {

auto StringOrEmpty(const QJsonValue& value) -> QString {
  return value.isString() ? value.toString() : QString{};
}

auto InlineTextFromContent(const QJsonValue& content) -> QString {
  if (content.isString()) {
    return content.toString();
  }
  if (content.isObject()) {
    const auto object = content.toObject();
    if (object.value(QStringLiteral("type")).toString() == QStringLiteral("text")) {
      return object.value(QStringLiteral("text")).toString();
    }
    return {};
  }
  if (!content.isArray()) {
    return {};
  }

  QStringList parts;
  for (const auto& entry : content.toArray()) {
    const auto text = InlineTextFromContent(entry).trimmed();
    if (!text.isEmpty()) {
      parts.push_back(text);
    }
  }
  return parts.join(QStringLiteral(" "));
}

struct OrderedBlock {
  QJsonObject block;
  double order_key{0.0};
  int insertion_order{0};
};

auto NormalizeRank(qsizetype rank, qsizetype count) -> double {
  if (count <= 0) {
    return 0.0;
  }
  return static_cast<double>(rank) / static_cast<double>(count);
}

auto BlockIndexById(const QJsonObject& document) -> QHash<QString, int> {
  QHash<QString, int> indexes;
  const auto blocks = document.value(QStringLiteral("blocks")).toArray();
  for (int index = 0; index < blocks.size(); ++index) {
    if (!blocks.at(index).isObject()) {
      continue;
    }
    const auto id = StringOrEmpty(blocks.at(index).toObject().value(QStringLiteral("id"))).trimmed();
    if (!id.isEmpty()) {
      indexes.insert(id, index);
    }
  }
  return indexes;
}

}  // namespace

ConflictMergeModel::ConflictMergeModel(QObject* parent) : QAbstractListModel(parent) {}

auto ConflictMergeModel::rowCount(const QModelIndex& parent) const -> int {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rows_.size());
}

auto ConflictMergeModel::data(const QModelIndex& index, int role) const -> QVariant {
  if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
    return {};
  }

  const auto& row = rows_.at(static_cast<std::size_t>(index.row()));
  switch (role) {
    case BlockIdRole:
      return row.block_id;
    case BlockTypeRole:
      return row.block_type;
    case BlockLabelRole:
      return row.block_label;
    case LocalPreviewRole:
      return row.local_preview;
    case RemotePreviewRole:
      return row.remote_preview;
    case ResolutionRole:
      return row.resolution;
    case IsConflictRole:
      return row.is_conflict;
    case HasLocalRole:
      return row.has_local;
    case HasRemoteRole:
      return row.has_remote;
    default:
      return {};
  }
}

auto ConflictMergeModel::roleNames() const -> QHash<int, QByteArray> {
  return {
      {BlockIdRole, "blockId"},
      {BlockTypeRole, "blockType"},
      {BlockLabelRole, "blockLabel"},
      {LocalPreviewRole, "localPreview"},
      {RemotePreviewRole, "remotePreview"},
      {ResolutionRole, "resolution"},
      {IsConflictRole, "isConflict"},
      {HasLocalRole, "hasLocal"},
      {HasRemoteRole, "hasRemote"},
  };
}

auto ConflictMergeModel::LoadConflict(
    const storage::DocumentConflictRecord& conflict,
    const std::optional<storage::DocumentRecord>& current_document) -> bool {
  QString error_message;
  const auto local_document = ParseSnapshotObject(conflict.local_snapshot, &error_message);
  if (!local_document.has_value()) {
    SetError(QStringLiteral("Local snapshot: %1").arg(error_message));
    return false;
  }

  const auto remote_document = ParseSnapshotObject(conflict.remote_snapshot, &error_message);
  if (!remote_document.has_value()) {
    SetError(QStringLiteral("Remote snapshot: %1").arg(error_message));
    return false;
  }

  const auto local_blocks = BlocksFromSnapshot(*local_document);
  const auto remote_blocks = BlocksFromSnapshot(*remote_document);
  std::vector<Row> rows;
  rows.reserve(local_blocks.size() + remote_blocks.size());

  QHash<QString, qsizetype> remote_indexes_by_id;
  for (qsizetype remote_index = 0; remote_index < static_cast<qsizetype>(remote_blocks.size());
       ++remote_index) {
    const auto remote_id =
        ExplicitBlockId(remote_blocks.at(static_cast<std::size_t>(remote_index)));
    if (!remote_id.isEmpty()) {
      remote_indexes_by_id.insert(remote_id, remote_index);
    }
  }

  std::vector<bool> remote_rows_used(remote_blocks.size(), false);
  for (qsizetype local_index = 0; local_index < static_cast<qsizetype>(local_blocks.size());
       ++local_index) {
    Row row;
    row.has_local = true;
    row.local_block = local_blocks.at(static_cast<std::size_t>(local_index));

    const auto local_explicit_id = ExplicitBlockId(row.local_block);
    qsizetype matching_remote_index = -1;
    if (!local_explicit_id.isEmpty()) {
      const auto remote_it = remote_indexes_by_id.constFind(local_explicit_id);
      if (remote_it != remote_indexes_by_id.cend()) {
        matching_remote_index = *remote_it;
      }
    } else if (local_index < static_cast<qsizetype>(remote_blocks.size()) &&
               !remote_rows_used.at(static_cast<std::size_t>(local_index)) &&
               ExplicitBlockId(remote_blocks.at(static_cast<std::size_t>(local_index))).isEmpty()) {
      matching_remote_index = local_index;
    }

    if (matching_remote_index >= 0 &&
        !remote_rows_used.at(static_cast<std::size_t>(matching_remote_index))) {
      row.has_remote = true;
      row.remote_block = remote_blocks.at(static_cast<std::size_t>(matching_remote_index));
      remote_rows_used[static_cast<std::size_t>(matching_remote_index)] = true;
    }

    const auto local_id = BlockIdForBlock(row.local_block, local_index);
    const auto remote_id =
        row.has_remote ? BlockIdForBlock(row.remote_block, matching_remote_index) : QString{};
    row.block_id = !local_id.isEmpty() ? local_id : remote_id;
    if (row.block_id.isEmpty()) {
      row.block_id = QStringLiteral("block-%1").arg(local_index);
    }

    row.block_type = row.has_local ? BlockTypeForBlock(row.local_block)
                                   : BlockTypeForBlock(row.remote_block);
    row.block_label = BlockLabelForBlock(row.has_local ? row.local_block : row.remote_block,
                                         local_index);
    row.local_preview = row.has_local ? PreviewTextForBlock(row.local_block)
                                      : QStringLiteral("(missing)");
    row.remote_preview = row.has_remote ? PreviewTextForBlock(row.remote_block)
                                        : QStringLiteral("(missing)");
    row.is_conflict = !row.has_local || !row.has_remote ||
                      !BlocksEquivalent(row.local_block, row.remote_block);
    row.resolution = row.has_local ? QStringLiteral("local") : QStringLiteral("remote");
    rows.push_back(std::move(row));
  }

  for (qsizetype remote_index = 0; remote_index < static_cast<qsizetype>(remote_blocks.size());
       ++remote_index) {
    if (remote_rows_used.at(static_cast<std::size_t>(remote_index))) {
      continue;
    }

    Row row;
    row.has_remote = true;
    row.remote_block = remote_blocks.at(static_cast<std::size_t>(remote_index));
    row.block_id = BlockIdForBlock(row.remote_block, remote_index);
    row.block_type = BlockTypeForBlock(row.remote_block);
    row.block_label = BlockLabelForBlock(row.remote_block, static_cast<qsizetype>(rows.size()));
    row.local_preview = QStringLiteral("(missing)");
    row.remote_preview = PreviewTextForBlock(row.remote_block);
    row.is_conflict = true;
    row.resolution = QStringLiteral("remote");
    rows.push_back(std::move(row));
  }

  beginResetModel();
  Clear();
  document_id_ = QString::fromStdString(conflict.document_id);
  workspace_id_ = QString::fromStdString(conflict.workspace_id);
  conflict_title_ =
      QStringLiteral("Conflict: %1 in %2").arg(document_id_, workspace_id_);
  local_document_ = *local_document;
  remote_document_ = *remote_document;
  rows_ = std::move(rows);

  if (current_document.has_value()) {
    const auto& metadata = current_document->metadata;
    if (conflict_title_.isEmpty()) {
      conflict_title_ = QString::fromStdString(metadata.title);
    }
  }

  endResetModel();
  return true;
}

auto ConflictMergeModel::HasContent() const -> bool {
  return !rows_.empty() && error_message_.isEmpty();
}

auto ConflictMergeModel::ErrorMessage() const -> QString { return error_message_; }

auto ConflictMergeModel::ConflictTitle() const -> QString { return conflict_title_; }

auto ConflictMergeModel::WorkspaceId() const -> QString { return workspace_id_; }

auto ConflictMergeModel::DocumentId() const -> QString { return document_id_; }

auto ConflictMergeModel::BuildMergedBlocks() const -> QJsonArray {
  if (PreferRemoteDocument()) {
    return remote_document_.value(QStringLiteral("blocks")).toArray();
  }
  if (PreferLocalDocument()) {
    return local_document_.value(QStringLiteral("blocks")).toArray();
  }

  const auto local_indexes = BlockIndexById(local_document_);
  const auto remote_indexes = BlockIndexById(remote_document_);
  const auto local_block_count =
      static_cast<qsizetype>(local_document_.value(QStringLiteral("blocks")).toArray().size());
  const auto remote_block_count =
      static_cast<qsizetype>(remote_document_.value(QStringLiteral("blocks")).toArray().size());

  std::vector<OrderedBlock> ordered_blocks;
  ordered_blocks.reserve(rows_.size() * 2);
  int insertion_order = 0;
  for (const auto& row : rows_) {
    const auto choice = row.resolution.trimmed().toLower();
    const auto local_index = local_indexes.value(row.block_id, -1);
    const auto remote_index = remote_indexes.value(row.block_id, -1);
    if (choice == QStringLiteral("remote")) {
      if (row.has_remote) {
        ordered_blocks.push_back(
            OrderedBlock{.block = row.remote_block,
                         .order_key = NormalizeRank(
                             remote_index >= 0 ? remote_index
                                               : static_cast<qsizetype>(insertion_order),
                             remote_block_count),
                         .insertion_order = insertion_order++});
      } else if (row.has_local) {
        ordered_blocks.push_back(
            OrderedBlock{.block = row.local_block,
                         .order_key = NormalizeRank(
                             local_index >= 0 ? local_index
                                              : static_cast<qsizetype>(insertion_order),
                             local_block_count),
                         .insertion_order = insertion_order++});
      }
      continue;
    }

    if (choice == QStringLiteral("both")) {
      if (row.has_local) {
        ordered_blocks.push_back(
            OrderedBlock{.block = row.local_block,
                         .order_key = NormalizeRank(
                             local_index >= 0 ? local_index
                                              : static_cast<qsizetype>(insertion_order),
                             local_block_count) +
                                      0.1,
                         .insertion_order = insertion_order++});
      }
      if (row.has_remote) {
        ordered_blocks.push_back(
            OrderedBlock{.block = CloneBlockWithFreshId(row.remote_block),
                         .order_key = NormalizeRank(
                             remote_index >= 0 ? remote_index
                                               : static_cast<qsizetype>(insertion_order),
                             remote_block_count) +
                                      0.2,
                         .insertion_order = insertion_order++});
      }
      continue;
    }

    if (row.has_local) {
      ordered_blocks.push_back(
          OrderedBlock{.block = row.local_block,
                       .order_key = NormalizeRank(
                           local_index >= 0 ? local_index
                                            : static_cast<qsizetype>(insertion_order),
                           local_block_count) + 0.1,
                       .insertion_order = insertion_order++});
    } else if (row.has_remote) {
      ordered_blocks.push_back(
          OrderedBlock{.block = row.remote_block,
                       .order_key = NormalizeRank(
                           remote_index >= 0 ? remote_index
                                             : static_cast<qsizetype>(insertion_order),
                           remote_block_count),
                       .insertion_order = insertion_order++});
    }
  }

  std::stable_sort(ordered_blocks.begin(), ordered_blocks.end(),
                   [](const OrderedBlock& lhs, const OrderedBlock& rhs) {
                     if (lhs.order_key == rhs.order_key) {
                       return lhs.insertion_order < rhs.insertion_order;
                     }
                     return lhs.order_key < rhs.order_key;
                   });

  QJsonArray merged_blocks;
  for (const auto& ordered_block : ordered_blocks) {
    merged_blocks.push_back(ordered_block.block);
  }
  return merged_blocks;
}

auto ConflictMergeModel::BuildMergedDocument() const -> std::optional<QJsonObject> {
  if (!HasContent()) {
    return std::nullopt;
  }

  const auto base_document = PreferredBaseDocument();
  if (base_document.isEmpty()) {
    return std::nullopt;
  }

  QJsonObject merged_document = base_document;
  merged_document.insert(QStringLiteral("id"), document_id_);
  merged_document.insert(QStringLiteral("schema_version"),
                         static_cast<int>(document::SchemaVersion::kV1));
  merged_document.insert(QStringLiteral("title"),
                         base_document.value(QStringLiteral("title")).toString());
  merged_document.insert(QStringLiteral("blocks"), BuildMergedBlocks());
  return merged_document;
}

auto ConflictMergeModel::PreferredBaseDocument() const -> QJsonObject {
  if (PreferRemoteDocument()) {
    return remote_document_;
  }
  if (!local_document_.isEmpty()) {
    return local_document_;
  }
  return remote_document_;
}

auto ConflictMergeModel::PreferRemoteDocument() const -> bool {
  if (rows_.empty()) {
    return false;
  }

  for (const auto& row : rows_) {
    const auto choice = row.resolution.trimmed().toLower();
    if (choice == QStringLiteral("both")) {
      return false;
    }
    if (row.has_remote && choice == QStringLiteral("remote")) {
      continue;
    }
    if (!row.has_local && row.has_remote && choice == QStringLiteral("remote")) {
      continue;
    }
    return false;
  }

  return !remote_document_.isEmpty();
}

auto ConflictMergeModel::PreferLocalDocument() const -> bool {
  if (rows_.empty()) {
    return false;
  }

  for (const auto& row : rows_) {
    const auto choice = row.resolution.trimmed().toLower();
    if (choice == QStringLiteral("both")) {
      return false;
    }
    if (row.has_local && choice == QStringLiteral("local")) {
      continue;
    }
    if (!row.has_remote && row.has_local && choice == QStringLiteral("local")) {
      continue;
    }
    return false;
  }

  return !local_document_.isEmpty();
}

void ConflictMergeModel::SetResolution(int row, const QString& resolution) {
  if (row < 0 || row >= rowCount()) {
    return;
  }

  auto& target = rows_.at(static_cast<std::size_t>(row));
  if (target.resolution == resolution) {
    return;
  }

  target.resolution = resolution;
  const auto index = createIndex(row, 0);
  emit dataChanged(index, index, {ResolutionRole});
}

void ConflictMergeModel::SetResolutionForAll(const QString& resolution) {
  if (rows_.empty()) {
    return;
  }

  for (auto& row : rows_) {
    row.resolution = resolution;
  }
  emit dataChanged(createIndex(0, 0), createIndex(rowCount() - 1, 0), {ResolutionRole});
}

auto ConflictMergeModel::ResolutionForRow(int row) const -> QString {
  if (row < 0 || row >= rowCount()) {
    return {};
  }
  return rows_.at(static_cast<std::size_t>(row)).resolution;
}

void ConflictMergeModel::Clear() {
  rows_.clear();
  conflict_title_.clear();
  workspace_id_.clear();
  document_id_.clear();
  error_message_.clear();
  local_document_ = QJsonObject{};
  remote_document_ = QJsonObject{};
}

void ConflictMergeModel::SetError(QString message) {
  beginResetModel();
  Clear();
  error_message_ = std::move(message);
  endResetModel();
}

auto ConflictMergeModel::PreviewTextForBlock(const QJsonObject& block) -> QString {
  const auto type = BlockTypeForBlock(block);
  const auto content = block.value(QStringLiteral("content"));
  const auto text = InlineTextFromContent(content).trimmed();
  if (text.isEmpty()) {
    return QStringLiteral("(%1)").arg(type);
  }
  return QStringLiteral("%1: %2").arg(type, text);
}

auto ConflictMergeModel::BlockTypeForBlock(const QJsonObject& block) -> QString {
  const auto type = StringOrEmpty(block.value(QStringLiteral("type")));
  return type.isEmpty() ? QStringLiteral("unknown") : type;
}

auto ConflictMergeModel::BlockLabelForBlock(const QJsonObject& block, qsizetype row_index)
    -> QString {
  const auto preview = PreviewTextForBlock(block);
  return QStringLiteral("%1. %2").arg(QString::number(row_index + 1), preview);
}

auto ConflictMergeModel::ParseSnapshotObject(const std::string& raw_snapshot_json,
                                             QString* error_message) -> std::optional<QJsonObject> {
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(raw_snapshot_json), &error);
  if (error.error != QJsonParseError::NoError || !json.isObject()) {
    if (error_message != nullptr) {
      *error_message = error.error == QJsonParseError::NoError
                           ? QStringLiteral("snapshot root is not an object")
                           : error.errorString();
    }
    return std::nullopt;
  }

  return json.object();
}

auto ConflictMergeModel::BlockIdForBlock(const QJsonObject& block, qsizetype row_index) -> QString {
  const auto id = ExplicitBlockId(block);
  if (!id.isEmpty()) {
    return id;
  }
  return QStringLiteral("block-%1").arg(row_index);
}

auto ConflictMergeModel::ExplicitBlockId(const QJsonObject& block) -> QString {
  return StringOrEmpty(block.value(QStringLiteral("id"))).trimmed();
}

auto ConflictMergeModel::BlocksFromSnapshot(const QJsonObject& snapshot)
    -> std::vector<QJsonObject> {
  std::vector<QJsonObject> blocks;
  const auto value = snapshot.value(QStringLiteral("blocks"));
  if (!value.isArray()) {
    return blocks;
  }

  const auto array = value.toArray();
  blocks.reserve(static_cast<std::size_t>(array.size()));
  for (const auto& entry : array) {
    if (entry.isObject()) {
      blocks.push_back(entry.toObject());
    }
  }
  return blocks;
}

auto ConflictMergeModel::BlocksEquivalent(const QJsonObject& lhs, const QJsonObject& rhs) -> bool {
  return lhs == rhs;
}

auto ConflictMergeModel::CloneBlockWithFreshId(QJsonObject block) -> QJsonObject {
  block.insert(QStringLiteral("id"), QString::fromStdString(GenerateUuidString()));
  return block;
}

}  // namespace cppwiki::gui::merge
