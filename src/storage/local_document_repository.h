#ifndef CPPWIKI_SRC_STORAGE_LOCAL_DOCUMENT_REPOSITORY_H_
#define CPPWIKI_SRC_STORAGE_LOCAL_DOCUMENT_REPOSITORY_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "document/block_note_snapshot.h"
#include "document/document.h"

namespace cppwiki::sync {
struct SyncBootstrap;
}

namespace cppwiki::storage {

struct DocumentRecord {
  document::PageMetadata metadata;
  document::BlockNoteDocumentSnapshot snapshot;
  std::string raw_snapshot_json;
};

struct DocumentSummary {
  std::string id;
  std::string title;
  std::string workspace_id;
  std::optional<std::string> parent_id;
  std::int32_t sort_order{};
  std::string created_at;
  std::string updated_at;
  std::string created_by;
};

[[nodiscard]] inline auto DocumentSummaryFromMetadata(
    const document::PageMetadata& metadata) -> DocumentSummary {
  return DocumentSummary{
      .id = metadata.id,
      .title = metadata.title,
      .workspace_id = metadata.workspace_id,
      .parent_id = metadata.parent_id,
      .sort_order = metadata.sort_order,
      .created_at = metadata.created_at,
      .updated_at = metadata.updated_at,
      .created_by = metadata.created_by,
  };
}

enum class RepositoryErrorCode {
  kOpenFailed,
  kReadFailed,
  kWriteFailed,
  kDeleteFailed,
  kInvalidRecord,
  kUnsupported,
};

struct RepositoryError {
  RepositoryErrorCode code;
  std::string message;
};

struct SaveDocumentResult {
  std::optional<RepositoryError> error;
};

struct DeleteDocumentResult {
  std::optional<RepositoryError> error;
};

struct LoadDocumentResult {
  std::optional<DocumentRecord> document;
  std::optional<RepositoryError> error;
};

struct ListDocumentsResult {
  std::vector<DocumentSummary> documents;
  std::optional<RepositoryError> error;
};

enum class SyncLifecycleState {
  kDisabled,
  kConfigured,
  kRunning,
  kError,
};

struct SyncStatus {
  SyncLifecycleState state{SyncLifecycleState::kDisabled};
  std::string status_text;
};

struct SyncOperationResult {
  std::optional<RepositoryError> error;
};

class LocalDocumentRepository {
 public:
  LocalDocumentRepository() = default;
  LocalDocumentRepository(const LocalDocumentRepository&) = delete;
  auto operator=(const LocalDocumentRepository&) -> LocalDocumentRepository& = delete;
  LocalDocumentRepository(LocalDocumentRepository&&) = delete;
  auto operator=(LocalDocumentRepository&&) -> LocalDocumentRepository& = delete;
  virtual ~LocalDocumentRepository() = default;

  [[nodiscard]] virtual auto SaveDocument(const DocumentRecord& document)
      -> SaveDocumentResult = 0;
  [[nodiscard]] virtual auto DeleteDocument(std::string_view page_id)
      -> DeleteDocumentResult = 0;
  [[nodiscard]] virtual auto LoadDocument(std::string_view page_id) -> LoadDocumentResult = 0;
  [[nodiscard]] virtual auto ListDocuments() -> ListDocumentsResult = 0;
  [[nodiscard]] virtual auto SupportsSync() const -> bool { return false; }
  [[nodiscard]] virtual auto SetSyncAccessToken(std::string)
      -> SyncOperationResult {
    return SyncOperationResult{
        .error = RepositoryError{
            .code = RepositoryErrorCode::kUnsupported,
            .message = "Repository does not support sync access tokens.",
        },
    };
  }
  [[nodiscard]] virtual auto ApplySyncBootstrap(const sync::SyncBootstrap&)
      -> SyncOperationResult {
    return SyncOperationResult{
        .error = RepositoryError{
            .code = RepositoryErrorCode::kUnsupported,
            .message = "Repository does not support sync bootstrap.",
        },
    };
  }
  [[nodiscard]] virtual auto StartSync() -> SyncOperationResult {
    return SyncOperationResult{
        .error = RepositoryError{
            .code = RepositoryErrorCode::kUnsupported,
            .message = "Repository does not support sync replication.",
        },
    };
  }
  [[nodiscard]] virtual auto StopSync() -> SyncOperationResult {
    return SyncOperationResult{};
  }
  [[nodiscard]] virtual auto GetSyncStatus() const -> SyncStatus {
    return SyncStatus{
        .state = SyncLifecycleState::kDisabled,
        .status_text = "Sync unsupported",
    };
  }
};

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_LOCAL_DOCUMENT_REPOSITORY_H_
