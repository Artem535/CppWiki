// Suppress upstream header warnings before including CBLite headers
#include "storage/cblite_document_repository.h"

#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QTimeZone>
#include <algorithm>
#include <atomic>
#include <cbl++/CouchbaseLite.hh>
#include <exception>
#include <filesystem>
#include <mutex>
#include <rfl/json.hpp>
#include <set>
#include <utility>

#include "core/constants.h"
#include "sync/sync_bootstrap.h"

namespace cppwiki::storage {
namespace {

constexpr std::string_view kSupportedSyncAuthMode = "oidc_access_token_passthrough";

auto Slice(std::string_view value) -> cbl::slice {
  return cbl::slice(value.data(), value.size());
}

auto MakeError(RepositoryErrorCode code, std::string message) -> RepositoryError {
  return RepositoryError{
      .code = code,
      .message = std::move(message),
  };
}

auto CbliteErrorMessage(const CBLError& error) -> std::string {
  const auto message = cbl::alloc_slice(CBLError_Message(&error)).asString();
  std::string result =
      "Couchbase Lite error domain=" + std::to_string(static_cast<int>(error.domain)) +
      " code=" + std::to_string(error.code);
  if (!message.empty()) {
    result += " message=" + message;
  }
  return result;
}

auto EnsureDirectoryExists(const std::filesystem::path& directory_path)
    -> std::optional<RepositoryError> {
  try {
    if (directory_path.empty()) {
      return MakeError(RepositoryErrorCode::kOpenFailed,
                       "Couchbase Lite database directory path is empty.");
    }

    if (std::filesystem::exists(directory_path)) {
      if (!std::filesystem::is_directory(directory_path)) {
        return MakeError(RepositoryErrorCode::kOpenFailed,
                         "Couchbase Lite database path exists but is not a directory: " +
                             directory_path.string());
      }
      return std::nullopt;
    }

    std::filesystem::create_directories(directory_path);
    return std::nullopt;
  } catch (const std::exception& error) {
    return MakeError(RepositoryErrorCode::kOpenFailed,
                     "Failed to create Couchbase Lite database directory '" +
                         directory_path.string() + "': " + error.what());
  }
}

auto GetMutableDocument(const cbl::Collection& collection, std::string_view document_id)
    -> cbl::MutableDocument {
  auto existing = collection.getDocument(Slice(document_id));
  if (existing) {
    return existing.mutableCopy();
  }
  return cbl::MutableDocument(Slice(document_id));
}

auto MakeBearerHeader(const std::string& access_token) -> std::string {
  return "Bearer " + access_token;
}

auto MakeConflictDocumentId(std::string_view conflict_id) -> std::string {
  return std::string(constants::kConflictDocumentIdPrefix) + std::string(conflict_id);
}

auto IsConflictDocumentId(std::string_view document_id) -> bool {
  return document_id.starts_with(constants::kConflictDocumentIdPrefix);
}

auto MakeWorkspaceRootDocumentId(std::string_view workspace_id) -> std::string {
  return std::string(constants::kWorkspaceDocumentIdPrefix) + std::string(workspace_id);
}

auto IsWorkspaceRootDocumentId(std::string_view document_id) -> bool {
  return document_id.starts_with(constants::kWorkspaceDocumentIdPrefix);
}

auto IsPendingConflict(const DocumentConflictRecord& conflict) -> bool {
  return conflict.resolution_state == "pending";
}

auto IsValidResolutionState(std::string_view resolution_state) -> bool {
  return resolution_state == "pending" || resolution_state == "resolved" ||
         resolution_state == "dismissed";
}

auto ConflictTimestamp(const cbl::Document& document) -> std::uint64_t {
  // NOTE: Some CBL++ distributions used in local environments expose
  // `sequence()` but not `timestamp()`. For runtime conflict diagnostics we
  // only need a monotonic-ish value for conflict id/detected_at fallback.
  return static_cast<bool>(document) ? document.sequence() : 0;
}

auto ConflictActor(const cbl::Document& document) -> std::string {
  if (!document) {
    return {};
  }
  if (const auto value = document.properties()["updated_by"]; value) {
    return std::string(value.asString());
  }
  if (const auto value = document.properties()["created_by"]; value) {
    return std::string(value.asString());
  }
  return {};
}

auto ConflictWorkspaceId(const cbl::Document& local_document, const cbl::Document& remote_document)
    -> std::string {
  const cbl::Document* documents[] = {&local_document, &remote_document};
  for (const auto* document : documents) {
    if (document != nullptr && *document) {
      if (const auto value = document->properties()["workspace_id"]; value) {
        return std::string(value.asString());
      }
    }
  }
  return {};
}

auto ConflictContentVersion(const cbl::Document& local_document,
                            const cbl::Document& remote_document) -> std::int64_t {
  const cbl::Document* documents[] = {&local_document, &remote_document};
  for (const auto* document : documents) {
    if (document != nullptr && *document) {
      if (const auto value = document->properties()["content_version"]; value) {
        return static_cast<std::int64_t>(value.asInt());
      }
    }
  }
  return 0;
}

auto ConflictSnapshotJson(const cbl::Document& document) -> std::string {
  if (!document) {
    return {};
  }
  if (const auto value = document.properties()["raw_snapshot"]; value) {
    return std::string(value.asString());
  }
  return document.propertiesAsJSON().asString();
}

auto TimestampToIso8601(std::uint64_t timestamp_ns) -> std::string {
  if (timestamp_ns == 0) {
    return {};
  }
  const auto datetime = QDateTime::fromMSecsSinceEpoch(
      static_cast<qint64>(timestamp_ns / 1000000ULL), QTimeZone::UTC);
  return datetime.toString(Qt::ISODateWithMs).toStdString();
}

auto NormalizeGatewayReplicationUrl(const sync::SyncBootstrap& bootstrap)
    -> std::optional<std::string> {
  std::string url = bootstrap.gateway_url.trimmed().toStdString();
  if (url.empty()) {
    return std::nullopt;
  }

  if (url.starts_with("http://")) {
    url.replace(0, std::string("http://").size(), "ws://");
  } else if (url.starts_with("https://")) {
    url.replace(0, std::string("https://").size(), "wss://");
  } else if (!url.starts_with("ws://") && !url.starts_with("wss://")) {
    return std::nullopt;
  }

  const auto scheme_delimiter = url.find("://");
  if (scheme_delimiter == std::string::npos) {
    return std::nullopt;
  }

  const auto host_start = scheme_delimiter + 3;
  if (host_start >= url.size()) {
    return std::nullopt;
  }

  const auto path_start = url.find('/', host_start);
  const auto database_name = bootstrap.database_name.trimmed().toStdString();
  if (path_start == std::string::npos) {
    if (database_name.empty()) {
      return std::nullopt;
    }
    url += "/" + database_name;
    return url;
  }

  if (path_start == host_start) {
    return std::nullopt;
  }

  if (path_start == url.size() - 1) {
    if (database_name.empty()) {
      return std::nullopt;
    }
    url += database_name;
  }

  return url;
}

auto MakeSyncStatus(SyncLifecycleState state, std::string status_text,
                    bool initial_pull_active = false, bool initial_pull_completed = false)
    -> SyncStatus {
  return SyncStatus{
      .state = state,
      .status_text = std::move(status_text),
      .initial_pull_active = initial_pull_active,
      .initial_pull_completed = initial_pull_completed,
  };
}

auto ReplicatorStatusText(const CBLReplicatorStatus& status) -> std::string {
  switch (status.activity) {
    case kCBLReplicatorStopped:
      if (status.error.code != 0) {
        return "Sync stopped: " + CbliteErrorMessage(status.error);
      }
      return "Sync stopped";
    case kCBLReplicatorOffline:
      return "Sync offline";
    case kCBLReplicatorConnecting:
      return "Sync connecting";
    case kCBLReplicatorIdle:
      return "Sync idle";
    case kCBLReplicatorBusy:
      return "Sync active";
  }

  return "Sync status unknown";
}

auto MakeCollectionQualifiedName(const cbl::Collection& collection) -> std::string {
  return "`" + collection.scopeName() + "`.`" + collection.name() + "`";
}

}  // namespace

class CbliteDocumentRepository::Impl {
 public:
  explicit Impl(CbliteDocumentRepositoryOptions options) : options_(std::move(options)) {}

  class ScopedCollectionWriteGuard {
   public:
    explicit ScopedCollectionWriteGuard(Impl& impl) : impl_(impl) {
      impl_.suppress_collection_dirty_tracking_.fetch_add(1);
    }

    ~ScopedCollectionWriteGuard() {
      impl_.suppress_collection_dirty_tracking_.fetch_sub(1);
    }

    ScopedCollectionWriteGuard(const ScopedCollectionWriteGuard&) = delete;
    auto operator=(const ScopedCollectionWriteGuard&) -> ScopedCollectionWriteGuard& = delete;

   private:
    Impl& impl_;
  };

  auto IsWorkspaceBackedBySync(std::string_view workspace_id) const -> bool {
    return synced_workspace_ids_.contains(std::string(workspace_id));
  }

  auto GetCollectionForWorkspace(std::string_view workspace_id) -> cbl::Collection& {
    if (IsWorkspaceBackedBySync(workspace_id)) {
      return *collection_;
    }
    return *local_collection_;
  }

  auto GetIndexDocumentIdForWorkspace(std::string_view workspace_id) const -> std::string_view {
    if (IsWorkspaceBackedBySync(workspace_id)) {
      return constants::kDocumentsIndexDocumentId;
    }
    return constants::kLocalDocumentsIndexDocumentId;
  }

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return SaveDocumentResult{.error = error};
    }

    try {
      ScopedCollectionWriteGuard guard(*this);
      spdlog::debug("CBLite SaveDocument: id={} parent={} sort_order={}", document.metadata.id,
                    document.metadata.parent_id.value_or("<root>"), document.metadata.sort_order);
      auto& coll = GetCollectionForWorkspace(document.metadata.workspace_id);
      auto doc = GetMutableDocument(coll, document.metadata.id);
      doc.set("title", Slice(document.metadata.title));
      doc.set("workspace_id", Slice(document.metadata.workspace_id));
      if (document.metadata.parent_id) {
        doc.set("parent_id", Slice(*document.metadata.parent_id));
      } else {
        doc.properties().remove("parent_id");
      }
      doc.set("sort_order", document.metadata.sort_order);
      doc.set("created_at", Slice(document.metadata.created_at));
      doc.set("updated_at", Slice(document.metadata.updated_at));
      doc.set("created_by", Slice(document.metadata.created_by));
      doc.set("updated_by", Slice(document.metadata.updated_by));
      doc.set("content_version", document.metadata.content_version);
      doc.set("raw_snapshot", Slice(document.raw_snapshot_json));

      coll.saveDocument(doc);
      SaveDocumentIndexEntry(document.metadata);
      return SaveDocumentResult{};
    } catch (const CBLError& error) {
      return SaveDocumentResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return SaveDocumentResult{.error =
                                    MakeError(RepositoryErrorCode::kWriteFailed, error.what())};
    }
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return DeleteDocumentResult{.error = error};
    }

    try {
      ScopedCollectionWriteGuard guard(*this);
      auto conflicts = ListConflicts();
      if (conflicts.error) {
        return DeleteDocumentResult{.error = std::move(conflicts.error)};
      }
      for (const auto& conflict : conflicts.conflicts) {
        if (conflict.document_id == page_id) {
          if (auto delete_conflict = DeleteConflict(conflict.id); delete_conflict.error) {
            return DeleteDocumentResult{.error = std::move(delete_conflict.error)};
          }
        }
      }

      spdlog::debug("CBLite DeleteDocument: id={}", page_id);
      auto doc = collection_->getDocument(Slice(page_id));
      if (doc) {
        collection_->deleteDocument(doc);
      }
      auto local_doc = local_collection_->getDocument(Slice(page_id));
      if (local_doc) {
        local_collection_->deleteDocument(local_doc);
      }
      RemoveDocumentIndexEntry(page_id);
      return DeleteDocumentResult{};
    } catch (const CBLError& error) {
      return DeleteDocumentResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return DeleteDocumentResult{.error =
                                      MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = error,
      };
    }

    try {
      spdlog::debug("CBLite LoadDocument: id={}", page_id);
      auto doc = collection_->getDocument(Slice(page_id));
      if (!doc) {
        doc = local_collection_->getDocument(Slice(page_id));
      }
      if (!doc) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Document not found in CBLite"),
        };
      }

      DocumentRecord record;
      record.metadata.id = std::string(page_id);
      // Document properties are accessed via Dict interface
      auto props = doc.properties();
      record.metadata.title = std::string(props["title"].asString());
      record.metadata.workspace_id = std::string(props["workspace_id"].asString());
      if (const auto parent_id = props["parent_id"]; parent_id) {
        record.metadata.parent_id = std::string(parent_id.asString());
      }
      record.metadata.sort_order = static_cast<std::int32_t>(props["sort_order"].asInt());
      record.metadata.created_at = std::string(props["created_at"].asString());
      record.metadata.updated_at = std::string(props["updated_at"].asString());
      record.metadata.created_by = std::string(props["created_by"].asString());
      record.metadata.updated_by = std::string(props["updated_by"].asString());
      record.metadata.content_version = static_cast<std::int64_t>(props["content_version"].asInt());
      if (record.metadata.content_version < 1) {
        record.metadata.content_version = 1;
      }
      record.raw_snapshot_json = std::string(props["raw_snapshot"].asString());

      try {
        auto result =
            rfl::json::read<document::BlockNoteDocumentSnapshot>(record.raw_snapshot_json);
        if (!result) {
          return LoadDocumentResult{
              .document = std::nullopt,
              .error = MakeError(
                  RepositoryErrorCode::kInvalidRecord,
                  std::string("Failed to deserialize snapshot: ") + result.error().what()),
          };
        }
        record.snapshot = result.value();
      } catch (const std::exception& e) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               std::string("Failed to deserialize snapshot: ") + e.what()),
        };
      }

      return LoadDocumentResult{
          .document = std::make_optional(std::move(record)),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

  [[nodiscard]] auto SaveConflict(const DocumentConflictRecord& conflict) -> SaveConflictResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return SaveConflictResult{.error = error};
    }

    try {
      ScopedCollectionWriteGuard guard(*this);
      auto doc = GetMutableDocument(*local_collection_, MakeConflictDocumentId(conflict.id));
      doc.set("document_id", Slice(conflict.document_id));
      doc.set("workspace_id", Slice(conflict.workspace_id));
      doc.set("base_version", conflict.base_version);
      doc.set("local_snapshot", Slice(conflict.local_snapshot));
      doc.set("remote_snapshot", Slice(conflict.remote_snapshot));
      doc.set("local_updated_by", Slice(conflict.local_updated_by));
      doc.set("remote_updated_by", Slice(conflict.remote_updated_by));
      doc.set("detected_at", Slice(conflict.detected_at));
      doc.set("resolution_state", Slice(conflict.resolution_state));

      local_collection_->saveDocument(doc);
      SaveConflictIndexEntry(conflict);
      return SaveConflictResult{};
    } catch (const CBLError& error) {
      return SaveConflictResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return SaveConflictResult{.error =
                                    MakeError(RepositoryErrorCode::kWriteFailed, error.what())};
    }
  }

  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id) -> DeleteConflictResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return DeleteConflictResult{.error = error};
    }

    try {
      ScopedCollectionWriteGuard guard(*this);
      auto doc = local_collection_->getDocument(Slice(MakeConflictDocumentId(conflict_id)));
      if (doc) {
        local_collection_->deleteDocument(doc);
      }
      RemoveConflictIndexEntry(conflict_id);
      return DeleteConflictResult{};
    } catch (const CBLError& error) {
      return DeleteConflictResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return DeleteConflictResult{.error =
                                      MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto LoadConflict(std::string_view conflict_id) -> LoadConflictResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return LoadConflictResult{
          .conflict = std::nullopt,
          .error = error,
      };
    }

    try {
      auto doc = local_collection_->getDocument(Slice(MakeConflictDocumentId(conflict_id)));
      if (!doc) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Conflict not found in CBLite"),
        };
      }

      auto props = doc.properties();
      DocumentConflictRecord record;
      record.id = std::string(conflict_id);
      record.document_id = std::string(props["document_id"].asString());
      record.workspace_id = std::string(props["workspace_id"].asString());
      record.base_version = static_cast<std::int64_t>(props["base_version"].asInt());
      record.local_snapshot = std::string(props["local_snapshot"].asString());
      record.remote_snapshot = std::string(props["remote_snapshot"].asString());
      record.local_updated_by = std::string(props["local_updated_by"].asString());
      record.remote_updated_by = std::string(props["remote_updated_by"].asString());
      record.detected_at = std::string(props["detected_at"].asString());
      record.resolution_state = std::string(props["resolution_state"].asString());
      if (record.resolution_state.empty()) {
        record.resolution_state = "pending";
      }
      if (!IsValidResolutionState(record.resolution_state)) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               "Conflict resolution state is invalid."),
        };
      }

      return LoadConflictResult{
          .conflict = std::make_optional(std::move(record)),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      return LoadConflictResult{
          .conflict = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      return LoadConflictResult{
          .conflict = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

  [[nodiscard]] auto ListConflicts() -> ListConflictsResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return ListConflictsResult{
          .conflicts = {},
          .error = error,
      };
    }

    try {
      std::vector<DocumentConflictRecord> conflicts;

      auto index_doc = local_collection_->getDocument(Slice(constants::kConflictsIndexDocumentId));
      if (!index_doc) {
        return ListConflictsResult{};
      }

      auto props = index_doc.properties();
      for (auto it = props.begin(); it != props.end(); ++it) {
        const auto conflict_id = it.keyString().asString();
        if (conflict_id == constants::kConflictsIndexDocumentId ||
            IsConflictDocumentId(conflict_id)) {
          continue;
        }
        auto loaded = LoadConflict(conflict_id);
        if (!loaded.conflict) {
          continue;
        }
        conflicts.push_back(std::move(*loaded.conflict));
      }

      std::ranges::sort(conflicts,
                        [](const DocumentConflictRecord& lhs, const DocumentConflictRecord& rhs) {
                          if (lhs.detected_at != rhs.detected_at) {
                            return lhs.detected_at < rhs.detected_at;
                          }
                          return lhs.id < rhs.id;
                        });

      return ListConflictsResult{
          .conflicts = std::move(conflicts),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      return ListConflictsResult{
          .conflicts = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      return ListConflictsResult{
          .conflicts = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult {
    return UpdateConflictResolutionState(conflict_id, "resolved");
  }

  [[nodiscard]] auto DismissConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult {
    return UpdateConflictResolutionState(conflict_id, "dismissed");
  }

  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult {
    if (const auto error = EnsureDatabaseOpen()) {
      spdlog::warn("CBLite ListDocuments failed: database not open ({})", error->message);
      return ListDocumentsResult{
          .documents = {},
          .error = error,
      };
    }

    try {
      std::vector<DocumentSummary> documents;

      if (document_index_dirty_.exchange(false)) {
        spdlog::info("CBLite ListDocuments: index marked dirty; rebuilding from collection scan");
        const auto scan_result = ListDocumentsByCollectionScan();
        spdlog::info("CBLite ListDocuments: dirty rebuild returned {} document(s)",
                     scan_result.documents.size());
        return scan_result;
      }

      auto doc = collection_->getDocument(Slice(constants::kDocumentsIndexDocumentId));
      auto local_doc =
          local_collection_->getDocument(Slice(constants::kLocalDocumentsIndexDocumentId));
      if (!doc && !local_doc) {
        spdlog::info("CBLite ListDocuments: no index docs; will rebuild from collection scan");
        const auto scan_result = ListDocumentsByCollectionScan();
        spdlog::info("CBLite ListDocuments: collection scan returned {} document(s)",
                     scan_result.documents.size());
        return scan_result;
      }

      if (doc) {
        auto props = doc.properties();
        for (auto it = props.begin(); it != props.end(); ++it) {
          const auto key = it.keyString().asString();
          if (key == constants::kDocumentsIndexDocumentId ||
              key == constants::kConflictsIndexDocumentId || IsConflictDocumentId(key)) {
            continue;
          }
          auto loaded = LoadDocument(key);
          if (!loaded.document) {
            continue;
          }
          documents.push_back(DocumentSummaryFromMetadata(loaded.document->metadata));
        }
      }

      if (local_doc) {
        auto props = local_doc.properties();
        for (auto it = props.begin(); it != props.end(); ++it) {
          const auto key = it.keyString().asString();
          if (key == constants::kLocalDocumentsIndexDocumentId || IsConflictDocumentId(key)) {
            continue;
          }
          auto loaded = LoadDocument(key);
          if (!loaded.document) {
            continue;
          }
          documents.push_back(DocumentSummaryFromMetadata(loaded.document->metadata));
        }
      }

      std::ranges::sort(documents, [](const DocumentSummary& lhs, const DocumentSummary& rhs) {
        if (lhs.sort_order != rhs.sort_order) {
          return lhs.sort_order < rhs.sort_order;
        }
        return lhs.title < rhs.title;
      });

      spdlog::info("CBLite ListDocuments: index scan returned {} document(s)", documents.size());
      return ListDocumentsResult{
          .documents = std::move(documents),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      const auto message = CbliteErrorMessage(error);
      spdlog::error("CBLite ListDocuments: CBLite error: {}", message);
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, message),
      };
    } catch (const std::exception& error) {
      spdlog::error("CBLite ListDocuments: exception: {}", error.what());
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

  [[nodiscard]] auto SetSyncAccessToken(std::string access_token) -> SyncOperationResult {
    const bool changed = access_token_ != access_token;
    access_token_ = std::move(access_token);
    if (changed) {
      replicator_dirty_ = true;
      if (replicator_) {
        ResetReplicator();
      }
    }
    return {};
  }

  [[nodiscard]] auto ApplySyncBootstrap(const sync::SyncBootstrap& bootstrap)
      -> SyncOperationResult {
    if (bootstrap.auth_mode.trimmed() !=
        QString::fromUtf8(kSupportedSyncAuthMode.data(),
                          static_cast<qsizetype>(kSupportedSyncAuthMode.size()))) {
      SetSyncStatus(
          MakeSyncStatus(SyncLifecycleState::kError, "Sync bootstrap auth mode is unsupported"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync bootstrap auth mode is unsupported."),
      };
    }

    if (bootstrap.gateway_url.trimmed().isEmpty()) {
      SetSyncStatus(
          MakeSyncStatus(SyncLifecycleState::kError, "Sync bootstrap is missing gateway URL"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync bootstrap is missing gateway URL."),
      };
    }

    if (const auto replication_url = NormalizeGatewayReplicationUrl(bootstrap); !replication_url) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync gateway URL is invalid for replication"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync gateway URL is invalid for replication."),
      };
    }

    const bool changed = bootstrap_.gateway_url != bootstrap.gateway_url ||
                         bootstrap_.database_name != bootstrap.database_name ||
                         bootstrap_.channels != bootstrap.channels ||
                         bootstrap_.auth_mode != bootstrap.auth_mode ||
                         bootstrap_.token_passthrough != bootstrap.token_passthrough;
    bootstrap_ = bootstrap;
    UpdateSyncedWorkspaceIdsFromBootstrap();
    if (const auto promote_error = PromoteLocalDocumentsToSyncCollections(); promote_error) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError, promote_error->message));
      return SyncOperationResult{.error = std::move(promote_error)};
    }
    if (changed) {
      replicator_dirty_ = true;
      if (replicator_) {
        ResetReplicator();
      }
    }
    SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kConfigured, "Sync bootstrap configured"));
    return {};
  }

  [[nodiscard]] auto StartSync() -> SyncOperationResult {
    if (const auto error = EnsureDatabaseOpen()) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError, error->message));
      return SyncOperationResult{.error = std::move(error)};
    }

    if (bootstrap_.gateway_url.trimmed().isEmpty()) {
      SetSyncStatus(
          MakeSyncStatus(SyncLifecycleState::kError, "Sync cannot start without bootstrap"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync cannot start without a configured bootstrap."),
      };
    }

    if (access_token_.empty()) {
      SetSyncStatus(
          MakeSyncStatus(SyncLifecycleState::kError, "Sync cannot start without access token"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync cannot start without an access token."),
      };
    }

    if (const auto replication_url = NormalizeGatewayReplicationUrl(bootstrap_); !replication_url) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync gateway URL is invalid for replication"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync gateway URL is invalid for replication."),
      };
    }

    if (replicator_ && !replicator_dirty_) {
      const auto status = replicator_->status();
      if (status.activity != kCBLReplicatorStopped) {
        const bool initial_pull_completed = status.activity == kCBLReplicatorIdle;
        SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kRunning, ReplicatorStatusText(status),
                                     !initial_pull_completed, initial_pull_completed));
        return {};
      }
    }

    auto replication_url = NormalizeGatewayReplicationUrl(bootstrap_);
    try {
      spdlog::info("CBLite StartSync: starting replicator to {} for collection '{}'",
                   *replication_url, constants::kDocumentsCollectionName);
      spdlog::info("CBLite StartSync: continuous=true channels={}", bootstrap_.channels.size());
      for (const auto& channel : bootstrap_.channels) {
        spdlog::info("CBLite StartSync: channel='{}'", channel.toStdString());
      }
      ResetReplicator();

      cbl::ReplicationCollection collection_config(*collection_);
      for (const auto& channel : bootstrap_.channels) {
        const auto trimmed_channel = channel.trimmed();
        if (!trimmed_channel.isEmpty()) {
          collection_config.channels.append(trimmed_channel.toStdString());
        }
      }
      collection_config.conflictResolver =
          [this](fleece::slice doc_id, const cbl::Document local_document,
                 const cbl::Document remote_document) -> cbl::Document {
        auto conflict = BuildConflictRecord(doc_id, local_document, remote_document);
        if (const auto save_result = SaveConflict(conflict); save_result.error) {
          spdlog::error(
              "CBLite conflict resolver: failed to persist sync conflict for document {}: {}",
              doc_id.asString(), save_result.error->message);
        } else {
          spdlog::warn(
              "CBLite conflict resolver: persisted sync conflict for document {} in workspace {}",
              conflict.document_id, conflict.workspace_id);
        }

        if (local_document) {
          return local_document;
        }
        return remote_document;
      };

      std::vector<cbl::ReplicationCollection> collections;
      collections.push_back(std::move(collection_config));

      auto endpoint = cbl::Endpoint::urlEndpoint(Slice(*replication_url));
      cbl::ReplicatorConfiguration replicator_config(std::move(collections), endpoint);
      replicator_config.continuous = true;
      replicator_config.headers.set(Slice("Authorization"), Slice(MakeBearerHeader(access_token_)));

      auto replicator = std::make_unique<cbl::Replicator>(replicator_config);
      change_listener_ =
          replicator->addChangeListener([this](cbl::Replicator, const CBLReplicatorStatus& status) {
            const auto status_text = ReplicatorStatusText(status);
            if (status.activity == kCBLReplicatorStopped && status.error.code != 0) {
              spdlog::warn("CBLite replicator status: {}", status_text);
              SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError, status_text));
              return;
            }
            const bool initial_pull_completed = status.activity == kCBLReplicatorIdle;
            spdlog::info("CBLite replicator status: {}", status_text);
            SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kRunning, status_text,
                                         !initial_pull_completed, initial_pull_completed));
          });

      document_listener_ = replicator->addDocumentReplicationListener(
          [this](cbl::Replicator, bool is_push,
                 const std::vector<CBLReplicatedDocument>& documents) {
            const char* direction = is_push ? "pushed" : "pulled";
            bool has_remote_document_changes = false;
            for (const auto& document : documents) {
              const auto doc_id = std::string(fleece::slice(document.ID).asString());
              if (document.error.code != 0) {
                spdlog::warn("CBLite replicator document {}: id={} error={}", direction, doc_id,
                             CbliteErrorMessage(document.error));
              } else if (document.flags & kCBLDocumentFlagsDeleted) {
                spdlog::info("CBLite replicator document deleted: id={}", doc_id);
                if (!is_push) {
                  has_remote_document_changes = true;
                }
              } else {
                spdlog::info("CBLite replicator document {}: id={}", direction, doc_id);
                if (!is_push) {
                  has_remote_document_changes = true;
                }
              }
            }

            if (has_remote_document_changes) {
              document_index_dirty_.store(true);
              spdlog::info(
                  "CBLite replicator document listener: marked document index dirty after remote "
                  "changes");
            }
          });

      replicator->start();
      replicator_ = std::move(replicator);
      replicator_dirty_ = false;
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kRunning,
                                   "Sync replication started for " + replication_url.value(), true,
                                   false));
      spdlog::info("CBLite StartSync: replicator started for {}", replication_url.value());
    } catch (const CBLError& error) {
      spdlog::error("CBLite StartSync: CBLite error: {}", CbliteErrorMessage(error));
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError, CbliteErrorMessage(error)));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kOpenFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      spdlog::error("CBLite StartSync: exception: {}", error.what());
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError, error.what()));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kOpenFailed, error.what()),
      };
    }

    return {};
  }

  [[nodiscard]] auto StopSync() -> SyncOperationResult {
    if (replicator_) {
      ResetReplicator();
    }
    SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kConfigured, "Sync stopped"));
    return {};
  }

  [[nodiscard]] auto GetSyncStatus() -> SyncStatus {
    std::lock_guard<std::mutex> lock(sync_status_mutex_);
    auto status = sync_status_;
    status.has_conflicts = false;
    status.conflict_count = 0;

    auto conflicts = ListConflicts();
    if (!conflicts.error) {
      status.conflict_count =
          static_cast<std::size_t>(std::ranges::count_if(conflicts.conflicts, IsPendingConflict));
      status.has_conflicts = status.conflict_count > 0;
    }

    return status;
  }

  [[nodiscard]] auto SaveWorkspaceRoot(const WorkspaceRootRecord& workspace_root)
      -> SaveWorkspaceRootResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return SaveWorkspaceRootResult{.error = error};
    }
    if (workspace_root.workspace_id.empty()) {
      return SaveWorkspaceRootResult{
          .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                             "Workspace root record requires a non-empty workspace id."),
      };
    }

    try {
      ScopedCollectionWriteGuard guard(*this);
      const auto document_id = MakeWorkspaceRootDocumentId(workspace_root.workspace_id);
      auto doc = GetMutableDocument(*collection_, document_id);
      doc.set("type", Slice(constants::kWorkspaceRootDocumentType));
      doc.set("workspace_id", Slice(workspace_root.workspace_id));
      doc.set("title", Slice(workspace_root.title));
      doc.set("created_at", Slice(workspace_root.created_at));
      doc.set("schema_version", workspace_root.schema_version);

      collection_->saveDocument(doc);
      spdlog::info("CBLite SaveWorkspaceRoot: workspace_id={} materialized locally",
                   workspace_root.workspace_id);
      return SaveWorkspaceRootResult{};
    } catch (const CBLError& error) {
      return SaveWorkspaceRootResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return SaveWorkspaceRootResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, error.what())};
    }
  }

  [[nodiscard]] auto LoadWorkspaceRoot(std::string_view workspace_id)
      -> std::optional<WorkspaceRootRecord> {
    if (const auto error = EnsureDatabaseOpen()) {
      return std::nullopt;
    }

    try {
      auto doc = collection_->getDocument(Slice(MakeWorkspaceRootDocumentId(workspace_id)));
      if (!doc) {
        return std::nullopt;
      }

      auto props = doc.properties();
      WorkspaceRootRecord record;
      record.workspace_id = std::string(props["workspace_id"].asString());
      record.title = std::string(props["title"].asString());
      record.created_at = std::string(props["created_at"].asString());
      record.schema_version = static_cast<std::int64_t>(props["schema_version"].asInt());
      if (record.schema_version < 1) {
        record.schema_version = 1;
      }
      if (record.workspace_id.empty()) {
        record.workspace_id = std::string(workspace_id);
      }
      return std::make_optional(std::move(record));
    } catch (const CBLError&) {
      return std::nullopt;
    } catch (const std::exception&) {
      return std::nullopt;
    }
  }

  [[nodiscard]] auto ListWorkspaces() -> ListWorkspacesResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return ListWorkspacesResult{.workspace_ids = {}, .error = error};
    }

    try {
      std::vector<std::string> workspace_ids;
      const auto query_text = "SELECT META().id AS doc_id FROM " +
                              MakeCollectionQualifiedName(*collection_) +
                              " WHERE type = 'workspace'";
      auto query = database_->createQuery(kCBLN1QLLanguage, Slice(query_text));
      auto results = query.execute();
      for (const auto& row : results) {
        const auto value = row["doc_id"];
        if (!value) {
          continue;
        }
        const auto document_id = std::string(value.asString());
        if (!IsWorkspaceRootDocumentId(document_id)) {
          continue;
        }
        auto loaded =
            LoadWorkspaceRoot(document_id.substr(constants::kWorkspaceDocumentIdPrefix.size()));
        if (!loaded) {
          continue;
        }
        workspace_ids.push_back(loaded->workspace_id);
      }

      std::ranges::sort(workspace_ids);
      return ListWorkspacesResult{.workspace_ids = std::move(workspace_ids), .error = std::nullopt};
    } catch (const CBLError& error) {
      return ListWorkspacesResult{
          .workspace_ids = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return ListWorkspacesResult{
          .workspace_ids = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

 private:
  [[nodiscard]] auto ListDocumentsByCollectionScan() -> ListDocumentsResult {
    try {
      std::vector<DocumentSummary> documents;

      // Synced collection scan
      const auto query_text_sync =
          "SELECT META().id AS doc_id FROM " + MakeCollectionQualifiedName(*collection_) +
          " WHERE raw_snapshot IS NOT MISSING AND workspace_id IS NOT MISSING";
      auto query_sync = database_->createQuery(kCBLN1QLLanguage, Slice(query_text_sync));
      auto results_sync = query_sync.execute();
      for (const auto& row : results_sync) {
        const auto value = row["doc_id"];
        if (!value)
          continue;
        const auto document_id = value.asString();
        if (document_id.empty())
          continue;
        auto loaded = LoadDocument(document_id);
        if (!loaded.document)
          continue;
        documents.push_back(DocumentSummaryFromMetadata(loaded.document->metadata));
      }

      // Local collection scan
      const auto query_text_local =
          "SELECT META().id AS doc_id FROM " + MakeCollectionQualifiedName(*local_collection_) +
          " WHERE raw_snapshot IS NOT MISSING AND workspace_id IS NOT MISSING";
      auto query_local = database_->createQuery(kCBLN1QLLanguage, Slice(query_text_local));
      auto results_local = query_local.execute();
      for (const auto& row : results_local) {
        const auto value = row["doc_id"];
        if (!value)
          continue;
        const auto document_id = value.asString();
        if (document_id.empty())
          continue;
        auto loaded = LoadDocument(document_id);
        if (!loaded.document)
          continue;
        documents.push_back(DocumentSummaryFromMetadata(loaded.document->metadata));
      }

      std::ranges::sort(documents, [](const DocumentSummary& lhs, const DocumentSummary& rhs) {
        if (lhs.sort_order != rhs.sort_order) {
          return lhs.sort_order < rhs.sort_order;
        }
        return lhs.title < rhs.title;
      });

      for (const auto& summary : documents) {
        SaveDocumentIndexEntry(document::PageMetadata{
            .id = summary.id,
            .schema_version = document::SchemaVersion::kV1,
            .title = summary.title,
            .workspace_id = summary.workspace_id,
            .parent_id = summary.parent_id,
            .sort_order = summary.sort_order,
            .created_at = summary.created_at,
            .updated_at = summary.updated_at,
            .created_by = summary.created_by,
            .updated_by = summary.updated_by,
            .content_version = summary.content_version,
        });
      }

      return ListDocumentsResult{
          .documents = std::move(documents),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

  void SaveDocumentIndexEntry(const document::PageMetadata& metadata) {
    spdlog::debug("CBLite SaveDocumentIndexEntry: id={} title={}", metadata.id, metadata.title);
    auto& coll = GetCollectionForWorkspace(metadata.workspace_id);
    const auto index_id = GetIndexDocumentIdForWorkspace(metadata.workspace_id);
    auto index_doc = GetMutableDocument(coll, index_id);
    index_doc.set(Slice(metadata.id), Slice(metadata.title));
    coll.saveDocument(index_doc);
  }

  void RemoveDocumentIndexEntry(std::string_view document_id) {
    spdlog::debug("CBLite RemoveDocumentIndexEntry: id={}", document_id);

    // Remove from local collection index
    auto local_index_doc =
        GetMutableDocument(*local_collection_, constants::kLocalDocumentsIndexDocumentId);
    local_index_doc.properties().remove(Slice(document_id));
    local_collection_->saveDocument(local_index_doc);

    // Remove from synced collection index
    auto sync_index_doc = GetMutableDocument(*collection_, constants::kDocumentsIndexDocumentId);
    sync_index_doc.properties().remove(Slice(document_id));
    collection_->saveDocument(sync_index_doc);
  }

  void SaveConflictIndexEntry(const DocumentConflictRecord& conflict) {
    auto index_doc = GetMutableDocument(*local_collection_, constants::kConflictsIndexDocumentId);
    index_doc.set(Slice(conflict.id), Slice(conflict.document_id));
    local_collection_->saveDocument(index_doc);
  }

  void RemoveConflictIndexEntry(std::string_view conflict_id) {
    auto index_doc = GetMutableDocument(*local_collection_, constants::kConflictsIndexDocumentId);
    index_doc.properties().remove(Slice(conflict_id));
    local_collection_->saveDocument(index_doc);
  }

  [[nodiscard]] auto UpdateConflictResolutionState(std::string_view conflict_id,
                                                   std::string resolution_state)
      -> UpdateConflictResolutionResult {
    auto loaded = LoadConflict(conflict_id);
    if (!loaded.conflict) {
      return UpdateConflictResolutionResult{.error = std::move(loaded.error)};
    }

    loaded.conflict->resolution_state = std::move(resolution_state);
    auto saved = SaveConflict(*loaded.conflict);
    return UpdateConflictResolutionResult{.error = std::move(saved.error)};
  }

  auto BuildConflictRecord(fleece::slice doc_id, const cbl::Document& local_document,
                           const cbl::Document& remote_document) -> DocumentConflictRecord {
    const auto local_timestamp = ConflictTimestamp(local_document);
    const auto remote_timestamp = ConflictTimestamp(remote_document);
    const auto detected_timestamp = std::max(local_timestamp, remote_timestamp);

    return DocumentConflictRecord{
        .id = doc_id.asString() + "-" + std::to_string(detected_timestamp),
        .document_id = doc_id.asString(),
        .workspace_id = ConflictWorkspaceId(local_document, remote_document),
        .base_version = ConflictContentVersion(local_document, remote_document),
        .local_snapshot = ConflictSnapshotJson(local_document),
        .remote_snapshot = ConflictSnapshotJson(remote_document),
        .local_updated_by = ConflictActor(local_document),
        .remote_updated_by = ConflictActor(remote_document),
        .detected_at = TimestampToIso8601(detected_timestamp),
        .resolution_state = "pending",
    };
  }

  void UpdateSyncedWorkspaceIdsFromBootstrap() {
    synced_workspace_ids_.clear();
    for (const auto& channel : bootstrap_.channels) {
      const auto trimmed_channel = channel.trimmed();
      if (!trimmed_channel.startsWith(QStringLiteral("workspace:"))) {
        continue;
      }

      const auto workspace_id =
          trimmed_channel.sliced(QStringLiteral("workspace:").size()).trimmed();
      if (!workspace_id.isEmpty()) {
        synced_workspace_ids_.insert(workspace_id.toStdString());
      }
    }
  }

  [[nodiscard]] auto PromoteLocalDocumentsToSyncCollections() -> std::optional<RepositoryError> {
    if (synced_workspace_ids_.empty()) {
      return std::nullopt;
    }

    try {
      ScopedCollectionWriteGuard guard(*this);
      bool migrated_any = false;

      for (const auto& workspace_id : synced_workspace_ids_) {
        const auto query_text =
            "SELECT META().id AS doc_id FROM " + MakeCollectionQualifiedName(*local_collection_) +
            " WHERE raw_snapshot IS NOT MISSING AND workspace_id = '" + workspace_id + "'";
        auto query = database_->createQuery(kCBLN1QLLanguage, Slice(query_text));
        auto results = query.execute();
        for (const auto& row : results) {
          const auto value = row["doc_id"];
          if (!value) {
            continue;
          }

          const auto document_id = std::string(value.asString());
          if (document_id.empty()) {
            continue;
          }

          auto local_doc = local_collection_->getDocument(Slice(document_id));
          if (!local_doc) {
            continue;
          }

          cbl::MutableDocument sync_doc(Slice(document_id));
          sync_doc.setPropertiesAsJSON(local_doc.propertiesAsJSON());
          collection_->saveDocument(sync_doc);
          local_collection_->deleteDocument(local_doc);
          migrated_any = true;
          spdlog::info(
              "CBLite PromoteLocalDocumentsToSyncCollections: promoted document {} for workspace "
              "{}",
              document_id, workspace_id);
        }
      }

      if (migrated_any) {
        document_index_dirty_.store(true);
      }
      return std::nullopt;
    } catch (const CBLError& error) {
      return MakeError(RepositoryErrorCode::kWriteFailed, CbliteErrorMessage(error));
    } catch (const std::exception& error) {
      return MakeError(RepositoryErrorCode::kWriteFailed, error.what());
    }
  }

  [[nodiscard]] auto EnsureDatabaseOpen() -> std::optional<RepositoryError> {
    if (database_) {
      return std::nullopt;
    }

    try {
      const auto normalized_directory =
          std::filesystem::absolute(options_.database_directory).lexically_normal();
      if (const auto error = EnsureDirectoryExists(normalized_directory)) {
        return error;
      }

      auto config = CBLDatabaseConfiguration_Default();
      database_directory_ = normalized_directory.string();
      config.directory = Slice(database_directory_);
      spdlog::info("CBLite opening database: name={} directory={}", options_.database_name,
                   database_directory_);
      database_ = std::make_unique<cbl::Database>(Slice(options_.database_name), config);
      if (!static_cast<bool>(*database_)) {
        return MakeError(RepositoryErrorCode::kOpenFailed, "Couchbase Lite database did not open.");
      }
      spdlog::info("CBLite database opened: path={}", database_->path());
      spdlog::info("CBLite creating/opening collection: {}", constants::kDocumentsCollectionName);
      collection_ = std::make_unique<cbl::Collection>(
          database_->createCollection(Slice(constants::kDocumentsCollectionName)));
      spdlog::info("CBLite collection ready: {}", constants::kDocumentsCollectionName);
      collection_change_listener_ =
          collection_->addChangeListener([this](cbl::CollectionChange* change) {
            if (change == nullptr) {
              return;
            }
            if (suppress_collection_dirty_tracking_.load() > 0) {
              return;
            }

            bool has_document_changes = false;
            for (const auto& doc_id : change->docIDs()) {
              const auto changed_id = std::string(doc_id.asString());
              if (changed_id == constants::kDocumentsIndexDocumentId ||
                  changed_id == constants::kConflictsIndexDocumentId ||
                  IsConflictDocumentId(changed_id) || IsWorkspaceRootDocumentId(changed_id)) {
                continue;
              }
              has_document_changes = true;
              break;
            }

            if (has_document_changes) {
              document_index_dirty_.store(true);
              spdlog::info(
                  "CBLite collection listener: marked document index dirty after external "
                  "collection changes");
            }
          });

      spdlog::info("CBLite creating/opening collection: {}",
                   constants::kLocalDocumentsCollectionName);
      local_collection_ = std::make_unique<cbl::Collection>(
          database_->createCollection(Slice(constants::kLocalDocumentsCollectionName)));
      spdlog::info("CBLite collection ready: {}", constants::kLocalDocumentsCollectionName);
      local_collection_change_listener_ =
          local_collection_->addChangeListener([this](cbl::CollectionChange* change) {
            if (change == nullptr) {
              return;
            }
            if (suppress_collection_dirty_tracking_.load() > 0) {
              return;
            }

            bool has_document_changes = false;
            for (const auto& doc_id : change->docIDs()) {
              const auto changed_id = std::string(doc_id.asString());
              if (changed_id == constants::kLocalDocumentsIndexDocumentId ||
                  IsConflictDocumentId(changed_id) || IsWorkspaceRootDocumentId(changed_id)) {
                continue;
              }
              has_document_changes = true;
              break;
            }

            if (has_document_changes) {
              document_index_dirty_.store(true);
              spdlog::info(
                  "CBLite collection listener: marked document index dirty after external local "
                  "collection changes");
            }
          });

      return std::nullopt;
    } catch (const CBLError& error) {
      return MakeError(RepositoryErrorCode::kOpenFailed, CbliteErrorMessage(error));
    } catch (const std::exception& error) {
      return MakeError(RepositoryErrorCode::kOpenFailed, error.what());
    }
  }

  void SetSyncStatus(SyncStatus status) {
    std::lock_guard<std::mutex> lock(sync_status_mutex_);
    sync_status_ = std::move(status);
  }

  void ResetReplicator() {
    change_listener_.remove();

    if (replicator_) {
      spdlog::info("CBLite ResetReplicator: stopping running replicator");
      replicator_->stop();
      replicator_.reset();
      spdlog::info("CBLite ResetReplicator: replicator stopped");
    }
  }

  CbliteDocumentRepositoryOptions options_;
  std::string database_directory_;
  std::unique_ptr<cbl::Database> database_;
  std::unique_ptr<cbl::Collection> collection_;
  std::unique_ptr<cbl::Collection> local_collection_;
  std::unique_ptr<cbl::Replicator> replicator_;
  cbl::Replicator::ChangeListener change_listener_;
  cbl::Replicator::DocumentReplicationListener document_listener_;
  cbl::Collection::CollectionChangeListener collection_change_listener_;
  cbl::Collection::CollectionChangeListener local_collection_change_listener_;
  std::atomic_bool document_index_dirty_{false};
  std::atomic_int suppress_collection_dirty_tracking_{0};
  mutable std::mutex sync_status_mutex_;
  std::set<std::string> synced_workspace_ids_;
  std::string access_token_;
  sync::SyncBootstrap bootstrap_;
  bool replicator_dirty_{true};
  SyncStatus sync_status_;
};

CbliteDocumentRepository::CbliteDocumentRepository(CbliteDocumentRepositoryOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

CbliteDocumentRepository::~CbliteDocumentRepository() = default;

auto CbliteDocumentRepository::SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
  return impl_->SaveDocument(document);
}

auto CbliteDocumentRepository::DeleteDocument(std::string_view page_id) -> DeleteDocumentResult {
  return impl_->DeleteDocument(page_id);
}

auto CbliteDocumentRepository::LoadDocument(std::string_view page_id) -> LoadDocumentResult {
  return impl_->LoadDocument(page_id);
}

auto CbliteDocumentRepository::ListDocuments() -> ListDocumentsResult {
  return impl_->ListDocuments();
}

auto CbliteDocumentRepository::SaveConflict(const DocumentConflictRecord& conflict)
    -> SaveConflictResult {
  return impl_->SaveConflict(conflict);
}

auto CbliteDocumentRepository::DeleteConflict(std::string_view conflict_id)
    -> DeleteConflictResult {
  return impl_->DeleteConflict(conflict_id);
}

auto CbliteDocumentRepository::LoadConflict(std::string_view conflict_id) -> LoadConflictResult {
  return impl_->LoadConflict(conflict_id);
}

auto CbliteDocumentRepository::ListConflicts() -> ListConflictsResult {
  return impl_->ListConflicts();
}

auto CbliteDocumentRepository::ResolveConflict(std::string_view conflict_id)
    -> UpdateConflictResolutionResult {
  return impl_->ResolveConflict(conflict_id);
}

auto CbliteDocumentRepository::DismissConflict(std::string_view conflict_id)
    -> UpdateConflictResolutionResult {
  return impl_->DismissConflict(conflict_id);
}

auto CbliteDocumentRepository::SupportsSync() const -> bool {
  return true;
}

auto CbliteDocumentRepository::SetSyncAccessToken(std::string access_token) -> SyncOperationResult {
  return impl_->SetSyncAccessToken(std::move(access_token));
}

auto CbliteDocumentRepository::ApplySyncBootstrap(const sync::SyncBootstrap& bootstrap)
    -> SyncOperationResult {
  return impl_->ApplySyncBootstrap(bootstrap);
}

auto CbliteDocumentRepository::StartSync() -> SyncOperationResult {
  return impl_->StartSync();
}

auto CbliteDocumentRepository::StopSync() -> SyncOperationResult {
  return impl_->StopSync();
}

auto CbliteDocumentRepository::GetSyncStatus() const -> SyncStatus {
  return impl_->GetSyncStatus();
}

auto CbliteDocumentRepository::SaveWorkspaceRoot(const WorkspaceRootRecord& workspace_root)
    -> SaveWorkspaceRootResult {
  return impl_->SaveWorkspaceRoot(workspace_root);
}

auto CbliteDocumentRepository::LoadWorkspaceRoot(std::string_view workspace_id)
    -> std::optional<WorkspaceRootRecord> {
  return impl_->LoadWorkspaceRoot(workspace_id);
}

auto CbliteDocumentRepository::ListWorkspaces() -> ListWorkspacesResult {
  return impl_->ListWorkspaces();
}

}  // namespace cppwiki::storage
