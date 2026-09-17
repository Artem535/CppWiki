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
  [[nodiscard]] auto SavePropertyDefinition(const knowledge::PropertyDefinition& definition)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeletePropertyDefinition(std::string_view definition_id)
      -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListPropertyDefinitions(std::string_view workspace_id)
      -> ListPropertyDefinitionsResult override;
  [[nodiscard]] auto SavePagePropertyValue(const knowledge::PagePropertyValue& value)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeletePagePropertyValue(std::string_view value_id)
      -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListPagePropertyValues(std::string_view workspace_id, std::string_view page_id)
      -> ListPagePropertyValuesResult override;
  [[nodiscard]] auto SaveRelationType(const knowledge::RelationType& relation_type)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeleteRelationType(std::string_view relation_type_id)
      -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListRelationTypes(std::string_view workspace_id)
      -> ListRelationTypesResult override;
  [[nodiscard]] auto SavePageRelation(const knowledge::PageRelation& relation)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeletePageRelation(std::string_view relation_id)
      -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListPageRelations(std::string_view workspace_id, std::string_view page_id)
      -> ListPageRelationsResult override;
  [[nodiscard]] auto DeleteKnowledgeForPage(std::string_view workspace_id, std::string_view page_id)
      -> DeleteKnowledgeForPageResult override;
  [[nodiscard]] auto SaveRepositoryArtifact(const knowledge::RepositoryArtifact& artifact)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeleteRepositoryArtifact(std::string_view artifact_id)
      -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListRepositoryArtifacts(std::string_view workspace_id)
      -> ListRepositoryArtifactsResult override;
  [[nodiscard]] auto SaveAgentRun(const knowledge::AgentRun& run)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeleteAgentRun(std::string_view run_id) -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListAgentRuns(std::string_view workspace_id) -> ListAgentRunsResult override;
  [[nodiscard]] auto SaveResultReference(const knowledge::ResultReference& reference)
      -> SaveKnowledgeRecordResult override;
  [[nodiscard]] auto DeleteResultReference(std::string_view reference_id)
      -> DeleteKnowledgeRecordResult override;
  [[nodiscard]] auto ListResultReferences(std::string_view workspace_id,
                                          std::string_view agent_run_id)
      -> ListResultReferencesResult override;
  [[nodiscard]] auto SaveAttachment(const AttachmentData& attachment)
      -> SaveAttachmentResult override;
  [[nodiscard]] auto LoadAttachment(std::string_view attachment_id, std::string_view workspace_id)
      -> LoadAttachmentResult override;
  [[nodiscard]] auto ListAttachments(std::string_view workspace_id)
      -> ListAttachmentsResult override;
  [[nodiscard]] auto SaveConflict(const DocumentConflictRecord& conflict)
      -> SaveConflictResult override;
  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id) -> DeleteConflictResult override;
  [[nodiscard]] auto LoadConflict(std::string_view conflict_id) -> LoadConflictResult override;
  [[nodiscard]] auto ListConflicts() -> ListConflictsResult override;
  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult override;
  [[nodiscard]] auto DismissConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult override;
  [[nodiscard]] auto SupportsSync() const -> bool override;
  [[nodiscard]] auto SetSyncAccessToken(std::string access_token) -> SyncOperationResult override;
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
