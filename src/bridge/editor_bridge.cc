#include "editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDateTime>
#include <QVariant>

#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/constants.h"
#include "core/qt_string.h"
#include "core/uuid.h"
#include "document/document_validator.h"
#include "storage/local_document_repository.h"

namespace cppwiki::bridge {

namespace {

auto SuccessResponse(const QVariant& result) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("ok"), true},
      {QStringLiteral("result"), result},
  };
}

auto ErrorResponse(const QString& code, const QString& message) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("ok"), false},
      {QStringLiteral("error"),
       QVariantMap{
           {QStringLiteral("code"), code},
           {QStringLiteral("message"), message},
       }},
  };
}

auto BridgeInfo() -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("namespace"), ToQString(constants::kDocumentsBridgeNamespace)},
      {QStringLiteral("methods"),
       QVariantList{
           ToQString(constants::kBridgeMethodGetBridgeInfo),
           ToQString(constants::kBridgeMethodGetInitialDocument),
           ToQString(constants::kBridgeMethodListDocuments),
           ToQString(constants::kBridgeMethodCreateDocument),
           ToQString(constants::kBridgeMethodCreateChildDocument),
           ToQString(constants::kBridgeMethodLoadDocument),
           ToQString(constants::kBridgeMethodOpenDocument),
           ToQString(constants::kBridgeMethodUpdateSnapshot),
       }},
  };
}

auto InlineText(QString text) -> QJsonArray {
  QJsonObject content;
  content.insert(QStringLiteral("type"), QStringLiteral("text"));
  content.insert(QStringLiteral("text"), std::move(text));
  content.insert(QStringLiteral("styles"), QJsonObject{});
  return QJsonArray{content};
}

auto MakeBlock(const QString& id,
               const QString& type,
               const QJsonArray& content,
               QJsonObject props = {}) -> QJsonObject {
  QJsonObject block;
  block.insert(QStringLiteral("id"), id);
  block.insert(QStringLiteral("type"), type);
  if (!props.isEmpty()) {
    block.insert(QStringLiteral("props"), props);
  }
  block.insert(QStringLiteral("content"), content);
  block.insert(QStringLiteral("children"), QJsonArray{});
  return block;
}

auto WelcomeBlocks() -> QJsonArray {
  QJsonObject heading_props;
  heading_props.insert(QStringLiteral("level"), 1);

  return QJsonArray{
      MakeBlock(QString::fromStdString(GenerateUuidString()),
                QStringLiteral("heading"),
                InlineText(ToQString(constants::kDefaultPageTitle)),
                heading_props),
      MakeBlock(QString::fromStdString(GenerateUuidString()),
                QStringLiteral("paragraph"),
                InlineText(ToQString(constants::kDefaultPageBodyText))),
  };
}

auto MakeDocumentSnapshotJson(const std::string& id,
                              const std::string& title,
                              const QJsonArray& blocks) -> QByteArray {
  QJsonObject document;
  document.insert(QStringLiteral("id"), QString::fromStdString(id));
  document.insert(QStringLiteral("schema_version"),
                  static_cast<int>(document::SchemaVersion::kV1));
  document.insert(QStringLiteral("title"), QString::fromStdString(title));
  document.insert(QStringLiteral("blocks"), blocks);
  return QJsonDocument(document).toJson(QJsonDocument::Compact);
}

auto BlocksFromRawSnapshotJson(const std::string& raw_snapshot_json)
    -> std::variant<QVariantList, QString> {
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(raw_snapshot_json), &error);
  if (error.error != QJsonParseError::NoError) {
    return QStringLiteral("Stored document snapshot is not valid JSON: %1").arg(error.errorString());
  }
  if (json.isArray()) {
    return json.array().toVariantList();
  }
  if (json.isObject()) {
    const auto blocks = json.object().value(QStringLiteral("blocks"));
    if (blocks.isArray()) {
      return blocks.toArray().toVariantList();
    }
  }

  return QStringLiteral("Stored document snapshot does not contain a block array.");
}

auto ExtractBlocks(const QByteArray& snapshot_bytes) -> std::variant<QJsonArray, QString> {
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(snapshot_bytes, &error);
  if (error.error != QJsonParseError::NoError) {
    return QStringLiteral("Snapshot payload is not valid JSON: %1").arg(error.errorString());
  }
  if (json.isArray()) {
    return json.array();
  }
  if (json.isObject()) {
    const auto blocks = json.object().value(QStringLiteral("blocks"));
    if (blocks.isArray()) {
      return blocks.toArray();
    }
  }
  return QStringLiteral("Snapshot payload does not contain a block array.");
}

auto OptionalStringToVariant(const std::optional<std::string>& value) -> QVariant {
  if (!value) {
    return QVariant{};
  }
  return QString::fromStdString(*value);
}

auto DocumentSummariesToVariant(const std::vector<storage::DocumentSummary>& documents)
    -> QVariantList {
  QVariantList result;
  result.reserve(static_cast<int>(documents.size()));
  for (const auto& document : documents) {
    result.append(QVariantMap{
        {QStringLiteral("id"), QString::fromStdString(document.id)},
        {QStringLiteral("title"), QString::fromStdString(document.title)},
        {QStringLiteral("parentId"), OptionalStringToVariant(document.parent_id)},
        {QStringLiteral("sortOrder"), document.sort_order},
        {QStringLiteral("createdAt"), QString::fromStdString(document.created_at)},
        {QStringLiteral("updatedAt"), QString::fromStdString(document.updated_at)},
    });
  }
  return result;
}

auto CurrentUtcTimestamp() -> std::string {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}

auto MakeWelcomeRecord() -> storage::DocumentRecord {
  const auto id = GenerateUuidString();
  const auto title = std::string(constants::kDefaultPageTitle);
  const auto now = CurrentUtcTimestamp();
  const auto blocks = WelcomeBlocks();
  const auto raw_snapshot_json = MakeDocumentSnapshotJson(id, title, blocks);

  return storage::DocumentRecord{
      .metadata =
          document::PageMetadata{
              .id = id,
              .schema_version = document::SchemaVersion::kV1,
              .title = title,
              .parent_id = std::nullopt,
              .sort_order = 0,
              .created_at = now,
              .updated_at = now,
          },
      .snapshot =
          document::BlockNoteDocumentSnapshot{
              .id = id,
              .schema_version = static_cast<std::int32_t>(document::SchemaVersion::kV1),
              .title = title,
              .blocks = std::vector<document::BlockNoteBlockSnapshot>{},
          },
      .raw_snapshot_json = std::string(raw_snapshot_json.constData(),
                                       static_cast<std::size_t>(raw_snapshot_json.size())),
  };
}

auto MakeNewDocumentRecord(std::optional<std::string> parent_id = std::nullopt,
                           std::int32_t sort_order = 0) -> storage::DocumentRecord {
  const auto id = GenerateUuidString();
  const auto title = std::string(constants::kNewDocumentTitle);
  const auto now = CurrentUtcTimestamp();

  const auto raw_snapshot_json = MakeDocumentSnapshotJson(id, title, QJsonArray{});

  return storage::DocumentRecord{
      .metadata =
          document::PageMetadata{
              .id = id,
              .schema_version = document::SchemaVersion::kV1,
              .title = title,
              .parent_id = std::move(parent_id),
              .sort_order = sort_order,
              .created_at = now,
              .updated_at = now,
          },
      .snapshot =
          document::BlockNoteDocumentSnapshot{
              .id = id,
              .schema_version = static_cast<std::int32_t>(document::SchemaVersion::kV1),
              .title = title,
              .blocks = std::vector<document::BlockNoteBlockSnapshot>{},
          },
      .raw_snapshot_json = std::string(raw_snapshot_json.constData(),
                                       static_cast<std::size_t>(raw_snapshot_json.size())),
  };
}

auto NextChildSortOrder(std::shared_ptr<storage::LocalDocumentRepository> repository,
                        std::string_view parent_id) -> std::int32_t {
  if (!repository) {
    return 0;
  }
  const auto list_result = repository->ListDocuments();
  std::int32_t max_order = 0;
  bool found_any = false;
  for (const auto& document : list_result.documents) {
    if (document.parent_id && *document.parent_id == parent_id) {
      if (!found_any || document.sort_order > max_order) {
        max_order = document.sort_order;
      }
      found_any = true;
    }
  }
  return found_any ? max_order + 1 : 0;
}

auto ExtractTitle(const document::Document& document, std::string_view fallback_title)
    -> std::string {
  for (const auto& block : document.blocks) {
    if (block.type == document::BlockType::kHeading && block.heading_props &&
        block.heading_props->level == 1 && !block.text_content.empty()) {
      return block.text_content;
    }
  }
  return std::string(fallback_title);
}

auto LoadDocumentRecord(std::shared_ptr<storage::LocalDocumentRepository> repository,
                        const QString& page_id) -> std::variant<storage::DocumentRecord, QVariantMap> {
  auto result = repository->LoadDocument(page_id.toStdString());
  if (!result.document) {
    return ErrorResponse(QStringLiteral("load_failed"),
                         QString::fromStdString(result.error ? result.error->message
                                                             : "Document was not found."));
  }
  return std::move(*result.document);
}

auto DeleteDocumentTree(std::shared_ptr<storage::LocalDocumentRepository> repository,
                        std::string_view root_id) -> std::optional<storage::RepositoryError> {
  auto list_result = repository->ListDocuments();
  if (list_result.error) {
    return list_result.error;
  }

  std::multimap<std::string, std::string> children_by_parent;
  for (const auto& document : list_result.documents) {
    if (document.parent_id) {
      children_by_parent.emplace(*document.parent_id, document.id);
    }
  }

  std::queue<std::string> pending;
  std::vector<std::string> deletion_order;
  pending.push(std::string(root_id));
  while (!pending.empty()) {
    auto current = std::move(pending.front());
    pending.pop();
    deletion_order.push_back(current);
    auto [it, end] = children_by_parent.equal_range(current);
    for (; it != end; ++it) {
      pending.push(it->second);
    }
  }

  for (auto it = deletion_order.rbegin(); it != deletion_order.rend(); ++it) {
    auto delete_result = repository->DeleteDocument(*it);
    if (delete_result.error) {
      return delete_result.error;
    }
  }
  return std::nullopt;
}

}  // namespace

QEditorBridge::QEditorBridge(QObject* parent) : QObject(parent) {}

void QEditorBridge::SetRepository(
    std::shared_ptr<storage::LocalDocumentRepository> repository) {
  repository_ = std::move(repository);
}

void QEditorBridge::RequestOpenDocument(const QString& page_id) {
  emit documentOpenRequested(page_id);
}

void QEditorBridge::ClearCurrentDocumentSelection() {
  current_page_id_.clear();
  emit documentSelectionCleared();
}

QVariantMap QEditorBridge::getBridgeInfo() {
  return SuccessResponse(BridgeInfo());
}

QVariantMap QEditorBridge::getInitialDocument() {
  return SuccessResponse(QVariantList{});
}

QVariantMap QEditorBridge::listDocuments() {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  auto result = repository_->ListDocuments();
  if (result.error) {
    return ErrorResponse(QStringLiteral("list_failed"),
                         QString::fromStdString(result.error->message));
  }

  if (result.documents.empty()) {
    auto welcome = MakeWelcomeRecord();
    const auto save_result = repository_->SaveDocument(welcome);
    if (save_result.error) {
      return ErrorResponse(QStringLiteral("default_page_failed"),
                           QString::fromStdString(save_result.error->message));
    }
    result.documents.push_back(storage::DocumentSummaryFromMetadata(welcome.metadata));
  }

  return SuccessResponse(DocumentSummariesToVariant(result.documents));
}

QVariantMap QEditorBridge::createDocument() {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  auto record = MakeNewDocumentRecord();
  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("create_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  current_page_id_ = QString::fromStdString(record.metadata.id);
  return SuccessResponse(QVariantMap{
      {QStringLiteral("id"), QString::fromStdString(record.metadata.id)},
      {QStringLiteral("title"), QString::fromStdString(record.metadata.title)},
      {QStringLiteral("parentId"), OptionalStringToVariant(record.metadata.parent_id)},
      {QStringLiteral("sortOrder"), record.metadata.sort_order},
      {QStringLiteral("createdAt"), QString::fromStdString(record.metadata.created_at)},
      {QStringLiteral("updatedAt"), QString::fromStdString(record.metadata.updated_at)},
  });
}

QVariantMap QEditorBridge::createChildDocument(const QString& parent_id) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  if (parent_id.isEmpty()) {
    return ErrorResponse(QStringLiteral("invalid_parent"),
                         QStringLiteral("Parent document id is required."));
  }

  const auto sort_order = NextChildSortOrder(repository_, parent_id.toStdString());
  auto record = MakeNewDocumentRecord(parent_id.toStdString(), sort_order);
  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("create_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  current_page_id_ = QString::fromStdString(record.metadata.id);
  return SuccessResponse(QVariantMap{
      {QStringLiteral("id"), QString::fromStdString(record.metadata.id)},
      {QStringLiteral("title"), QString::fromStdString(record.metadata.title)},
      {QStringLiteral("parentId"), OptionalStringToVariant(record.metadata.parent_id)},
      {QStringLiteral("sortOrder"), record.metadata.sort_order},
      {QStringLiteral("createdAt"), QString::fromStdString(record.metadata.created_at)},
      {QStringLiteral("updatedAt"), QString::fromStdString(record.metadata.updated_at)},
  });
}

QVariantMap QEditorBridge::updateDocumentPlacement(const QString& page_id, const QString& parent_id,
                                                   bool has_parent_id, int sort_order) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  auto loaded_or_error = LoadDocumentRecord(repository_, page_id);
  if (std::holds_alternative<QVariantMap>(loaded_or_error)) {
    return std::get<QVariantMap>(std::move(loaded_or_error));
  }

  auto record = std::move(std::get<storage::DocumentRecord>(loaded_or_error));
  record.metadata.parent_id = has_parent_id ? std::make_optional(parent_id.toStdString()) : std::nullopt;
  record.metadata.sort_order = sort_order;
  record.metadata.updated_at = CurrentUtcTimestamp();

  auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("move_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(QVariantMap{
      {QStringLiteral("id"), QString::fromStdString(record.metadata.id)},
      {QStringLiteral("title"), QString::fromStdString(record.metadata.title)},
      {QStringLiteral("parentId"), OptionalStringToVariant(record.metadata.parent_id)},
      {QStringLiteral("sortOrder"), record.metadata.sort_order},
      {QStringLiteral("createdAt"), QString::fromStdString(record.metadata.created_at)},
      {QStringLiteral("updatedAt"), QString::fromStdString(record.metadata.updated_at)},
  });
}

QVariantMap QEditorBridge::deleteDocument(const QString& page_id) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }
  if (page_id.isEmpty()) {
    return ErrorResponse(QStringLiteral("invalid_document"),
                         QStringLiteral("Document id is required."));
  }

  if (const auto error = DeleteDocumentTree(repository_, page_id.toStdString())) {
    return ErrorResponse(QStringLiteral("delete_failed"),
                         QString::fromStdString(error->message));
  }

  if (current_page_id_ == page_id) {
    current_page_id_.clear();
  }
  return SuccessResponse(QVariant{});
}

QVariantMap QEditorBridge::loadDocument(const QString& page_id) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  auto result = repository_->LoadDocument(page_id.toStdString());
  if (!result.document) {
    return ErrorResponse(QStringLiteral("load_failed"),
                         QString::fromStdString(result.error ? result.error->message
                                                             : "Document was not found."));
  }

  auto blocks = BlocksFromRawSnapshotJson(result.document->raw_snapshot_json);
  if (std::holds_alternative<QString>(blocks)) {
    return ErrorResponse(QStringLiteral("invalid_stored_snapshot"), std::get<QString>(blocks));
  }

  current_page_id_ = page_id;
  return SuccessResponse(QVariantMap{
      {QStringLiteral("id"), QString::fromStdString(result.document->metadata.id)},
      {QStringLiteral("title"), QString::fromStdString(result.document->metadata.title)},
      {QStringLiteral("parentId"), OptionalStringToVariant(result.document->metadata.parent_id)},
      {QStringLiteral("sortOrder"), result.document->metadata.sort_order},
      {QStringLiteral("createdAt"), QString::fromStdString(result.document->metadata.created_at)},
      {QStringLiteral("updatedAt"), QString::fromStdString(result.document->metadata.updated_at)},
      {QStringLiteral("blocks"), std::get<QVariantList>(std::move(blocks))},
  });
}

QVariantMap QEditorBridge::openDocument(const QString& page_id) {
  const auto response = loadDocument(page_id);
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    emit documentLoadFailed(page_id, error.value(QStringLiteral("message")).toString());
    return response;
  }

  emit documentLoaded(response.value(QStringLiteral("result")).toMap());
  return response;
}

QVariantMap QEditorBridge::updateSnapshot(const QString& snapshot_json) {
  if (current_page_id_.isEmpty()) {
    return ErrorResponse(QStringLiteral("no_document_selected"),
                         QStringLiteral("No document is selected."));
  }

  const auto snapshot_bytes = snapshot_json.toUtf8();
  const auto validation = document::DocumentValidator::ParseAndValidateSnapshot(snapshot_bytes);
  if (validation.error) {
    spdlog::warn("editor snapshot rejected: {}: {}",
                 document::ToString(validation.error->code),
                 validation.error->message);
    emit saveStatusChanged(current_page_id_, false,
                           QString::fromStdString(validation.error->message));
    return ErrorResponse(ToQString(document::ToString(validation.error->code)),
                         QString::fromStdString(validation.error->message));
  }

  const auto block_count = validation.document->blocks.size();
  spdlog::info("editor snapshot received: bytes={}, blocks={}", snapshot_bytes.size(), block_count);

  // Save to repository if available
  if (repository_) {
    auto blocks = ExtractBlocks(snapshot_bytes);
    if (std::holds_alternative<QString>(blocks)) {
      return ErrorResponse(QStringLiteral("invalid_root"), std::get<QString>(blocks));
    }

    storage::DocumentRecord record;
    if (auto current = repository_->LoadDocument(current_page_id_.toStdString());
        current.document) {
      record.metadata = current.document->metadata;
    }
    record.metadata.id = current_page_id_.toStdString();
    record.metadata.schema_version = document::SchemaVersion::kV1;
    const auto fallback_title = record.metadata.title.empty()
                                    ? std::string_view(constants::kNewDocumentTitle)
                                    : std::string_view(record.metadata.title);
    record.metadata.title = ExtractTitle(*validation.document, fallback_title);
    if (record.metadata.created_at.empty()) {
      record.metadata.created_at = CurrentUtcTimestamp();
    }
    record.metadata.updated_at = CurrentUtcTimestamp();
    record.snapshot = *validation.snapshot;
    record.snapshot.id = record.metadata.id;
    record.snapshot.schema_version = static_cast<std::int32_t>(document::SchemaVersion::kV1);
    record.snapshot.title = record.metadata.title;

    const auto raw_snapshot_json = MakeDocumentSnapshotJson(record.metadata.id,
                                                            record.metadata.title,
                                                            std::get<QJsonArray>(blocks));
    record.raw_snapshot_json =
        std::string(raw_snapshot_json.constData(), static_cast<std::size_t>(raw_snapshot_json.size()));

    auto save_result = repository_->SaveDocument(record);
    if (save_result.error) {
      spdlog::error("Failed to save document: {}", save_result.error->message);
      emit saveStatusChanged(current_page_id_, false,
                             QString::fromStdString(save_result.error->message));
      return ErrorResponse(QStringLiteral("save_failed"),
                           QString::fromStdString(save_result.error->message));
    }

    spdlog::info("Document saved successfully: id={}", record.metadata.id);
    emit saveStatusChanged(current_page_id_, true, QStringLiteral("Saved"));
  } else {
    spdlog::warn("No repository set, skipping save");
  }

  return SuccessResponse(QVariant{});
}

}  // namespace cppwiki::bridge
