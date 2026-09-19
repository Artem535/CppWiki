#include "knowledge/context_pack_resolver.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "storage/file_document_repository.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto MakeAudit() -> cppwiki::knowledge::AuditMetadata {
  return {
      .created_at = "2026-09-19T10:00:00.000Z",
      .updated_at = "2026-09-19T10:00:00.000Z",
      .created_by = "tester",
      .updated_by = "tester",
  };
}

auto MakePageDocument(std::string id, std::string paragraph_text)
    -> cppwiki::storage::DocumentRecord {
  // FileDocumentRepository only round-trips raw_snapshot_json (LoadDocument leaves `.snapshot`
  // default-constructed) -- see ResolveContextPackItemContents, which parses raw_snapshot_json.
  return cppwiki::storage::DocumentRecord{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = id,
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = "Auth",
              .workspace_id = "engineering",
              .created_at = "2026-09-19T10:00:00.000Z",
              .updated_at = "2026-09-19T10:00:00.000Z",
              .created_by = "tester",
              .updated_by = "tester",
              .content_version = 1,
          },
      .snapshot = cppwiki::document::BlockNoteDocumentSnapshot{},
      .raw_snapshot_json = R"({"blocks": [{"id": "b1", "type": "paragraph",
          "content": [{"type": "text", "text": ")" + paragraph_text + R"("}]}]})",
  };
}

auto TestResolveContextPackItemContentsLoadsIncludedPagesOnly() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-context-pack-resolver-test";
  std::filesystem::remove_all(storage_directory);
  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});
  Require(!repository.SaveDocument(MakePageDocument("page-auth", "Session handling notes.")).error,
          "saving the included item's page must succeed");

  const cppwiki::knowledge::ContextPack pack{
      .id = "pack-a",
      .workspace_id = "engineering",
      .task_id = "page-task-a",
      .task_intent = "Fix the login bug",
      .items =
          {
              cppwiki::knowledge::ContextPackItem{
                  .id = "item-1",
                  .relation_id = "edge-1",
                  .artifact_kind = cppwiki::knowledge::ArtifactKind::kPage,
                  .artifact_id = "page-auth",
                  .included = true,
              },
              cppwiki::knowledge::ContextPackItem{
                  .id = "item-2",
                  .relation_id = "edge-2",
                  .artifact_kind = cppwiki::knowledge::ArtifactKind::kPage,
                  .artifact_id = "page-never-saved",
                  .included = false,
              },
          },
      .repository_guidance = std::nullopt,
      .state = cppwiki::knowledge::ContextPackState::kApproved,
      .audit = MakeAudit(),
  };

  const auto resolved = cppwiki::knowledge::ResolveContextPackItemContents(pack, repository);
  Require(std::holds_alternative<std::vector<cppwiki::knowledge::ContextPackItemContent>>(resolved),
          "resolving a pack whose included items exist must succeed");
  const auto& contents =
      std::get<std::vector<cppwiki::knowledge::ContextPackItemContent>>(resolved);
  Require(contents.size() == 1, "only the included item must be resolved");
  Require(contents.front().item_id == "item-1", "resolved content must be tagged by item id");
  Require(contents.front().text.find("Session handling notes.") != std::string::npos,
          "resolved content must contain the page's extracted text");

  std::filesystem::remove_all(storage_directory);
}

auto TestResolveContextPackItemContentsFailsForMissingPage() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-context-pack-resolver-missing-test";
  std::filesystem::remove_all(storage_directory);
  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  const cppwiki::knowledge::ContextPack pack{
      .id = "pack-a",
      .workspace_id = "engineering",
      .task_id = "page-task-a",
      .task_intent = "Fix the login bug",
      .items =
          {
              cppwiki::knowledge::ContextPackItem{
                  .id = "item-1",
                  .relation_id = "edge-1",
                  .artifact_kind = cppwiki::knowledge::ArtifactKind::kPage,
                  .artifact_id = "page-does-not-exist",
                  .included = true,
              },
          },
      .repository_guidance = std::nullopt,
      .state = cppwiki::knowledge::ContextPackState::kApproved,
      .audit = MakeAudit(),
  };

  const auto resolved = cppwiki::knowledge::ResolveContextPackItemContents(pack, repository);
  Require(std::holds_alternative<std::string>(resolved),
          "resolving a pack whose included item's page is missing must fail");

  std::filesystem::remove_all(storage_directory);
}

}  // namespace

auto main() -> int {
  TestResolveContextPackItemContentsLoadsIncludedPagesOnly();
  TestResolveContextPackItemContentsFailsForMissingPage();
  std::cout << "cppwiki_context_pack_resolver_tests passed\n";
  return EXIT_SUCCESS;
}
