#include "storage/local_document_repository.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <utility>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord& document)
      -> cppwiki::storage::SaveDocumentResult override {
    document_ = document;
    return cppwiki::storage::SaveDocumentResult{.error = std::nullopt};
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id)
      -> cppwiki::storage::DeleteDocumentResult override {
    if (document_ && document_->metadata.id == page_id) {
      document_.reset();
    }
    return cppwiki::storage::DeleteDocumentResult{.error = std::nullopt};
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id)
      -> cppwiki::storage::LoadDocumentResult override {
    if (!document_ || document_->metadata.id != page_id) {
      return cppwiki::storage::LoadDocumentResult{
          .document = std::nullopt,
          .error = cppwiki::storage::RepositoryError{
              .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
              .message = "Document was not found.",
          },
      };
    }

    return cppwiki::storage::LoadDocumentResult{
        .document = document_,
        .error = std::nullopt,
    };
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    cppwiki::storage::ListDocumentsResult result;
    if (document_) {
      result.documents.push_back(
          cppwiki::storage::DocumentSummaryFromMetadata(document_->metadata));
    }
    return result;
  }

  [[nodiscard]] auto SaveConflict(const cppwiki::storage::DocumentConflictRecord& conflict)
      -> cppwiki::storage::SaveConflictResult override {
    conflicts_.push_back(conflict);
    return cppwiki::storage::SaveConflictResult{.error = std::nullopt};
  }

  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id)
      -> cppwiki::storage::DeleteConflictResult override {
    std::erase_if(conflicts_, [conflict_id](const auto& conflict) {
      return conflict.id == conflict_id;
    });
    return cppwiki::storage::DeleteConflictResult{.error = std::nullopt};
  }

  [[nodiscard]] auto LoadConflict(std::string_view conflict_id)
      -> cppwiki::storage::LoadConflictResult override {
    for (const auto& conflict : conflicts_) {
      if (conflict.id == conflict_id) {
        return cppwiki::storage::LoadConflictResult{
            .conflict = conflict,
            .error = std::nullopt,
        };
      }
    }

    return cppwiki::storage::LoadConflictResult{
        .conflict = std::nullopt,
        .error = cppwiki::storage::RepositoryError{
            .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
            .message = "Conflict was not found.",
        },
    };
  }

  [[nodiscard]] auto ListConflicts() -> cppwiki::storage::ListConflictsResult override {
    return cppwiki::storage::ListConflictsResult{
        .conflicts = conflicts_,
        .error = std::nullopt,
    };
  }

  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    for (auto& conflict : conflicts_) {
      if (conflict.id == conflict_id) {
        conflict.resolution_state = "resolved";
        return {};
      }
    }
    return {.error = cppwiki::storage::RepositoryError{
                .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                .message = "Conflict was not found.",
            }};
  }

  [[nodiscard]] auto DismissConflict(std::string_view conflict_id)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    for (auto& conflict : conflicts_) {
      if (conflict.id == conflict_id) {
        conflict.resolution_state = "dismissed";
        return {};
      }
    }
    return {.error = cppwiki::storage::RepositoryError{
                .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                .message = "Conflict was not found.",
            }};
  }

 private:
  std::optional<cppwiki::storage::DocumentRecord> document_;
  std::vector<cppwiki::storage::DocumentConflictRecord> conflicts_;
};

auto TestRepositoryInterfaceStoresValidatedRawSnapshot() -> void {
  FakeDocumentRepository repository;

  cppwiki::storage::DocumentRecord document{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = "page-1",
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = "Getting Started",
              .workspace_id = "default",
              .parent_id = std::nullopt,
              .sort_order = 10,
              .created_at = "2026-06-16T08:00:00.000Z",
              .updated_at = "2026-06-16T08:01:00.000Z",
              .created_by = "tester",
              .updated_by = "editor",
              .content_version = 7,
          },
      .snapshot =
          cppwiki::document::BlockNoteDocumentSnapshot{
              .id = std::string("page-1"),
              .schema_version = 1,
              .title = std::string("Getting Started"),
              .blocks = std::vector<cppwiki::document::BlockNoteBlockSnapshot>{},
          },
      .raw_snapshot_json = R"({"id":"page-1","schema_version":1,"title":"Getting Started","blocks":[]})",
  };

  const auto save_result = repository.SaveDocument(document);
  Require(!save_result.error, "save through repository interface should succeed");

  const auto load_result = repository.LoadDocument("page-1");
  Require(load_result.document.has_value(), "load through repository interface should return document");
  Require(!load_result.error, "load through repository interface should not return error");
  Require(load_result.document->metadata.id == "page-1", "loaded metadata id should match");
  Require(load_result.document->metadata.sort_order == 10,
          "loaded metadata sort order should match");
  Require(load_result.document->metadata.workspace_id == "default",
          "loaded workspace id should match");
  Require(load_result.document->metadata.created_by == "tester",
          "loaded created_by should match");
  Require(load_result.document->metadata.updated_by == "editor",
          "loaded updated_by should match");
  Require(load_result.document->metadata.content_version == 7,
          "loaded content_version should match");
  Require(load_result.document->raw_snapshot_json == document.raw_snapshot_json,
          "loaded raw snapshot should match");

  const auto list_result = repository.ListDocuments();
  Require(!list_result.error, "list through repository interface should not return error");
  Require(list_result.documents.size() == 1, "repository list should include saved document");
  Require(list_result.documents.front().id == "page-1", "listed metadata id should match");
  Require(list_result.documents.front().sort_order == 10,
          "listed metadata sort order should match");
  Require(list_result.documents.front().workspace_id == "default",
          "listed workspace id should match");
  Require(list_result.documents.front().created_by == "tester",
          "listed created_by should match");
  Require(list_result.documents.front().updated_by == "editor",
          "listed updated_by should match");
  Require(list_result.documents.front().content_version == 7,
          "listed content_version should match");

  const auto delete_result = repository.DeleteDocument("page-1");
  Require(!delete_result.error, "delete through repository interface should succeed");
  Require(!repository.LoadDocument("page-1").document.has_value(),
          "deleted document should no longer be loadable");
}

auto TestRepositoryInterfaceStoresConflictRecords() -> void {
  FakeDocumentRepository repository;

  cppwiki::storage::DocumentConflictRecord conflict{
      .id = "conflict-1",
      .document_id = "page-1",
      .workspace_id = "default",
      .base_version = 7,
      .local_snapshot = R"({"title":"Local"})",
      .remote_snapshot = R"({"title":"Remote"})",
      .local_updated_by = "alice",
      .remote_updated_by = "bob",
      .detected_at = "2026-06-30T10:00:00.000Z",
      .resolution_state = "pending",
  };

  const auto save_result = repository.SaveConflict(conflict);
  Require(!save_result.error, "save conflict through repository interface should succeed");

  const auto load_result = repository.LoadConflict("conflict-1");
  Require(load_result.conflict.has_value(), "load conflict should return record");
  Require(load_result.conflict->document_id == "page-1", "loaded conflict document id should match");
  Require(load_result.conflict->remote_updated_by == "bob",
          "loaded conflict remote_updated_by should match");

  const auto list_result = repository.ListConflicts();
  Require(!list_result.error, "list conflicts through repository interface should succeed");
  Require(list_result.conflicts.size() == 1, "repository should list saved conflict");
  Require(list_result.conflicts.front().resolution_state == "pending",
          "listed conflict resolution_state should match");

  const auto delete_result = repository.DeleteConflict("conflict-1");
  Require(!delete_result.error, "delete conflict through repository interface should succeed");
  Require(!repository.LoadConflict("conflict-1").conflict.has_value(),
          "deleted conflict should no longer be loadable");
}

auto TestRepositoryInterfaceUpdatesConflictResolutionState() -> void {
  FakeDocumentRepository repository;
  cppwiki::storage::DocumentConflictRecord conflict{
      .id = "conflict-2",
      .document_id = "page-2",
      .workspace_id = "default",
      .base_version = 3,
      .local_snapshot = "{}",
      .remote_snapshot = "{}",
      .local_updated_by = "alice",
      .remote_updated_by = "bob",
      .detected_at = "2026-06-30T12:00:00.000Z",
      .resolution_state = "pending",
  };
  Require(!repository.SaveConflict(conflict).error, "seed conflict should save");
  Require(!repository.ResolveConflict("conflict-2").error, "resolve conflict should succeed");
  Require(repository.LoadConflict("conflict-2").conflict->resolution_state == "resolved",
          "resolved conflict should keep resolved state");
  Require(!repository.DismissConflict("conflict-2").error, "dismiss conflict should succeed");
  Require(repository.LoadConflict("conflict-2").conflict->resolution_state == "dismissed",
          "dismissed conflict should keep dismissed state");
}

}  // namespace

auto main() -> int {
  TestRepositoryInterfaceStoresValidatedRawSnapshot();
  TestRepositoryInterfaceStoresConflictRecords();
  TestRepositoryInterfaceUpdatesConflictResolutionState();

  spdlog::info("cppwiki_local_document_repository_tests passed");
  return EXIT_SUCCESS;
}
