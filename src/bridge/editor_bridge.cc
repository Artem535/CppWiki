#include "editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
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
           ToQString(constants::kBridgeMethodRenameDocument),
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

auto MakeBlock(const QString& id, const QString& type, const QJsonArray& content,
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
      MakeBlock(QString::fromStdString(GenerateUuidString()), QStringLiteral("heading"),
                InlineText(ToQString(constants::kDefaultPageTitle)), heading_props),
      MakeBlock(QString::fromStdString(GenerateUuidString()), QStringLiteral("paragraph"),
                InlineText(ToQString(constants::kDefaultPageBodyText))),
  };
}

auto MakeDocumentSnapshotJson(const std::string& id, const std::string& title,
                              const QJsonArray& blocks) -> QByteArray {
  QJsonObject document;
  document.insert(QStringLiteral("id"), QString::fromStdString(id));
  document.insert(QStringLiteral("schema_version"), static_cast<int>(document::SchemaVersion::kV1));
  document.insert(QStringLiteral("title"), QString::fromStdString(title));
  document.insert(QStringLiteral("blocks"), blocks);
  return QJsonDocument(document).toJson(QJsonDocument::Compact);
}

auto BlocksFromRawSnapshotJson(const std::string& raw_snapshot_json)
    -> std::variant<QVariantList, QString> {
  QJsonParseError error;
  const auto json = QJsonDocument::fromJson(QByteArray::fromStdString(raw_snapshot_json), &error);
  if (error.error != QJsonParseError::NoError) {
    return QStringLiteral("Stored document snapshot is not valid JSON: %1")
        .arg(error.errorString());
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

auto NormalizeWorkspaceId(QString workspace_id) -> QString {
  const auto trimmed = workspace_id.trimmed();
  return trimmed.isEmpty() ? QStringLiteral("default") : trimmed;
}

auto EffectiveWorkspaceId(const std::string& workspace_id) -> std::string {
  return workspace_id.empty() ? std::string("default") : workspace_id;
}

auto ProcessAuthorId() -> std::string {
  const auto environment = QProcessEnvironment::systemEnvironment();
  const auto author = environment.value(QStringLiteral("USER"));
  if (!author.isEmpty()) {
    return author.toStdString();
  }

  const auto username = environment.value(QStringLiteral("USERNAME"));
  if (!username.isEmpty()) {
    return username.toStdString();
  }

  return "unknown";
}

auto EffectiveAuthorId(const QString& author_id) -> std::string {
  const auto trimmed = author_id.trimmed();
  if (!trimmed.isEmpty()) {
    return trimmed.toStdString();
  }
  return ProcessAuthorId();
}

auto MetadataToVariant(const document::PageMetadata& metadata) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("id"), QString::fromStdString(metadata.id)},
      {QStringLiteral("title"), QString::fromStdString(metadata.title)},
      {QStringLiteral("parentId"), OptionalStringToVariant(metadata.parent_id)},
      {QStringLiteral("sortOrder"), metadata.sort_order},
      {QStringLiteral("workspaceId"),
       QString::fromStdString(EffectiveWorkspaceId(metadata.workspace_id))},
      {QStringLiteral("createdBy"), QString::fromStdString(metadata.created_by)},
      {QStringLiteral("updatedBy"), QString::fromStdString(metadata.updated_by)},
      {QStringLiteral("contentVersion"), static_cast<qlonglong>(metadata.content_version)},
      {QStringLiteral("createdAt"), QString::fromStdString(metadata.created_at)},
      {QStringLiteral("updatedAt"), QString::fromStdString(metadata.updated_at)},
  };
}

auto DocumentSummariesToVariant(const std::vector<storage::DocumentSummary>& documents,
                                const QString& current_workspace_id) -> QVariantList {
  QVariantList result;
  for (const auto& document : documents) {
    const auto workspace_id = QString::fromStdString(EffectiveWorkspaceId(document.workspace_id));
    if (workspace_id != current_workspace_id) {
      continue;
    }

    result.append(QVariantMap{
        {QStringLiteral("id"), QString::fromStdString(document.id)},
        {QStringLiteral("title"), QString::fromStdString(document.title)},
        {QStringLiteral("parentId"), OptionalStringToVariant(document.parent_id)},
        {QStringLiteral("sortOrder"), document.sort_order},
        {QStringLiteral("workspaceId"), workspace_id},
        {QStringLiteral("createdBy"), QString::fromStdString(document.created_by)},
        {QStringLiteral("updatedBy"), QString::fromStdString(document.updated_by)},
        {QStringLiteral("contentVersion"), static_cast<qlonglong>(document.content_version)},
        {QStringLiteral("createdAt"), QString::fromStdString(document.created_at)},
        {QStringLiteral("updatedAt"), QString::fromStdString(document.updated_at)},
    });
  }
  return result;
}

auto CurrentUtcTimestamp() -> std::string {
  return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
}

void ApplyDocumentMutationAudit(document::PageMetadata& metadata, const QString& author_id) {
  metadata.updated_at = CurrentUtcTimestamp();
  metadata.updated_by = EffectiveAuthorId(author_id);
  if (metadata.content_version < 1) {
    metadata.content_version = 1;
  } else {
    ++metadata.content_version;
  }
}

auto MakeWelcomeRecord(const QString& workspace_id, const QString& author_id)
    -> storage::DocumentRecord {
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
              .workspace_id = workspace_id.toStdString(),
              .parent_id = std::nullopt,
              .sort_order = 0,
              .created_at = now,
              .updated_at = now,
              .created_by = EffectiveAuthorId(author_id),
              .updated_by = EffectiveAuthorId(author_id),
              .content_version = 1,
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
                           std::int32_t sort_order = 0,
                           QString workspace_id = QStringLiteral("default"), QString author_id = {})
    -> storage::DocumentRecord {
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
              .workspace_id = workspace_id.toStdString(),
              .parent_id = std::move(parent_id),
              .sort_order = sort_order,
              .created_at = now,
              .updated_at = now,
              .created_by = EffectiveAuthorId(author_id),
              .updated_by = EffectiveAuthorId(author_id),
              .content_version = 1,
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
                        const QString& page_id, const QString& current_workspace_id)
    -> std::variant<storage::DocumentRecord, QVariantMap> {
  auto result = repository->LoadDocument(page_id.toStdString());
  if (!result.document) {
    return ErrorResponse(
        QStringLiteral("load_failed"),
        QString::fromStdString(result.error ? result.error->message : "Document was not found."));
  }
  if (NormalizeWorkspaceId(QString::fromStdString(
          EffectiveWorkspaceId(result.document->metadata.workspace_id))) != current_workspace_id) {
    return ErrorResponse(QStringLiteral("workspace_mismatch"),
                         QStringLiteral("Document belongs to another workspace."));
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

void QEditorBridge::SetRepository(std::shared_ptr<storage::LocalDocumentRepository> repository) {
  repository_ = std::move(repository);
}

void QEditorBridge::SetSyncStateProvider(const sync::SyncStateProvider* provider) {
  sync_state_provider_ = provider;
}

void QEditorBridge::SetPendingDocumentAccess(bool editable, QString lock_owner,
                                             QString access_message) {
  pending_document_editable_ = editable;
  pending_lock_owner_ = std::move(lock_owner);
  pending_access_message_ = std::move(access_message);
}

void QEditorBridge::SetCurrentDocumentAccess(bool editable, QString lock_owner,
                                             QString access_message) {
  current_document_editable_ = editable;
  current_lock_owner_ = std::move(lock_owner);
  current_access_message_ = std::move(access_message);
  emit documentAccessChanged(current_document_editable_, current_lock_owner_,
                             current_access_message_);
}

void QEditorBridge::SetCurrentDocumentConflicted(bool has_conflict) {
  current_document_has_conflict_ = has_conflict;
}

void QEditorBridge::SetCurrentAuthorId(QString author_id) {
  current_author_id_ = std::move(author_id);
}

void QEditorBridge::SetCurrentWorkspaceId(QString workspace_id) {
  current_workspace_id_ = NormalizeWorkspaceId(std::move(workspace_id));
  ClearCurrentDocumentSelection();
}

void QEditorBridge::RequestOpenDocument(const QString& page_id) {
  emit documentOpenRequested(page_id);
}

void QEditorBridge::ClearCurrentDocumentSelection() {
  current_page_id_.clear();
  current_document_editable_ = true;
  current_document_has_conflict_ = false;
  current_lock_owner_.clear();
  current_access_message_.clear();
  emit documentSelectionCleared();
}

QVariantMap QEditorBridge::getBridgeInfo() {
  return SuccessResponse(BridgeInfo());
}

QVariantMap QEditorBridge::getInitialDocument() {
  return SuccessResponse(QVariantList{});
}

QVariantMap QEditorBridge::listDocumentsInWorkspace(const QString& workspace_id) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  const auto normalized_workspace_id = NormalizeWorkspaceId(workspace_id);
  auto result = repository_->ListDocuments();
  if (result.error) {
    return ErrorResponse(QStringLiteral("list_failed"),
                         QString::fromStdString(result.error->message));
  }

  auto documents = DocumentSummariesToVariant(result.documents, normalized_workspace_id);
  if (documents.empty()) {
    const bool should_create_welcome =
        sync_state_provider_ == nullptr ||
        sync_state_provider_->ShouldCreateSyntheticWelcomePage(normalized_workspace_id);
    const bool sync_expected =
        repository_->SupportsSync() && sync_state_provider_ != nullptr &&
        sync_state_provider_->ShouldExpectRemoteDocuments(normalized_workspace_id);
    if (sync_expected) {
      spdlog::info(
          "Workspace '{}' is empty locally, but remote sync is expected; skipping welcome creation",
          normalized_workspace_id.toStdString());
      return SuccessResponse(QVariantList{});
    }

    if (!should_create_welcome) {
      spdlog::info(
          "Workspace '{}' is empty locally and synthetic welcome creation is suppressed while sync "
          "mode is active",
          normalized_workspace_id.toStdString());
      return SuccessResponse(QVariantList{});
    }

    spdlog::info(
        "Workspace '{}' is empty locally and no remote sync is expected; creating local welcome "
        "page",
        normalized_workspace_id.toStdString());
    auto welcome = MakeWelcomeRecord(normalized_workspace_id, current_author_id_);
    const auto save_result = repository_->SaveDocument(welcome);
    if (save_result.error) {
      return ErrorResponse(QStringLiteral("default_page_failed"),
                           QString::fromStdString(save_result.error->message));
    }

    // Materialize a local workspace root/meta record so this workspace is
    // treated as a real, fact-proven entity (not just an id inferred from a
    // channel list), consistent with synced workspaces that gain their root
    // document via replication. For a purely local-only workspace this root
    // is created immediately since there is no remote pull to wait for.
    if (!repository_->LoadWorkspaceRoot(normalized_workspace_id.toStdString()).has_value()) {
      const auto workspace_root_result =
          repository_->SaveWorkspaceRoot(storage::WorkspaceRootRecord{
              .workspace_id = normalized_workspace_id.toStdString(),
              .title = normalized_workspace_id.toStdString(),
              .created_at = CurrentUtcTimestamp(),
              .schema_version = 1,
          });
      if (workspace_root_result.error) {
        spdlog::warn("Failed to materialize local workspace root for '{}': {}",
                     normalized_workspace_id.toStdString(), workspace_root_result.error->message);
      }
    }

    documents = DocumentSummariesToVariant(
        std::vector<storage::DocumentSummary>{
            storage::DocumentSummaryFromMetadata(welcome.metadata)},
        normalized_workspace_id);
  }

  return SuccessResponse(documents);
}

QVariantMap QEditorBridge::listDocuments() {
  return listDocumentsInWorkspace(current_workspace_id_);
}

QVariantMap QEditorBridge::createDocumentInWorkspace(const QString& workspace_id) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  auto record = MakeNewDocumentRecord(std::nullopt, 0, NormalizeWorkspaceId(workspace_id),
                                      current_author_id_);
  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("create_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(MetadataToVariant(record.metadata));
}

QVariantMap QEditorBridge::createDocument() {
  return createDocumentInWorkspace(current_workspace_id_);
}

QVariantMap QEditorBridge::createChildDocumentInWorkspace(const QString& workspace_id,
                                                          const QString& parent_id) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  if (parent_id.isEmpty()) {
    return ErrorResponse(QStringLiteral("invalid_parent"),
                         QStringLiteral("Parent document id is required."));
  }

  const auto normalized_workspace_id = NormalizeWorkspaceId(workspace_id);
  const auto sort_order = NextChildSortOrder(repository_, parent_id.toStdString());
  auto parent_loaded = repository_->LoadDocument(parent_id.toStdString());
  if (!parent_loaded.document) {
    return ErrorResponse(QStringLiteral("invalid_parent"),
                         QStringLiteral("Parent document was not found."));
  }
  if (NormalizeWorkspaceId(QString::fromStdString(EffectiveWorkspaceId(
          parent_loaded.document->metadata.workspace_id))) != normalized_workspace_id) {
    return ErrorResponse(QStringLiteral("invalid_parent"),
                         QStringLiteral("Parent document belongs to another workspace."));
  }

  auto record = MakeNewDocumentRecord(parent_id.toStdString(), sort_order, normalized_workspace_id,
                                      current_author_id_);
  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("create_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(MetadataToVariant(record.metadata));
}

QVariantMap QEditorBridge::createChildDocument(const QString& parent_id) {
  return createChildDocumentInWorkspace(current_workspace_id_, parent_id);
}

std::optional<QVariantMap> QEditorBridge::RejectIfCurrentDocumentLocked(
    const QString& page_id) const {
  if (page_id != current_page_id_ || current_document_editable_) {
    return std::nullopt;
  }
  return ErrorResponse(QStringLiteral("document_read_only"),
                       current_access_message_.isEmpty() ? QStringLiteral("Document is read-only.")
                                                         : current_access_message_);
}

std::optional<QVariantMap> QEditorBridge::RejectIfCurrentDocumentConflicted(
    const QString& page_id) const {
  if (page_id != current_page_id_ || !current_document_has_conflict_) {
    return std::nullopt;
  }
  return ErrorResponse(
      QStringLiteral("document_read_only"),
      QStringLiteral("Document has an unresolved sync conflict. Resolve it before editing."));
}

QVariantMap QEditorBridge::renameDocument(const QString& page_id, const QString& title) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  if (page_id.isEmpty()) {
    return ErrorResponse(QStringLiteral("invalid_document"),
                         QStringLiteral("Document id is required."));
  }

  const auto trimmed_title = title.trimmed();
  if (trimmed_title.isEmpty()) {
    return ErrorResponse(QStringLiteral("invalid_title"),
                         QStringLiteral("Document title cannot be empty."));
  }

  if (auto lock_error = RejectIfCurrentDocumentLocked(page_id)) {
    return std::move(*lock_error);
  }
  if (auto conflict_error = RejectIfCurrentDocumentConflicted(page_id)) {
    return std::move(*conflict_error);
  }

  auto loaded_or_error = LoadDocumentRecord(repository_, page_id, current_workspace_id_);
  if (std::holds_alternative<QVariantMap>(loaded_or_error)) {
    return std::get<QVariantMap>(std::move(loaded_or_error));
  }

  auto record = std::move(std::get<storage::DocumentRecord>(loaded_or_error));
  record.metadata.title = trimmed_title.toStdString();
  ApplyDocumentMutationAudit(record.metadata, current_author_id_);
  record.snapshot.title = record.metadata.title;

  auto blocks_or_error = ExtractBlocks(QByteArray::fromStdString(record.raw_snapshot_json));
  if (std::holds_alternative<QString>(blocks_or_error)) {
    return ErrorResponse(QStringLiteral("invalid_stored_snapshot"),
                         std::get<QString>(blocks_or_error));
  }

  const auto blocks = std::get<QJsonArray>(std::move(blocks_or_error));
  const auto raw_snapshot_json =
      MakeDocumentSnapshotJson(record.metadata.id, record.metadata.title, blocks);
  record.raw_snapshot_json = std::string(raw_snapshot_json.constData(),
                                         static_cast<std::size_t>(raw_snapshot_json.size()));

  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("rename_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(MetadataToVariant(record.metadata));
}

QVariantMap QEditorBridge::updateDocumentPlacement(const QString& page_id, const QString& parent_id,
                                                   bool has_parent_id, int sort_order) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  if (auto lock_error = RejectIfCurrentDocumentLocked(page_id)) {
    return std::move(*lock_error);
  }
  if (auto conflict_error = RejectIfCurrentDocumentConflicted(page_id)) {
    return std::move(*conflict_error);
  }

  auto loaded_or_error = LoadDocumentRecord(repository_, page_id, current_workspace_id_);
  if (std::holds_alternative<QVariantMap>(loaded_or_error)) {
    return std::get<QVariantMap>(std::move(loaded_or_error));
  }

  auto record = std::move(std::get<storage::DocumentRecord>(loaded_or_error));
  record.metadata.parent_id =
      has_parent_id ? std::make_optional(parent_id.toStdString()) : std::nullopt;
  record.metadata.sort_order = sort_order;
  ApplyDocumentMutationAudit(record.metadata, current_author_id_);

  auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("move_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(MetadataToVariant(record.metadata));
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

  if (auto lock_error = RejectIfCurrentDocumentLocked(page_id)) {
    return std::move(*lock_error);
  }
  if (auto conflict_error = RejectIfCurrentDocumentConflicted(page_id)) {
    return std::move(*conflict_error);
  }

  auto loaded_or_error = LoadDocumentRecord(repository_, page_id, current_workspace_id_);
  if (std::holds_alternative<QVariantMap>(loaded_or_error)) {
    return std::get<QVariantMap>(std::move(loaded_or_error));
  }

  if (const auto error = DeleteDocumentTree(repository_, page_id.toStdString())) {
    return ErrorResponse(QStringLiteral("delete_failed"), QString::fromStdString(error->message));
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
    return ErrorResponse(
        QStringLiteral("load_failed"),
        QString::fromStdString(result.error ? result.error->message : "Document was not found."));
  }
  if (NormalizeWorkspaceId(QString::fromStdString(
          EffectiveWorkspaceId(result.document->metadata.workspace_id))) != current_workspace_id_) {
    return ErrorResponse(QStringLiteral("workspace_mismatch"),
                         QStringLiteral("Document belongs to another workspace."));
  }

  auto blocks = BlocksFromRawSnapshotJson(result.document->raw_snapshot_json);
  if (std::holds_alternative<QString>(blocks)) {
    return ErrorResponse(QStringLiteral("invalid_stored_snapshot"), std::get<QString>(blocks));
  }

  current_page_id_ = page_id;
  current_document_editable_ = pending_document_editable_;
  current_document_has_conflict_ = false;
  current_lock_owner_ = pending_lock_owner_;
  current_access_message_ = pending_access_message_;
  auto response = MetadataToVariant(result.document->metadata);
  response.insert(QStringLiteral("blocks"), std::get<QVariantList>(std::move(blocks)));
  response.insert(QStringLiteral("editable"), current_document_editable_);
  response.insert(QStringLiteral("lockOwner"), current_lock_owner_);
  response.insert(QStringLiteral("accessMessage"), current_access_message_);
  return SuccessResponse(response);
}

QVariantMap QEditorBridge::openDocument(const QString& page_id) {
  const auto response = loadDocument(page_id);
  if (!response.value(QStringLiteral("ok")).toBool()) {
    const auto error = response.value(QStringLiteral("error")).toMap();
    emit documentLoadFailed(page_id, error.value(QStringLiteral("message")).toString());
    return response;
  }

  emit documentLoaded(response.value(QStringLiteral("result")).toMap());
  emit documentAccessChanged(current_document_editable_, current_lock_owner_,
                             current_access_message_);
  return response;
}

QVariantMap QEditorBridge::updateSnapshot(const QString& snapshot_json) {
  if (current_page_id_.isEmpty()) {
    return ErrorResponse(QStringLiteral("no_document_selected"),
                         QStringLiteral("No document is selected."));
  }

  if (!current_document_editable_) {
    emit saveStatusChanged(current_page_id_, false,
                           current_access_message_.isEmpty()
                               ? QStringLiteral("Document is read-only.")
                               : current_access_message_);
    return ErrorResponse(QStringLiteral("document_read_only"),
                         current_access_message_.isEmpty()
                             ? QStringLiteral("Document is read-only.")
                             : current_access_message_);
  }

  if (current_document_has_conflict_) {
    const auto message =
        QStringLiteral("Document has an unresolved sync conflict. Resolve it before editing.");
    emit saveStatusChanged(current_page_id_, false, message);
    return ErrorResponse(QStringLiteral("document_read_only"), message);
  }

  emit saveStatusChanged(current_page_id_, true, QStringLiteral("Saving..."));

  const auto snapshot_bytes = snapshot_json.toUtf8();
  const auto validation = document::DocumentValidator::ParseAndValidateSnapshot(snapshot_bytes);
  if (validation.error) {
    spdlog::warn("editor snapshot rejected: {}: {}", document::ToString(validation.error->code),
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
    if (record.metadata.workspace_id.empty()) {
      record.metadata.workspace_id = "default";
    }
    if (record.metadata.created_by.empty()) {
      record.metadata.created_by = EffectiveAuthorId(current_author_id_);
    }
    if (record.metadata.updated_by.empty()) {
      record.metadata.updated_by = record.metadata.created_by;
    }
    if (record.metadata.content_version < 1) {
      record.metadata.content_version = 1;
    }
    const auto fallback_title = record.metadata.title.empty()
                                    ? std::string_view(constants::kNewDocumentTitle)
                                    : std::string_view(record.metadata.title);
    record.metadata.title = ExtractTitle(*validation.document, fallback_title);
    if (record.metadata.created_at.empty()) {
      record.metadata.created_at = CurrentUtcTimestamp();
    }
    ApplyDocumentMutationAudit(record.metadata, current_author_id_);
    record.snapshot = *validation.snapshot;
    record.snapshot.id = record.metadata.id;
    record.snapshot.schema_version = static_cast<std::int32_t>(document::SchemaVersion::kV1);
    record.snapshot.title = record.metadata.title;

    const auto raw_snapshot_json = MakeDocumentSnapshotJson(
        record.metadata.id, record.metadata.title, std::get<QJsonArray>(blocks));
    record.raw_snapshot_json = std::string(raw_snapshot_json.constData(),
                                           static_cast<std::size_t>(raw_snapshot_json.size()));

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
