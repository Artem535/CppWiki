#include "storage/workspace_archive.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "storage/file_document_repository.h"

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto MakeDocument(std::string id, std::string workspace_id) -> cppwiki::storage::DocumentRecord {
  return cppwiki::storage::DocumentRecord{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = id,
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = "Archived Page " + id,
              .workspace_id = std::move(workspace_id),
              .sort_order = 1,
              .created_at = "2026-06-30T10:00:00.000Z",
              .updated_at = "2026-06-30T10:05:00.000Z",
              .created_by = "creator",
              .updated_by = "editor",
              .content_version = 2,
          },
      .snapshot =
          cppwiki::document::BlockNoteDocumentSnapshot{
              .id = id,
              .schema_version = 1,
              .title = "Archived Page " + id,
              .blocks = {},
          },
      .raw_snapshot_json =
          R"({"id":")" + id + R"(","schema_version":1,"title":"Archived","blocks":[]})",
  };
}

auto TestExportImportRoundTripsDocumentsAndConflicts() -> void {
  const auto source_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-source";
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-dest";
  const auto archive_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test.json";
  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);

  cppwiki::storage::FileDocumentRepository source_repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = source_dir});

  // FileDocumentRepository doesn't implement workspace root records at all (only the CBLite
  // backend does) -- SaveWorkspaceRoot/LoadWorkspaceRoot fall through to the base class's
  // kUnsupported default here. Export/import must still work fine without one; it's an optional
  // capability, exercised separately wherever a backend actually supports it.
  const auto root_save = source_repository.SaveWorkspaceRoot(cppwiki::storage::WorkspaceRootRecord{
      .workspace_id = "engineering",
      .title = "Engineering",
      .created_at = "2026-06-30T09:00:00.000Z",
      .schema_version = 1,
  });
  Require(root_save.error.has_value() &&
              root_save.error->code == cppwiki::storage::RepositoryErrorCode::kUnsupported,
          "FileDocumentRepository is expected not to support workspace root records");

  Require(!source_repository.SaveDocument(MakeDocument("page-1", "engineering")).error,
          "saving page-1 should succeed");
  Require(!source_repository.SaveDocument(MakeDocument("page-2", "engineering")).error,
          "saving page-2 should succeed");
  // A document in a different workspace must not leak into the "engineering" export.
  Require(!source_repository.SaveDocument(MakeDocument("other-page", "design")).error,
          "saving other-page should succeed");
  const auto MakeAttachment = [](std::string id, std::string workspace_id,
                                 std::vector<std::uint8_t> bytes) {
    return cppwiki::storage::AttachmentData{
        .metadata =
            cppwiki::storage::AttachmentMetadata{
                .id = std::move(id),
                .workspace_id = std::move(workspace_id),
                .filename = "architecture.png",
                .mime_type = "image/png",
                .size_bytes = bytes.size(),
                .sha256 = "0123456789abcdef",
                .created_at = "2026-08-31T10:00:00Z",
                .created_by = "tester",
            },
        .bytes = std::move(bytes),
    };
  };
  Require(
      !source_repository.SaveAttachment(MakeAttachment("attachment-1", "engineering", {1, 2, 3, 4}))
           .error,
      "engineering attachment should save");
  Require(!source_repository.SaveAttachment(MakeAttachment("attachment-other", "design", {5, 6, 7}))
               .error,
          "other workspace attachment should save");

  Require(!source_repository
               .SaveConflict(cppwiki::storage::DocumentConflictRecord{
                   .id = "conflict-1",
                   .document_id = "page-1",
                   .workspace_id = "engineering",
                   .base_version = 2,
                   .local_snapshot = R"({"title":"Local"})",
                   .remote_snapshot = R"({"title":"Remote"})",
                   .local_updated_by = "alice",
                   .remote_updated_by = "bob",
                   .detected_at = "2026-06-30T10:06:00.000Z",
                   .resolution_state = "pending",
               })
               .error,
          "saving a conflict should succeed");

  const auto export_result = cppwiki::storage::ExportWorkspaceToFile(
      source_repository, "engineering", archive_path.string());
  Require(!export_result.error, "exporting the workspace should succeed");
  Require(std::filesystem::exists(archive_path), "the archive file should have been written");

  cppwiki::storage::FileDocumentRepository dest_repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto import_result =
      cppwiki::storage::ImportWorkspaceFromFile(dest_repository, archive_path.string());
  Require(!import_result.error, "importing the workspace archive should succeed");
  Require(import_result.workspace_id.has_value() && *import_result.workspace_id == "engineering",
          "import should report the restored workspace id");

  const auto restored_page_1 = dest_repository.LoadDocument("page-1");
  Require(restored_page_1.document.has_value(), "page-1 should be restored");
  Require(restored_page_1.document->metadata.title == "Archived Page page-1",
          "page-1's title should round-trip");
  const auto restored_page_2 = dest_repository.LoadDocument("page-2");
  Require(restored_page_2.document.has_value(), "page-2 should be restored");

  const auto restored_other_page = dest_repository.LoadDocument("other-page");
  Require(!restored_other_page.document.has_value(),
          "a document from a different workspace must not be restored");

  const auto restored_conflicts = dest_repository.ListConflicts();
  Require(!restored_conflicts.error, "listing restored conflicts should succeed");
  Require(restored_conflicts.conflicts.size() == 1,
          "exactly the one engineering-workspace conflict should be restored");
  Require(restored_conflicts.conflicts.front().id == "conflict-1",
          "the restored conflict should preserve its id");
  const auto restored_attachment = dest_repository.LoadAttachment("attachment-1", "engineering");
  Require(!restored_attachment.error && restored_attachment.attachment.has_value(),
          "engineering attachment should be restored");
  Require(restored_attachment.attachment->bytes == std::vector<std::uint8_t>({1, 2, 3, 4}),
          "attachment bytes should round-trip");
  Require(!dest_repository.LoadAttachment("attachment-other", "design").attachment.has_value(),
          "other workspace attachment must not be restored");

  std::filesystem::remove_all(source_dir);
  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(archive_path);
}

auto TestImportRejectsInvalidArchiveFile() -> void {
  const auto dest_dir =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-test-invalid";
  const auto bogus_path =
      std::filesystem::temp_directory_path() / "cppwiki-workspace-archive-bogus.json";
  std::filesystem::remove_all(dest_dir);
  {
    std::ofstream bogus(bogus_path, std::ios::binary | std::ios::trunc);
    bogus << "not json at all";
  }

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = dest_dir});

  const auto result = cppwiki::storage::ImportWorkspaceFromFile(repository, bogus_path.string());
  Require(result.error.has_value(), "importing an invalid archive file should fail");

  std::filesystem::remove_all(dest_dir);
  std::filesystem::remove(bogus_path);
}

}  // namespace

auto main() -> int {
  TestExportImportRoundTripsDocumentsAndConflicts();
  TestImportRejectsInvalidArchiveFile();
  spdlog::info("cppwiki_workspace_archive_tests passed");
  return EXIT_SUCCESS;
}
