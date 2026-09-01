#ifndef CPPWIKI_SRC_STORAGE_LOCAL_DOCUMENT_REPOSITORY_H_
#define CPPWIKI_SRC_STORAGE_LOCAL_DOCUMENT_REPOSITORY_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "document/block_note_snapshot.h"
#include "document/document.h"
#include "storage/attachment.h"

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
  document::DocumentKind kind{document::DocumentKind::kWikiPage};
  std::string title;
  std::string workspace_id;
  std::optional<std::string> parent_id;
  std::int32_t sort_order{};
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
  std::int64_t content_version{1};
  // Issue #167: set when the repository found this document's storage entry but couldn't load
  // it (e.g. corrupted/unparseable JSON on disk) -- ListDocuments() still reports a placeholder
  // summary for it (see FileDocumentRepository::ListDocuments()) instead of silently omitting it,
  // so the workspace tree can show a visible error placeholder rather than the page just
  // vanishing with no trace. load_error holds RepositoryError::message when set.
  bool is_corrupted{false};
  std::optional<std::string> load_error;
  // Issue #165: mirrors PageMetadata::trashed_at -- nullopt means live, set means in the trash.
  std::optional<std::string> trashed_at;
};

[[nodiscard]] inline auto DocumentSummaryFromMetadata(const document::PageMetadata& metadata)
    -> DocumentSummary {
  return DocumentSummary{
      .id = metadata.id,
      .kind = metadata.kind,
      .title = metadata.title,
      .workspace_id = metadata.workspace_id,
      .parent_id = metadata.parent_id,
      .sort_order = metadata.sort_order,
      .created_at = metadata.created_at,
      .updated_at = metadata.updated_at,
      .created_by = metadata.created_by,
      .updated_by = metadata.updated_by,
      .content_version = metadata.content_version,
      .trashed_at = metadata.trashed_at,
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

struct SaveAttachmentResult {
  std::optional<RepositoryError> error;
};

struct LoadAttachmentResult {
  std::optional<AttachmentData> attachment;
  std::optional<RepositoryError> error;
};

struct ListAttachmentsResult {
  std::vector<AttachmentMetadata> attachments;
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

struct DocumentConflictRecord {
  std::string id;
  std::string document_id;
  std::string workspace_id;
  std::int64_t base_version{};
  std::string local_snapshot;
  std::string remote_snapshot;
  std::string local_updated_by;
  std::string remote_updated_by;
  std::string detected_at;
  std::string resolution_state{"pending"};
};

struct SaveConflictResult {
  std::optional<RepositoryError> error;
};

struct LoadConflictResult {
  std::optional<DocumentConflictRecord> conflict;
  std::optional<RepositoryError> error;
};

struct ListConflictsResult {
  std::vector<DocumentConflictRecord> conflicts;
  std::optional<RepositoryError> error;
};

struct DeleteConflictResult {
  std::optional<RepositoryError> error;
};

struct UpdateConflictResolutionResult {
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
  bool initial_pull_active{false};
  bool initial_pull_completed{false};
  bool has_conflicts{false};
  std::size_t conflict_count{0};
};

struct SyncOperationResult {
  std::optional<RepositoryError> error;
};

struct WorkspaceRootRecord {
  std::string workspace_id;
  std::string title;
  std::string created_at;
  std::int64_t schema_version{1};
};

struct SaveWorkspaceRootResult {
  std::optional<RepositoryError> error;
};

struct ListWorkspacesResult {
  std::vector<std::string> workspace_ids;
  std::optional<RepositoryError> error;
};

// Issue #166: a single prior snapshot of a document's content, kept around so an unwanted edit
// can be undone without a full version-control system. Deliberately minimal -- no diffs, no
// naming/tagging, no sync awareness (see the issue's explicit out-of-scope list).
struct DocumentRevisionRecord {
  std::string id;
  std::string document_id;
  std::string workspace_id;
  std::string raw_snapshot_json;
  std::string title;
  std::string saved_at;
};

struct SaveDocumentRevisionResult {
  std::optional<RepositoryError> error;
};

struct ListDocumentRevisionsResult {
  // Newest first.
  std::vector<DocumentRevisionRecord> revisions;
  std::optional<RepositoryError> error;
};

struct DeleteDocumentRevisionResult {
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

  [[nodiscard]] virtual auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult = 0;
  [[nodiscard]] virtual auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult = 0;
  [[nodiscard]] virtual auto LoadDocument(std::string_view page_id) -> LoadDocumentResult = 0;
  [[nodiscard]] virtual auto ListDocuments() -> ListDocumentsResult = 0;
  [[nodiscard]] virtual auto SaveAttachment(const AttachmentData&) -> SaveAttachmentResult {
    return SaveAttachmentResult{
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support attachments.",
            },
    };
  }
  [[nodiscard]] virtual auto LoadAttachment(std::string_view, std::string_view)
      -> LoadAttachmentResult {
    return LoadAttachmentResult{
        .attachment = std::nullopt,
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support attachments.",
            },
    };
  }
  [[nodiscard]] virtual auto ListAttachments(std::string_view) -> ListAttachmentsResult {
    return ListAttachmentsResult{
        .attachments = {},
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support attachments.",
            },
    };
  }
  [[nodiscard]] virtual auto SaveConflict(const DocumentConflictRecord& conflict)
      -> SaveConflictResult = 0;
  [[nodiscard]] virtual auto DeleteConflict(std::string_view conflict_id)
      -> DeleteConflictResult = 0;
  [[nodiscard]] virtual auto LoadConflict(std::string_view conflict_id) -> LoadConflictResult = 0;
  [[nodiscard]] virtual auto ListConflicts() -> ListConflictsResult = 0;
  [[nodiscard]] virtual auto ResolveConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult = 0;
  [[nodiscard]] virtual auto DismissConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult = 0;
  [[nodiscard]] virtual auto SupportsSync() const -> bool {
    return false;
  }
  [[nodiscard]] virtual auto SetSyncAccessToken(std::string) -> SyncOperationResult {
    return SyncOperationResult{
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support sync access tokens.",
            },
    };
  }
  [[nodiscard]] virtual auto ApplySyncBootstrap(const sync::SyncBootstrap&) -> SyncOperationResult {
    return SyncOperationResult{
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support sync bootstrap.",
            },
    };
  }
  [[nodiscard]] virtual auto StartSync() -> SyncOperationResult {
    return SyncOperationResult{
        .error =
            RepositoryError{
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

  // Workspace root/meta document support. A workspace is only considered
  // "materialized" once its root record exists locally - this is the fact
  // that proves an initial pull actually completed for that workspace, even
  // if the workspace itself contains zero pages.
  [[nodiscard]] virtual auto SaveWorkspaceRoot(const WorkspaceRootRecord&)
      -> SaveWorkspaceRootResult {
    return SaveWorkspaceRootResult{
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support workspace root records.",
            },
    };
  }
  [[nodiscard]] virtual auto LoadWorkspaceRoot(std::string_view)
      -> std::optional<WorkspaceRootRecord> {
    return std::nullopt;
  }
  [[nodiscard]] virtual auto ListWorkspaces() -> ListWorkspacesResult {
    return ListWorkspacesResult{
        .workspace_ids = {},
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support listing workspaces.",
            },
    };
  }

  // Issue #166: document revision history. Default kUnsupported (mirrors the workspace-root
  // methods above) for backends that don't implement it yet.
  [[nodiscard]] virtual auto SaveDocumentRevision(const DocumentRevisionRecord&)
      -> SaveDocumentRevisionResult {
    return SaveDocumentRevisionResult{
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support document revisions.",
            },
    };
  }
  [[nodiscard]] virtual auto ListDocumentRevisions(std::string_view)
      -> ListDocumentRevisionsResult {
    return ListDocumentRevisionsResult{
        .revisions = {},
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support document revisions.",
            },
    };
  }
  [[nodiscard]] virtual auto DeleteDocumentRevision(std::string_view)
      -> DeleteDocumentRevisionResult {
    return DeleteDocumentRevisionResult{
        .error =
            RepositoryError{
                .code = RepositoryErrorCode::kUnsupported,
                .message = "Repository does not support document revisions.",
            },
    };
  }
};

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_LOCAL_DOCUMENT_REPOSITORY_H_
