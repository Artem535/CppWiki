#include "gui/merge/conflict_merge_resolution.h"

#include <spdlog/spdlog.h>

#include <QCoreApplication>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto ParseObject(std::string_view json) -> QJsonObject {
  const auto document = QJsonDocument::fromJson(
      QByteArray(json.data(), static_cast<qsizetype>(json.size())));
  Require(document.isObject(), "test JSON must be an object");
  return document.object();
}

auto ValidSnapshot(std::string_view title, std::string_view block_text) -> std::string {
  return std::string(R"({"id":"page-1","schema_version":1,"title":")") +
         std::string(title) +
         R"(","blocks":[{"id":"a","type":"paragraph","content":[{"type":"text","text":")" +
         std::string(block_text) + R"(","styles":{}}],"children":[]}]})";
}

class FakeDocumentRepository final : public cppwiki::storage::LocalDocumentRepository {
 public:
  [[nodiscard]] auto SaveDocument(const cppwiki::storage::DocumentRecord& document)
      -> cppwiki::storage::SaveDocumentResult override {
    save_called = true;
    saved_document = document;
    if (fail_save) {
      return {.error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kWriteFailed,
                  .message = "save failed",
              }};
    }
    documents[document.metadata.id] = document;
    return {};
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id)
      -> cppwiki::storage::DeleteDocumentResult override {
    documents.erase(std::string(page_id));
    return {};
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id)
      -> cppwiki::storage::LoadDocumentResult override {
    const auto it = documents.find(std::string(page_id));
    if (it == documents.end()) {
      return {.document = std::nullopt,
              .error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                  .message = "not found",
              }};
    }
    return {.document = it->second, .error = std::nullopt};
  }

  [[nodiscard]] auto ListDocuments() -> cppwiki::storage::ListDocumentsResult override {
    return {};
  }

  [[nodiscard]] auto SaveConflict(const cppwiki::storage::DocumentConflictRecord& conflict)
      -> cppwiki::storage::SaveConflictResult override {
    conflicts[conflict.id] = conflict;
    return {};
  }

  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id)
      -> cppwiki::storage::DeleteConflictResult override {
    conflicts.erase(std::string(conflict_id));
    return {};
  }

  [[nodiscard]] auto LoadConflict(std::string_view conflict_id)
      -> cppwiki::storage::LoadConflictResult override {
    const auto it = conflicts.find(std::string(conflict_id));
    if (it == conflicts.end()) {
      return {.conflict = std::nullopt,
              .error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kReadFailed,
                  .message = "not found",
              }};
    }
    return {.conflict = it->second, .error = std::nullopt};
  }

  [[nodiscard]] auto ListConflicts() -> cppwiki::storage::ListConflictsResult override {
    return {};
  }

  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    resolve_called = true;
    if (fail_resolve) {
      return {.error = cppwiki::storage::RepositoryError{
                  .code = cppwiki::storage::RepositoryErrorCode::kWriteFailed,
                  .message = "resolve failed",
              }};
    }
    auto it = conflicts.find(std::string(conflict_id));
    if (it != conflicts.end()) {
      it->second.resolution_state = "resolved";
    }
    return {};
  }

  [[nodiscard]] auto DismissConflict(std::string_view)
      -> cppwiki::storage::UpdateConflictResolutionResult override {
    return {};
  }

  std::map<std::string, cppwiki::storage::DocumentRecord> documents;
  std::map<std::string, cppwiki::storage::DocumentConflictRecord> conflicts;
  std::optional<cppwiki::storage::DocumentRecord> saved_document;
  bool save_called{false};
  bool resolve_called{false};
  bool fail_save{false};
  bool fail_resolve{false};
};

auto MakeCurrentDocument() -> cppwiki::storage::DocumentRecord {
  return cppwiki::storage::DocumentRecord{
      .metadata = cppwiki::document::PageMetadata{
          .id = "page-1",
          .schema_version = cppwiki::document::SchemaVersion::kV1,
          .title = "Existing",
          .workspace_id = "default",
          .parent_id = std::nullopt,
          .sort_order = 0,
          .created_at = "2026-07-01T10:00:00.000Z",
          .updated_at = "2026-07-01T10:05:00.000Z",
          .created_by = "original-author",
          .updated_by = "previous-editor",
          .content_version = 8,
      },
      .snapshot = cppwiki::document::BlockNoteDocumentSnapshot{
          .id = std::string("page-1"),
          .schema_version = 1,
          .title = std::string("Existing"),
          .blocks = std::vector<cppwiki::document::BlockNoteBlockSnapshot>{},
      },
      .raw_snapshot_json = ValidSnapshot("Existing", "Before"),
  };
}

auto MakeConflict(std::int64_t base_version = 7) -> cppwiki::storage::DocumentConflictRecord {
  return cppwiki::storage::DocumentConflictRecord{
      .id = "conflict-1",
      .document_id = "page-1",
      .workspace_id = "default",
      .base_version = base_version,
      .local_snapshot = ValidSnapshot("Local", "Local text"),
      .remote_snapshot = ValidSnapshot("Remote", "Remote text"),
      .local_updated_by = "alice",
      .remote_updated_by = "bob",
      .detected_at = "2026-07-02T10:00:00.000Z",
      .resolution_state = "pending",
  };
}

auto TestApplyMergedConflictResolutionSavesDocumentAndResolvesConflict() -> void {
  FakeDocumentRepository repository;
  repository.documents["page-1"] = MakeCurrentDocument();
  repository.conflicts["conflict-1"] = MakeConflict();

  const auto result = cppwiki::gui::merge::ApplyMergedConflictResolution(
      repository, QStringLiteral("conflict-1"), QStringLiteral("merger"),
      ParseObject(ValidSnapshot("Merged", "Merged text")));

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kApplied,
          "merge resolution should be applied");
  Require(repository.save_called, "document save should be called");
  Require(repository.resolve_called, "conflict resolve should be called");
  Require(repository.saved_document.has_value(), "saved document should be captured");
  const auto& saved = *repository.saved_document;
  Require(saved.metadata.id == "page-1", "saved document id should match conflict document");
  Require(saved.metadata.workspace_id == "default", "saved workspace should come from conflict");
  Require(saved.metadata.title == "Merged", "saved title should come from merged snapshot");
  Require(saved.metadata.created_at == "2026-07-01T10:00:00.000Z",
          "created_at should be preserved");
  Require(saved.metadata.created_by == "original-author", "created_by should be preserved");
  Require(saved.metadata.updated_by == "merger", "updated_by should use effective author");
  Require(saved.metadata.content_version == 9, "content_version should increment from current");
  Require(repository.conflicts["conflict-1"].resolution_state == "resolved",
          "conflict should be marked resolved");
  Require(saved.raw_snapshot_json.find("Merged text") != std::string::npos,
          "raw snapshot should contain merged content");
}

auto TestApplyMergedConflictResolutionUsesBaseVersionForMissingCurrentDocument() -> void {
  FakeDocumentRepository repository;
  repository.conflicts["conflict-1"] = MakeConflict(11);

  const auto result = cppwiki::gui::merge::ApplyMergedConflictResolution(
      repository, QStringLiteral("conflict-1"), QStringLiteral("merger"),
      ParseObject(ValidSnapshot("Merged", "Merged text")));

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kApplied,
          "merge resolution should apply without current document");
  Require(repository.saved_document.has_value(), "saved document should exist");
  Require(repository.saved_document->metadata.created_by == "merger",
          "created_by should use author for new records");
  Require(repository.saved_document->metadata.content_version == 12,
          "content_version should increment from conflict base version");
}

auto TestApplyMergedConflictResolutionDoesNotResolveWhenSaveFails() -> void {
  FakeDocumentRepository repository;
  repository.conflicts["conflict-1"] = MakeConflict();
  repository.fail_save = true;

  const auto result = cppwiki::gui::merge::ApplyMergedConflictResolution(
      repository, QStringLiteral("conflict-1"), QStringLiteral("merger"),
      ParseObject(ValidSnapshot("Merged", "Merged text")));

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kSaveFailed,
          "save failure should be reported");
  Require(repository.save_called, "save should be attempted");
  Require(!repository.resolve_called, "resolve should not be attempted after failed save");
}

auto TestApplyMergedConflictResolutionReportsPartialSuccessWhenResolveFails() -> void {
  FakeDocumentRepository repository;
  repository.conflicts["conflict-1"] = MakeConflict();
  repository.fail_resolve = true;

  const auto result = cppwiki::gui::merge::ApplyMergedConflictResolution(
      repository, QStringLiteral("conflict-1"), QStringLiteral("merger"),
      ParseObject(ValidSnapshot("Merged", "Merged text")));

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kResolveFailed,
          "resolve failure should be reported as partial success");
  Require(repository.save_called, "document should still be saved");
  Require(repository.resolve_called, "resolve should be attempted");
  Require(repository.saved_document.has_value(), "saved document should be captured");
  Require(repository.conflicts["conflict-1"].resolution_state == "pending",
          "failed resolve should leave conflict pending");
}

auto TestApplyConflictSideResolutionUsesLocalSnapshotAndAuthor() -> void {
  FakeDocumentRepository repository;
  repository.documents["page-1"] = MakeCurrentDocument();
  repository.conflicts["conflict-1"] = MakeConflict();

  const auto result = cppwiki::gui::merge::ApplyConflictSideResolution(
      repository, QStringLiteral("conflict-1"),
      cppwiki::gui::merge::ConflictResolutionSide::kLocal);

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kApplied,
          "local side resolution should be applied");
  Require(repository.saved_document.has_value(), "saved local side document should exist");
  Require(repository.saved_document->metadata.title == "Local",
          "local side resolution should use local title");
  Require(repository.saved_document->metadata.updated_by == "alice",
          "local side resolution should use local author");
  Require(repository.saved_document->raw_snapshot_json.find("Local text") != std::string::npos,
          "local side resolution should save local snapshot content");
}

auto TestApplyConflictSideResolutionUsesRemoteSnapshotAndAuthor() -> void {
  FakeDocumentRepository repository;
  repository.documents["page-1"] = MakeCurrentDocument();
  repository.conflicts["conflict-1"] = MakeConflict();

  const auto result = cppwiki::gui::merge::ApplyConflictSideResolution(
      repository, QStringLiteral("conflict-1"),
      cppwiki::gui::merge::ConflictResolutionSide::kRemote);

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kApplied,
          "remote side resolution should be applied");
  Require(repository.saved_document.has_value(), "saved remote side document should exist");
  Require(repository.saved_document->metadata.title == "Remote",
          "remote side resolution should use remote title");
  Require(repository.saved_document->metadata.updated_by == "bob",
          "remote side resolution should use remote author");
  Require(repository.saved_document->raw_snapshot_json.find("Remote text") != std::string::npos,
          "remote side resolution should save remote snapshot content");
}

auto TestApplyConflictSideResolutionDoesNotResolveInvalidSnapshot() -> void {
  FakeDocumentRepository repository;
  auto conflict = MakeConflict();
  conflict.local_snapshot = R"({"id":"page-1","schema_version":1,"title":"Broken","blocks":[{"type":"paragraph"}]})";
  repository.conflicts["conflict-1"] = conflict;

  const auto result = cppwiki::gui::merge::ApplyConflictSideResolution(
      repository, QStringLiteral("conflict-1"),
      cppwiki::gui::merge::ConflictResolutionSide::kLocal);

  Require(result.status == cppwiki::gui::merge::MergeResolutionStatus::kInvalidMergedSnapshot,
          "invalid local snapshot should be rejected");
  Require(!repository.save_called, "invalid side snapshot should not be saved");
  Require(!repository.resolve_called, "invalid side snapshot should not resolve conflict");
}

}  // namespace

auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  TestApplyMergedConflictResolutionSavesDocumentAndResolvesConflict();
  TestApplyMergedConflictResolutionUsesBaseVersionForMissingCurrentDocument();
  TestApplyMergedConflictResolutionDoesNotResolveWhenSaveFails();
  TestApplyMergedConflictResolutionReportsPartialSuccessWhenResolveFails();
  TestApplyConflictSideResolutionUsesLocalSnapshotAndAuthor();
  TestApplyConflictSideResolutionUsesRemoteSnapshotAndAuthor();
  TestApplyConflictSideResolutionDoesNotResolveInvalidSnapshot();
  return EXIT_SUCCESS;
}
