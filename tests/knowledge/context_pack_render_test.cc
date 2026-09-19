#include "knowledge/context_pack_render.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <variant>

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

auto MakePageDocument() -> cppwiki::storage::DocumentRecord {
  return cppwiki::storage::DocumentRecord{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = "page-auth",
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
          "content": [{"type": "text", "text": "Session handling notes."}]}]})",
  };
}

auto MakePack(cppwiki::knowledge::ContextPackState state) -> cppwiki::knowledge::ContextPack {
  return {
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
          },
      .repository_guidance = "Follow the existing auth module conventions.",
      .state = state,
      .audit = MakeAudit(),
  };
}

auto TestRenderApprovedContextPackProducesRefAndText() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-context-pack-render-test";
  std::filesystem::remove_all(storage_directory);
  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});
  Require(!repository.SaveDocument(MakePageDocument()).error,
          "saving the included item's page must succeed");

  const auto pack = MakePack(cppwiki::knowledge::ContextPackState::kApproved);
  const auto rendered = cppwiki::knowledge::RenderApprovedContextPack(pack, repository);
  Require(std::holds_alternative<cppwiki::knowledge::ContextPackRenderResult>(rendered),
          "rendering a well-formed approved pack must succeed");
  const auto& result = std::get<cppwiki::knowledge::ContextPackRenderResult>(rendered);
  Require(result.context_pack_ref == "pack-a@1.0",
          "the context pack ref must be \"<pack id>@1.0\"");
  Require(result.text.find("Fix the login bug") != std::string::npos,
          "rendered text must contain the task intent");
  Require(result.text.find("Session handling notes.") != std::string::npos,
          "rendered text must contain the included item's resolved content");
  Require(result.text.find("Follow the existing auth module conventions.") != std::string::npos,
          "rendered text must contain the repository guidance");

  std::filesystem::remove_all(storage_directory);
}

auto TestRenderApprovedContextPackRejectsADraftPack() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-context-pack-render-draft-test";
  std::filesystem::remove_all(storage_directory);
  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});
  Require(!repository.SaveDocument(MakePageDocument()).error,
          "saving the included item's page must succeed");

  const auto pack = MakePack(cppwiki::knowledge::ContextPackState::kDraft);
  const auto rendered = cppwiki::knowledge::RenderApprovedContextPack(pack, repository);
  Require(std::holds_alternative<std::string>(rendered),
          "rendering a draft pack must fail -- only approved packs may be frozen");

  std::filesystem::remove_all(storage_directory);
}

}  // namespace

auto main() -> int {
  TestRenderApprovedContextPackProducesRefAndText();
  TestRenderApprovedContextPackRejectsADraftPack();
  std::cout << "cppwiki_context_pack_render_tests passed\n";
  return EXIT_SUCCESS;
}
