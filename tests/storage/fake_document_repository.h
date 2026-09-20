#ifndef CPPWIKI_TESTS_STORAGE_FAKE_DOCUMENT_REPOSITORY_H_
#define CPPWIKI_TESTS_STORAGE_FAKE_DOCUMENT_REPOSITORY_H_

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "knowledge/knowledge_record.h"
#include "storage/local_document_repository.h"

namespace cppwiki::storage::testing {

// A stateful in-memory repository double shared by bridge, editability-gate, and AI Chat tests.
class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord& document)
      -> cppwiki::storage::SaveDocumentResult override {
    documents_[document.metadata.id] = document;
    return {};
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id)
      -> cppwiki::storage::DeleteDocumentResult override {
    documents_.erase(std::string(page_id));
    return {};
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id)
      -> cppwiki::storage::LoadDocumentResult override {
    const auto it = documents_.find(std::string(page_id));
    if (it == documents_.end()) {
      return {.document = std::nullopt,
              .error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                  .message = "Document was not found.",
              }};
    }
    return {.document = it->second, .error = std::nullopt};
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    cppwiki::storage::ListDocumentsResult result;
    for (const auto& [id, document] : documents_) {
      result.documents.push_back(cppwiki::storage::DocumentSummaryFromMetadata(document.metadata));
    }
    return result;
  }

  [[nodiscard]] auto SaveAttachment(const cppwiki::storage::AttachmentData& attachment)
      -> cppwiki::storage::SaveAttachmentResult override {
    attachments_[attachment.metadata.id] = attachment;
    return {};
  }

  [[nodiscard]] auto LoadAttachment(std::string_view attachment_id, std::string_view workspace_id)
      -> cppwiki::storage::LoadAttachmentResult override {
    const auto it = attachments_.find(std::string(attachment_id));
    if (it == attachments_.end() || it->second.metadata.workspace_id != workspace_id) {
      return {.attachment = std::nullopt,
              .error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                  .message = "Attachment was not found.",
              }};
    }
    return {.attachment = it->second, .error = std::nullopt};
  }

  [[nodiscard]] auto ListAttachments(std::string_view workspace_id)
      -> cppwiki::storage::ListAttachmentsResult override {
    cppwiki::storage::ListAttachmentsResult result;
    for (const auto& [id, attachment] : attachments_) {
      if (attachment.metadata.workspace_id == workspace_id) {
        result.attachments.push_back(attachment.metadata);
      }
    }
    return result;
  }

  [[nodiscard]] auto SaveConflict(const cppwiki::storage::DocumentConflictRecord& conflict)
      -> cppwiki::storage::SaveConflictResult override {
    conflicts_[conflict.id] = conflict;
    return {};
  }

  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id)
      -> cppwiki::storage::DeleteConflictResult override {
    conflicts_.erase(std::string(conflict_id));
    return {};
  }

  [[nodiscard]] auto LoadConflict(std::string_view conflict_id)
      -> cppwiki::storage::LoadConflictResult override {
    const auto it = conflicts_.find(std::string(conflict_id));
    if (it == conflicts_.end()) {
      return {.conflict = std::nullopt, .error = std::nullopt};
    }
    return {.conflict = it->second, .error = std::nullopt};
  }

  [[nodiscard]] auto ListConflicts() -> cppwiki::storage::ListConflictsResult override {
    cppwiki::storage::ListConflictsResult result;
    for (const auto& [id, conflict] : conflicts_) {
      result.conflicts.push_back(conflict);
    }
    return result;
  }

  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    const auto it = conflicts_.find(std::string(conflict_id));
    if (it != conflicts_.end()) {
      it->second.resolution_state = "resolved";
    }
    return {};
  }

  [[nodiscard]] auto DismissConflict(std::string_view conflict_id)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    const auto it = conflicts_.find(std::string(conflict_id));
    if (it != conflicts_.end()) {
      it->second.resolution_state = "dismissed";
    }
    return {};
  }

  [[nodiscard]] auto SaveWorkspaceRoot(const cppwiki::storage::WorkspaceRootRecord& root)
      -> cppwiki::storage::SaveWorkspaceRootResult override {
    workspace_roots_[root.workspace_id] = root;
    return {};
  }

  [[nodiscard]] auto LoadWorkspaceRoot(std::string_view workspace_id)
      -> std::optional<cppwiki::storage::WorkspaceRootRecord> override {
    const auto it = workspace_roots_.find(std::string(workspace_id));
    if (it == workspace_roots_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  [[nodiscard]] auto SupportsSync() const -> bool override { return true; }

  [[nodiscard]] auto SaveDocumentRevision(const cppwiki::storage::DocumentRevisionRecord& revision)
      -> cppwiki::storage::SaveDocumentRevisionResult override {
    revisions_[revision.id] = revision;
    return {};
  }

  [[nodiscard]] auto ListDocumentRevisions(std::string_view document_id)
      -> cppwiki::storage::ListDocumentRevisionsResult override {
    cppwiki::storage::ListDocumentRevisionsResult result;
    for (const auto& [id, revision] : revisions_) {
      if (revision.document_id == document_id) {
        result.revisions.push_back(revision);
      }
    }
    std::ranges::sort(result.revisions,
                      [](const auto& lhs, const auto& rhs) { return lhs.saved_at > rhs.saved_at; });
    return result;
  }

  [[nodiscard]] auto DeleteDocumentRevision(std::string_view revision_id)
      -> cppwiki::storage::DeleteDocumentRevisionResult override {
    revisions_.erase(std::string(revision_id));
    return {};
  }

  [[nodiscard]] auto SavePropertyDefinition(
      const cppwiki::knowledge::PropertyDefinition& definition)
      -> cppwiki::storage::SaveKnowledgeRecordResult override {
    property_definitions_[definition.id] = definition;
    return {};
  }

  [[nodiscard]] auto ListPropertyDefinitions(std::string_view workspace_id)
      -> cppwiki::storage::ListPropertyDefinitionsResult override {
    cppwiki::storage::ListPropertyDefinitionsResult result;
    for (const auto& [id, definition] : property_definitions_) {
      if (definition.workspace_id == workspace_id) {
        result.definitions.push_back(definition);
      }
    }
    return result;
  }

  [[nodiscard]] auto SavePagePropertyValue(const cppwiki::knowledge::PagePropertyValue& value)
      -> cppwiki::storage::SaveKnowledgeRecordResult override {
    page_property_values_[value.id] = value;
    return {};
  }

  [[nodiscard]] auto ListPagePropertyValues(std::string_view workspace_id,
                                             std::string_view page_id)
      -> cppwiki::storage::ListPagePropertyValuesResult override {
    cppwiki::storage::ListPagePropertyValuesResult result;
    for (const auto& [id, value] : page_property_values_) {
      if (value.workspace_id == workspace_id && value.page_id == page_id) {
        result.values.push_back(value);
      }
    }
    return result;
  }

 private:
  std::map<std::string, cppwiki::storage::DocumentRecord> documents_;
  std::map<std::string, cppwiki::storage::AttachmentData> attachments_;
  std::map<std::string, cppwiki::storage::DocumentRevisionRecord> revisions_;
  std::map<std::string, cppwiki::storage::WorkspaceRootRecord> workspace_roots_;
  std::map<std::string, cppwiki::knowledge::PropertyDefinition> property_definitions_;
  std::map<std::string, cppwiki::knowledge::PagePropertyValue> page_property_values_;
  std::map<std::string, cppwiki::storage::DocumentConflictRecord> conflicts_;
};

}  // namespace cppwiki::storage::testing

#endif  // CPPWIKI_TESTS_STORAGE_FAKE_DOCUMENT_REPOSITORY_H_
