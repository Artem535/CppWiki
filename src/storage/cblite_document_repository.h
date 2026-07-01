#ifndef CPPWIKI_SRC_STORAGE_CBLITE_DOCUMENT_REPOSITORY_H_
#define CPPWIKI_SRC_STORAGE_CBLITE_DOCUMENT_REPOSITORY_H_

#include <filesystem>
#include <memory>
#include <string>

#include "storage/local_document_repository.h"

namespace cppwiki::storage {

struct CbliteDocumentRepositoryOptions {
  std::filesystem::path database_directory;
  std::string database_name{"cppwiki"};
};

class CbliteDocumentRepository final : public LocalDocumentRepository {
 public:
  explicit CbliteDocumentRepository(CbliteDocumentRepositoryOptions options);
  ~CbliteDocumentRepository() override;

  CbliteDocumentRepository(const CbliteDocumentRepository&) = delete;
  auto operator=(const CbliteDocumentRepository&) -> CbliteDocumentRepository& = delete;
  CbliteDocumentRepository(CbliteDocumentRepository&&) = delete;
  auto operator=(CbliteDocumentRepository&&) -> CbliteDocumentRepository& = delete;

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult override;
  [[nodiscard]] auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult override;
  [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult override;
  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult override;
  [[nodiscard]] auto SaveConflict(const DocumentConflictRecord& conflict)
      -> SaveConflictResult override;
  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id)
      -> DeleteConflictResult override;
  [[nodiscard]] auto LoadConflict(std::string_view conflict_id) -> LoadConflictResult override;
  [[nodiscard]] auto ListConflicts() -> ListConflictsResult override;
  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult override;
  [[nodiscard]] auto DismissConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult override;
  [[nodiscard]] auto SupportsSync() const -> bool override;
  [[nodiscard]] auto SetSyncAccessToken(std::string access_token)
      -> SyncOperationResult override;
  [[nodiscard]] auto ApplySyncBootstrap(const sync::SyncBootstrap& bootstrap)
      -> SyncOperationResult override;
  [[nodiscard]] auto StartSync() -> SyncOperationResult override;
  [[nodiscard]] auto StopSync() -> SyncOperationResult override;
  [[nodiscard]] auto GetSyncStatus() const -> SyncStatus override;
  [[nodiscard]] auto SaveWorkspaceRoot(const WorkspaceRootRecord& workspace_root)
      -> SaveWorkspaceRootResult override;
  [[nodiscard]] auto LoadWorkspaceRoot(std::string_view workspace_id)
      -> std::optional<WorkspaceRootRecord> override;
  [[nodiscard]] auto ListWorkspaces() -> ListWorkspacesResult override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_CBLITE_DOCUMENT_REPOSITORY_H_
