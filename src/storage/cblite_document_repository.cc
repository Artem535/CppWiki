// Suppress upstream header warnings before including CBLite headers
#include "core/constants.h"
#include "storage/cblite_document_repository.h"

#include <cbl++/CouchbaseLite.hh>
#include <rfl/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <mutex>
#include <utility>

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
  std::string result = "Couchbase Lite error domain=" +
                       std::to_string(static_cast<int>(error.domain)) +
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

auto MakeSyncStatus(SyncLifecycleState state, std::string status_text) -> SyncStatus {
  return SyncStatus{
      .state = state,
      .status_text = std::move(status_text),
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

}  // namespace

class CbliteDocumentRepository::Impl {
 public:
  explicit Impl(CbliteDocumentRepositoryOptions options) : options_(std::move(options)) {}

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return SaveDocumentResult{.error = error};
    }

    try {
      spdlog::debug("CBLite SaveDocument: id={} parent={} sort_order={}",
                    document.metadata.id,
                    document.metadata.parent_id.value_or("<root>"),
                    document.metadata.sort_order);
      auto doc = GetMutableDocument(*collection_, document.metadata.id);
      doc.set("title", Slice(document.metadata.title));
      if (document.metadata.parent_id) {
        doc.set("parent_id", Slice(*document.metadata.parent_id));
      } else {
        doc.properties().remove("parent_id");
      }
      doc.set("sort_order", document.metadata.sort_order);
      doc.set("created_at", Slice(document.metadata.created_at));
      doc.set("updated_at", Slice(document.metadata.updated_at));
      doc.set("raw_snapshot", Slice(document.raw_snapshot_json));

      collection_->saveDocument(doc);
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
      spdlog::debug("CBLite DeleteDocument: id={}", page_id);
      auto doc = collection_->getDocument(Slice(page_id));
      if (doc) {
        collection_->deleteDocument(doc);
      }
      RemoveDocumentIndexEntry(page_id);
      return DeleteDocumentResult{};
    } catch (const CBLError& error) {
      return DeleteDocumentResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return DeleteDocumentResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
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
      if (const auto parent_id = props["parent_id"]; parent_id) {
        record.metadata.parent_id = std::string(parent_id.asString());
      }
      record.metadata.sort_order = static_cast<std::int32_t>(props["sort_order"].asInt());
      record.metadata.created_at = std::string(props["created_at"].asString());
      record.metadata.updated_at = std::string(props["updated_at"].asString());
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

  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return ListDocumentsResult{
          .documents = {},
          .error = error,
      };
    }

    try {
      std::vector<DocumentSummary> documents;

      spdlog::debug("CBLite ListDocuments: loading index doc {}", constants::kDocumentsIndexDocumentId);
      auto doc = collection_->getDocument(Slice(constants::kDocumentsIndexDocumentId));
      if (!doc) {
        spdlog::debug("CBLite ListDocuments: index doc not found, returning empty list");
        return ListDocumentsResult{};
      }

      auto props = doc.properties();
      for (auto it = props.begin(); it != props.end(); ++it) {
        auto loaded = LoadDocument(it.keyString().asString());
        if (!loaded.document) {
          continue;
        }
        documents.push_back(DocumentSummaryFromMetadata(loaded.document->metadata));
      }

      std::ranges::sort(documents, [](const DocumentSummary& lhs, const DocumentSummary& rhs) {
        if (lhs.sort_order != rhs.sort_order) {
          return lhs.sort_order < rhs.sort_order;
        }
        return lhs.title < rhs.title;
      });

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

  [[nodiscard]] auto ApplySyncBootstrap(const sync::SyncBootstrap& bootstrap) -> SyncOperationResult {
    if (bootstrap.auth_mode.trimmed() != QString::fromUtf8(kSupportedSyncAuthMode.data(),
                                                           static_cast<qsizetype>(kSupportedSyncAuthMode.size()))) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync bootstrap auth mode is unsupported"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync bootstrap auth mode is unsupported."),
      };
    }

    if (bootstrap.gateway_url.trimmed().isEmpty()) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync bootstrap is missing gateway URL"));
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
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync cannot start without bootstrap"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync cannot start without a configured bootstrap."),
      };
    }

    if (access_token_.empty()) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync cannot start without access token"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync cannot start without an access token."),
      };
    }

    if (replicator_ && !replicator_dirty_) {
      const auto status = replicator_->status();
      if (status.activity != kCBLReplicatorStopped) {
        SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kRunning, ReplicatorStatusText(status)));
        return {};
      }
    }

    auto replication_url = NormalizeGatewayReplicationUrl(bootstrap_);
    if (!replication_url) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                   "Sync gateway URL is invalid for replication"));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kUnsupported,
                             "Sync gateway URL is invalid for replication."),
      };
    }

    try {
      ResetReplicator();

      cbl::CollectionConfiguration collection_config(*collection_);
      for (const auto& channel : bootstrap_.channels) {
        const auto trimmed_channel = channel.trimmed();
        if (!trimmed_channel.isEmpty()) {
          collection_config.channels.append(trimmed_channel.toStdString());
        }
      }

      std::vector<cbl::CollectionConfiguration> collections;
      collections.push_back(std::move(collection_config));

      auto endpoint = cbl::Endpoint::urlEndpoint(Slice(*replication_url));
      cbl::ReplicatorConfiguration replicator_config(std::move(collections), endpoint);
      replicator_config.continuous = true;
      replicator_config.headers.set(Slice("Authorization"), Slice(MakeBearerHeader(access_token_)));

      auto replicator = std::make_unique<cbl::Replicator>(replicator_config);
      change_listener_ = replicator->addChangeListener(
          [this](cbl::Replicator, const CBLReplicatorStatus& status) {
            if (status.activity == kCBLReplicatorStopped && status.error.code != 0) {
              SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError,
                                           ReplicatorStatusText(status)));
              return;
            }

            SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kRunning,
                                         ReplicatorStatusText(status)));
          });

      replicator->start();
      replicator_ = std::move(replicator);
      replicator_dirty_ = false;
      SetSyncStatus(MakeSyncStatus(
          SyncLifecycleState::kRunning,
          "Sync replication started for " + replication_url.value()));
    } catch (const CBLError& error) {
      SetSyncStatus(MakeSyncStatus(SyncLifecycleState::kError, CbliteErrorMessage(error)));
      return SyncOperationResult{
          .error = MakeError(RepositoryErrorCode::kOpenFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
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

  [[nodiscard]] auto GetSyncStatus() const -> SyncStatus {
    std::lock_guard<std::mutex> lock(sync_status_mutex_);
    return sync_status_;
  }

 private:
  void SaveDocumentIndexEntry(const document::PageMetadata& metadata) {
    spdlog::debug("CBLite SaveDocumentIndexEntry: id={} title={}", metadata.id, metadata.title);
    auto index_doc = GetMutableDocument(*collection_, constants::kDocumentsIndexDocumentId);
    index_doc.set(Slice(metadata.id), Slice(metadata.title));
    collection_->saveDocument(index_doc);
  }

  void RemoveDocumentIndexEntry(std::string_view document_id) {
    spdlog::debug("CBLite RemoveDocumentIndexEntry: id={}", document_id);
    auto index_doc = GetMutableDocument(*collection_, constants::kDocumentsIndexDocumentId);
    index_doc.properties().remove(Slice(document_id));
    collection_->saveDocument(index_doc);
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
      spdlog::info("CBLite opening database: name={} directory={}",
                   options_.database_name,
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
      replicator_->stop();
      replicator_.reset();
    }
  }

  CbliteDocumentRepositoryOptions options_;
  std::string database_directory_;
  std::unique_ptr<cbl::Database> database_;
  std::unique_ptr<cbl::Collection> collection_;
  std::unique_ptr<cbl::Replicator> replicator_;
  cbl::Replicator::ChangeListener change_listener_;
  mutable std::mutex sync_status_mutex_;
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

auto CbliteDocumentRepository::SupportsSync() const -> bool { return true; }

auto CbliteDocumentRepository::SetSyncAccessToken(std::string access_token)
    -> SyncOperationResult {
  return impl_->SetSyncAccessToken(std::move(access_token));
}

auto CbliteDocumentRepository::ApplySyncBootstrap(const sync::SyncBootstrap& bootstrap)
    -> SyncOperationResult {
  return impl_->ApplySyncBootstrap(bootstrap);
}

auto CbliteDocumentRepository::StartSync() -> SyncOperationResult { return impl_->StartSync(); }

auto CbliteDocumentRepository::StopSync() -> SyncOperationResult { return impl_->StopSync(); }

auto CbliteDocumentRepository::GetSyncStatus() const -> SyncStatus { return impl_->GetSyncStatus(); }

}  // namespace cppwiki::storage
