#include "editor_bridge.h"

#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QUrl>
#include <QVariant>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "auth/ai_api_key_store.h"
#include "core/constants.h"
#include "core/qt_string.h"
#include "core/uuid.h"
#include "document/document.h"
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

auto BridgeInfo(bool ai_features_enabled, bool ai_autocomplete_enabled,
               bool ai_inline_suggestions_enabled) -> QVariantMap {
  return QVariantMap{
      {QStringLiteral("apiVersion"), constants::kBridgeApiVersion},
      {QStringLiteral("namespace"), ToQString(constants::kDocumentsBridgeNamespace)},
      {QStringLiteral("aiFeaturesEnabled"), ai_features_enabled},
      {QStringLiteral("aiAutocompleteEnabled"), ai_autocomplete_enabled},
      {QStringLiteral("aiInlineSuggestionsEnabled"), ai_inline_suggestions_enabled},
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
      {QStringLiteral("kind"), QString::fromStdString(document::ToDocumentKindKey(metadata.kind))},
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
        {QStringLiteral("kind"), QString::fromStdString(document::ToDocumentKindKey(document.kind))},
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

// Minimal, valid nbformat v4 document with no cells — the seed content for a newly created
// kJupyterNotebook document. Deliberately not a full nbformat schema (validation of that shape
// is out of scope per #52); just enough for NotebookView.tsx to render an empty notebook.
auto MakeEmptyNotebookSnapshotJson() -> QByteArray {
  QJsonObject notebook;
  notebook.insert(QStringLiteral("cells"), QJsonArray{});
  notebook.insert(QStringLiteral("metadata"), QJsonObject{});
  notebook.insert(QStringLiteral("nbformat"), 4);
  notebook.insert(QStringLiteral("nbformat_minor"), 5);
  return QJsonDocument(notebook).toJson(QJsonDocument::Compact);
}

// Minimal, valid Excalidraw scene with no elements — the seed content for a newly created
// kExcalidrawCanvas document. Shape must match ExcalidrawSceneJson (frontend/editor/src/canvas/
// excalidrawScene.ts): {type: "excalidraw", version: 2, elements: [], appState: {...}, files: {}}.
auto MakeEmptyExcalidrawSceneSnapshotJson() -> QByteArray {
  QJsonObject scene;
  scene.insert(QStringLiteral("type"), QStringLiteral("excalidraw"));
  scene.insert(QStringLiteral("version"), 2);
  scene.insert(QStringLiteral("elements"), QJsonArray{});
  scene.insert(QStringLiteral("appState"), QJsonObject{});
  scene.insert(QStringLiteral("files"), QJsonObject{});
  return QJsonDocument(scene).toJson(QJsonDocument::Compact);
}

auto MakeNewDocumentRecord(std::optional<std::string> parent_id = std::nullopt,
                           std::int32_t sort_order = 0,
                           QString workspace_id = QStringLiteral("default"), QString author_id = {},
                           document::DocumentKind kind = document::DocumentKind::kWikiPage)
    -> storage::DocumentRecord {
  const auto id = GenerateUuidString();
  const auto title = std::string(constants::kNewDocumentTitle);
  const auto now = CurrentUtcTimestamp();

  const auto raw_snapshot_json = [&]() -> QByteArray {
    switch (kind) {
      case document::DocumentKind::kJupyterNotebook:
        return MakeEmptyNotebookSnapshotJson();
      case document::DocumentKind::kExcalidrawCanvas:
        return MakeEmptyExcalidrawSceneSnapshotJson();
      case document::DocumentKind::kWikiPage:
        return MakeDocumentSnapshotJson(id, title, QJsonArray{});
    }
    return MakeDocumentSnapshotJson(id, title, QJsonArray{});
  }();

  return storage::DocumentRecord{
      .metadata =
          document::PageMetadata{
              .id = id,
              .schema_version = document::SchemaVersion::kV1,
              .kind = kind,
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
  current_page_kind_ = document::DocumentKind::kWikiPage;
  current_document_editable_ = true;
  current_document_has_conflict_ = false;
  current_lock_owner_.clear();
  current_access_message_.clear();
  emit documentSelectionCleared();
}

QVariantMap QEditorBridge::getBridgeInfo() {
  return SuccessResponse(
      BridgeInfo(ai_features_enabled_, ai_autocomplete_enabled_, ai_inline_suggestions_enabled_));
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

QVariantMap QEditorBridge::createDocumentInWorkspace(const QString& workspace_id,
                                                     const QString& kind) {
  if (!repository_) {
    return ErrorResponse(QStringLiteral("repository_unavailable"),
                         QStringLiteral("Document repository is not configured."));
  }

  auto record = MakeNewDocumentRecord(std::nullopt, 0, NormalizeWorkspaceId(workspace_id),
                                      current_author_id_,
                                      document::DocumentKindFromKey(kind.toStdString()));
  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("create_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(MetadataToVariant(record.metadata));
}

QVariantMap QEditorBridge::createDocument() {
  return createDocumentInWorkspace(current_workspace_id_,
                                   QString::fromStdString(document::ToDocumentKindKey(
                                       document::DocumentKind::kWikiPage)));
}

QVariantMap QEditorBridge::createChildDocumentInWorkspace(const QString& workspace_id,
                                                          const QString& parent_id,
                                                          const QString& kind) {
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
                                      current_author_id_,
                                      document::DocumentKindFromKey(kind.toStdString()));
  const auto save_result = repository_->SaveDocument(record);
  if (save_result.error) {
    return ErrorResponse(QStringLiteral("create_failed"),
                         QString::fromStdString(save_result.error->message));
  }

  return SuccessResponse(MetadataToVariant(record.metadata));
}

QVariantMap QEditorBridge::createChildDocument(const QString& parent_id, const QString& kind) {
  return createChildDocumentInWorkspace(current_workspace_id_, parent_id, kind);
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

  const auto kind = result.document->metadata.kind;

  // "blocks" only means anything for kWikiPage (BlockNote's block array). The other kinds
  // (nbformat/Excalidraw, #52/#53) don't have blocks at all — their renderer instead reads the
  // raw stored snapshot JSON verbatim via "rawContent" below, so skip the block-array
  // extraction (and its "does not contain a block array" error) entirely for them.
  QVariantList blocks_variant;
  QString raw_content;
  if (kind == document::DocumentKind::kWikiPage) {
    auto blocks = BlocksFromRawSnapshotJson(result.document->raw_snapshot_json);
    if (std::holds_alternative<QString>(blocks)) {
      return ErrorResponse(QStringLiteral("invalid_stored_snapshot"), std::get<QString>(blocks));
    }
    blocks_variant = std::get<QVariantList>(std::move(blocks));
  } else {
    // Non-wikiPage kinds (nbformat/Excalidraw, #52/#53) don't fit the BlockNote block-array
    // shape; hand the raw stored JSON to the frontend as-is via `rawContent` instead, and leave
    // `blocks` empty for LoadedDocument.blocks' array type.
    raw_content = QString::fromStdString(result.document->raw_snapshot_json);
  }

  current_page_id_ = page_id;
  current_page_kind_ = kind;
  current_document_editable_ = pending_document_editable_;
  current_document_has_conflict_ = false;
  current_lock_owner_ = pending_lock_owner_;
  current_access_message_ = pending_access_message_;
  auto response = MetadataToVariant(result.document->metadata);
  response.insert(QStringLiteral("blocks"), blocks_variant);
  if (kind != document::DocumentKind::kWikiPage) {
    response.insert(QStringLiteral("rawContent"), raw_content);
  }
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

QVariantMap QEditorBridge::updateSnapshot(const QString& page_id, const QString& snapshot_json) {
  if (current_page_id_.isEmpty()) {
    return ErrorResponse(QStringLiteral("no_document_selected"),
                         QStringLiteral("No document is selected."));
  }

  // The caller (JS) captured page_id at the time it decided to save, but the save may reach
  // here after an intervening document switch (debounced timers, overlapping async open
  // chains). Silently applying it would corrupt whatever document happens to be open now, with
  // no way to distinguish that from a legitimate write — see the header doc comment.
  if (page_id != current_page_id_) {
    return ErrorResponse(QStringLiteral("stale_document"),
                         QStringLiteral("Snapshot targets a document that is no longer open."));
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
  const auto validation =
      document::DocumentValidator::ParseAndValidateSnapshot(snapshot_bytes, current_page_kind_);
  if (validation.error) {
    spdlog::warn("editor snapshot rejected: {}: {}", document::ToString(validation.error->code),
                 validation.error->message);
    emit saveStatusChanged(current_page_id_, false,
                           QString::fromStdString(validation.error->message));
    return ErrorResponse(ToQString(document::ToString(validation.error->code)),
                         QString::fromStdString(validation.error->message));
  }

  const bool is_wiki_page = current_page_kind_ == document::DocumentKind::kWikiPage;
  if (is_wiki_page) {
    spdlog::info("editor snapshot received: bytes={}, blocks={}", snapshot_bytes.size(),
                 validation.document->blocks.size());
  } else {
    spdlog::info("editor snapshot received: bytes={}, kind={}", snapshot_bytes.size(),
                 document::ToDocumentKindKey(current_page_kind_));
  }

  // Save to repository if available
  if (repository_) {
    storage::DocumentRecord record;
    if (auto current = repository_->LoadDocument(current_page_id_.toStdString());
        current.document) {
      record.metadata = current.document->metadata;
    }
    record.metadata.id = current_page_id_.toStdString();
    record.metadata.schema_version = document::SchemaVersion::kV1;
    record.metadata.kind = current_page_kind_;
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
    if (record.metadata.created_at.empty()) {
      record.metadata.created_at = CurrentUtcTimestamp();
    }

    if (is_wiki_page) {
      // BlockNote path (unchanged): the payload is a validated block array/document, so its
      // title comes from the first-level-1-heading heuristic and the persisted snapshot is
      // re-serialized as {id, schema_version, title, blocks}.
      auto blocks = ExtractBlocks(snapshot_bytes);
      if (std::holds_alternative<QString>(blocks)) {
        return ErrorResponse(QStringLiteral("invalid_root"), std::get<QString>(blocks));
      }

      const auto fallback_title = record.metadata.title.empty()
                                      ? std::string_view(constants::kNewDocumentTitle)
                                      : std::string_view(record.metadata.title);
      record.metadata.title = ExtractTitle(*validation.document, fallback_title);
      ApplyDocumentMutationAudit(record.metadata, current_author_id_);
      record.snapshot = *validation.snapshot;
      record.snapshot.id = record.metadata.id;
      record.snapshot.schema_version = static_cast<std::int32_t>(document::SchemaVersion::kV1);
      record.snapshot.title = record.metadata.title;

      const auto raw_snapshot_json = MakeDocumentSnapshotJson(
          record.metadata.id, record.metadata.title, std::get<QJsonArray>(blocks));
      record.raw_snapshot_json = std::string(raw_snapshot_json.constData(),
                                             static_cast<std::size_t>(raw_snapshot_json.size()));
    } else {
      // Non-wiki-page kinds (nbformat/Excalidraw, #52/#53): DocumentValidator only confirmed
      // the payload is well-formed JSON above — there's no BlockNote block array to extract a
      // title from or re-derive raw_snapshot_json from, so persist the caller's JSON verbatim
      // and keep whatever title is already on the document (set at creation time / rename).
      if (record.metadata.title.empty()) {
        record.metadata.title = constants::kNewDocumentTitle;
      }
      ApplyDocumentMutationAudit(record.metadata, current_author_id_);
      record.raw_snapshot_json = validation.raw_snapshot_json;
    }

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

void QEditorBridge::SetAiTransportConfig(bool backend_enabled, QString backend_base_url,
                                         QString backend_access_token) {
  ai_backend_enabled_ = backend_enabled;
  ai_backend_base_url_ = std::move(backend_base_url);
  ai_backend_access_token_ = std::move(backend_access_token);
}

void QEditorBridge::SetAiApiKeyStore(auth::AiApiKeyStore* key_store) {
  ai_api_key_store_ = key_store;
}

void QEditorBridge::SetAiFeatureFlags(bool features_enabled, bool autocomplete_enabled,
                                      bool inline_suggestions_enabled) {
  ai_features_enabled_ = features_enabled;
  ai_autocomplete_enabled_ = autocomplete_enabled;
  ai_inline_suggestions_enabled_ = inline_suggestions_enabled;
}

QString QEditorBridge::startAiRequest(const QString& prompt, const QString& context_text,
                                      const QString& mode, const QString& tool_name,
                                      const QString& tool_schema_json) {
  const auto request_id = QString::fromStdString(GenerateUuidString());

  if (ai_backend_enabled_ && !ai_backend_base_url_.isEmpty()) {
    // Server-mediated (default): forward to cppwiki_server, which holds the
    // provider key (ADR-012).
    StartServerMediatedAiRequest(request_id, prompt, context_text, mode, tool_name,
                                 tool_schema_json);
  } else if (ai_api_key_store_ != nullptr) {
    // Local-key fallback (ADR-012 addendum, serverless installs): read the
    // key from the OS keychain and call the provider directly from C++.
    StartLocalKeyAiRequest(request_id, prompt, context_text, mode, tool_name, tool_schema_json);
  } else {
    // Deferred emit so callers can subscribe to the returned request id
    // before the failure signal fires.
    QMetaObject::invokeMethod(
        this,
        [this, request_id]() {
          emit aiRequestFailed(request_id,
                               QStringLiteral("No AI backend configured and no local API key "
                                              "is set. Configure one in Settings > AI."));
        },
        Qt::QueuedConnection);
  }

  return request_id;
}

void QEditorBridge::StartServerMediatedAiRequest(const QString& request_id, const QString& prompt,
                                                 const QString& context_text,
                                                 const QString& mode, const QString& tool_name,
                                                 const QString& tool_schema_json) {
  QJsonObject body;
  body.insert(QStringLiteral("prompt"), prompt);
  body.insert(QStringLiteral("context"), context_text);
  body.insert(QStringLiteral("mode"), mode);
  if (!tool_name.isEmpty() && !tool_schema_json.isEmpty()) {
    body.insert(QStringLiteral("toolName"), tool_name);
    body.insert(QStringLiteral("toolSchemaJson"), tool_schema_json);
  }

  const QUrl url(ai_backend_base_url_ + QStringLiteral("/api/v1/ai/chat"));
  const auto auth_header =
      ai_backend_access_token_.isEmpty()
          ? QString()
          : QStringLiteral("Bearer ") + ai_backend_access_token_;
  CallProviderAndRelay(request_id, url, QJsonDocument(body).toJson(QJsonDocument::Compact),
                      auth_header, tool_name);
}

void QEditorBridge::StartLocalKeyAiRequest(const QString& request_id, const QString& prompt,
                                           const QString& context_text, const QString& mode,
                                           const QString& tool_name,
                                           const QString& tool_schema_json) {
  // The keychain read is asynchronous; wire a one-shot connection so we call
  // the provider once the key (or its absence) is known.
  auto* key_store = ai_api_key_store_;
  auto loaded_connection = std::make_shared<QMetaObject::Connection>();
  auto missing_connection = std::make_shared<QMetaObject::Connection>();

  *loaded_connection = connect(
      key_store, &auth::AiApiKeyStore::apiKeyLoaded, this,
      [this, request_id, prompt, context_text, mode, tool_name, tool_schema_json,
       loaded_connection, missing_connection](const QString& api_key) {
        QObject::disconnect(*loaded_connection);
        QObject::disconnect(*missing_connection);

        QJsonObject body;
        QJsonArray messages;
        QJsonObject message;
        message.insert(QStringLiteral("role"), QStringLiteral("user"));
        message.insert(QStringLiteral("content"),
                       mode + QStringLiteral(": ") + context_text + QStringLiteral("\n\n") +
                           prompt);
        messages.append(message);
        body.insert(QStringLiteral("messages"), messages);

        if (!tool_name.isEmpty() && !tool_schema_json.isEmpty()) {
          // Build an OpenAI-compatible tool-calling request instead of a
          // plain completion: the provider must reply with structured
          // arguments matching `tool_schema_json` rather than prose (see
          // issue #65 — xl-ai ignores plain text responses entirely).
          QJsonParseError schema_parse_error{};
          const auto schema_document =
              QJsonDocument::fromJson(tool_schema_json.toUtf8(), &schema_parse_error);

          QJsonObject function_def;
          function_def.insert(QStringLiteral("name"), tool_name);
          function_def.insert(QStringLiteral("parameters"),
                              schema_parse_error.error == QJsonParseError::NoError
                                  ? schema_document.object()
                                  : QJsonObject{});

          QJsonObject tool_def;
          tool_def.insert(QStringLiteral("type"), QStringLiteral("function"));
          tool_def.insert(QStringLiteral("function"), function_def);
          body.insert(QStringLiteral("tools"), QJsonArray{tool_def});

          QJsonObject tool_choice_function;
          tool_choice_function.insert(QStringLiteral("name"), tool_name);
          QJsonObject tool_choice;
          tool_choice.insert(QStringLiteral("type"), QStringLiteral("function"));
          tool_choice.insert(QStringLiteral("function"), tool_choice_function);
          body.insert(QStringLiteral("tool_choice"), tool_choice);
        }

        const QUrl url(QStringLiteral("https://api.openai.com/v1/chat/completions"));
        CallProviderAndRelay(request_id, url,
                            QJsonDocument(body).toJson(QJsonDocument::Compact),
                            QStringLiteral("Bearer ") + api_key, tool_name);
      });

  *missing_connection = connect(
      key_store, &auth::AiApiKeyStore::apiKeyMissing, this,
      [this, request_id, loaded_connection, missing_connection]() {
        QObject::disconnect(*loaded_connection);
        QObject::disconnect(*missing_connection);
        emit aiRequestFailed(request_id,
                             QStringLiteral("No local AI provider API key is configured."));
      });

  key_store->Load();
}

void QEditorBridge::CallProviderAndRelay(const QString& request_id, const QUrl& url,
                                        const QByteArray& body,
                                        const QString& auth_header_value,
                                        const QString& tool_name) {
  if (network_manager_ == nullptr) {
    network_manager_ = new QNetworkAccessManager(this);
  }

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
  if (!auth_header_value.isEmpty()) {
    request.setRawHeader("Authorization", auth_header_value.toUtf8());
  }

  auto* reply = network_manager_->post(request, body);
  connect(reply, &QNetworkReply::finished, this, [this, request_id, reply, tool_name]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      emit aiRequestFailed(request_id, reply->errorString());
      return;
    }

    const auto raw = reply->readAll();
    QJsonParseError parse_error{};
    const auto document = QJsonDocument::fromJson(raw, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
      emit aiRequestFailed(request_id, QStringLiteral("AI provider returned an invalid response."));
      return;
    }

    const auto object = document.object();

    if (!tool_name.isEmpty()) {
      // Structured tool-call path: extract the tool call's arguments (a
      // JSON string) instead of prose text.
      QString arguments_json;
      if (object.contains(QStringLiteral("result")) &&
          object.value(QStringLiteral("result")).isObject()) {
        // cppwiki_server envelope: { ok, result: { toolArgumentsJson } }.
        arguments_json = object.value(QStringLiteral("result"))
                             .toObject()
                             .value(QStringLiteral("toolArgumentsJson"))
                             .toString();
      } else if (object.contains(QStringLiteral("choices"))) {
        // Raw OpenAI-compatible tool-calling response (local-key path).
        const auto choices = object.value(QStringLiteral("choices")).toArray();
        if (!choices.isEmpty()) {
          const auto tool_calls = choices.first()
                                       .toObject()
                                       .value(QStringLiteral("message"))
                                       .toObject()
                                       .value(QStringLiteral("tool_calls"))
                                       .toArray();
          if (!tool_calls.isEmpty()) {
            arguments_json = tool_calls.first()
                                  .toObject()
                                  .value(QStringLiteral("function"))
                                  .toObject()
                                  .value(QStringLiteral("arguments"))
                                  .toString();
          }
        }
      }

      if (arguments_json.isEmpty()) {
        emit aiRequestFailed(request_id,
                             QStringLiteral("AI provider returned an empty tool call."));
        return;
      }

      emit aiToolCallReceived(request_id, tool_name, arguments_json);
      emit aiRequestCompleted(request_id);
      return;
    }

    QString text;
    if (object.contains(QStringLiteral("result")) && object.value(QStringLiteral("result")).isObject()) {
      // cppwiki_server envelope: { ok, result: { text } }.
      text = object.value(QStringLiteral("result")).toObject().value(QStringLiteral("text")).toString();
    } else if (object.contains(QStringLiteral("choices"))) {
      // Raw OpenAI-compatible chat completion response (local-key path).
      const auto choices = object.value(QStringLiteral("choices")).toArray();
      if (!choices.isEmpty()) {
        const auto content = choices.first().toObject().value(QStringLiteral("message")).toObject()
                                  .value(QStringLiteral("content"));
        if (content.isString()) {
          text = content.toString();
        } else if (content.isArray()) {
          // Some OpenAI-compatible backends (notably vision-capable models)
          // reply with a content-parts array, e.g.
          // [{"type": "text", "text": "..."}], even for text-only replies,
          // instead of a plain string. Concatenate every part's "text" field.
          for (const auto& part : content.toArray()) {
            const auto part_text = part.toObject().value(QStringLiteral("text"));
            if (part_text.isString()) {
              text += part_text.toString();
            }
          }
        }
      }
    }

    if (text.isEmpty()) {
      emit aiRequestFailed(request_id, QStringLiteral("AI provider returned an empty response."));
      return;
    }

    EmitChunkedCompletion(request_id, text);
  });
}

void QEditorBridge::EmitChunkedCompletion(const QString& request_id, const QString& full_text) {
  // Relays the (already-complete) response as a sequence of bridge signals,
  // since chunk-by-chunk delivery is required by the bridge signal contract
  // (ADR-012) even when the upstream call itself was not a native stream.
  constexpr int kChunkSize = 24;
  for (int offset = 0; offset < full_text.size(); offset += kChunkSize) {
    emit aiChunkReceived(request_id, full_text.mid(offset, kChunkSize));
  }
  emit aiRequestCompleted(request_id);
}

}  // namespace cppwiki::bridge
