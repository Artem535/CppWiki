#include "storage/file_document_repository.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto MakeDocument() -> cppwiki::storage::DocumentRecord {
  return cppwiki::storage::DocumentRecord{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = "page-1",
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = "File Repo Page",
              .workspace_id = "engineering",
              .parent_id = std::make_optional<std::string>("workspace-root"),
              .sort_order = 3,
              .created_at = "2026-06-30T10:00:00.000Z",
              .updated_at = "2026-06-30T10:05:00.000Z",
              .created_by = "creator",
              .updated_by = "editor",
              .content_version = 4,
          },
      .snapshot =
          cppwiki::document::BlockNoteDocumentSnapshot{
              .id = std::string("page-1"),
              .schema_version = 1,
              .title = std::string("File Repo Page"),
              .blocks = std::vector<cppwiki::document::BlockNoteBlockSnapshot>{},
          },
      .raw_snapshot_json =
          R"({"id":"page-1","schema_version":1,"title":"File Repo Page","blocks":[]})",
  };
}

auto MakeConflict() -> cppwiki::storage::DocumentConflictRecord {
  return cppwiki::storage::DocumentConflictRecord{
      .id = "conflict-1",
      .document_id = "page-1",
      .workspace_id = "engineering",
      .base_version = 4,
      .local_snapshot = R"({"title":"Local"})",
      .remote_snapshot = R"({"title":"Remote"})",
      .local_updated_by = "alice",
      .remote_updated_by = "bob",
      .detected_at = "2026-06-30T10:06:00.000Z",
      .resolution_state = "pending",
  };
}

auto TestFileRepositoryPersistsDocumentsAndConflicts() -> void {
  const auto storage_directory = std::filesystem::temp_directory_path() / "cppwiki-file-repo-test";
  std::filesystem::remove_all(storage_directory);

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  const auto document = MakeDocument();
  const auto save_document = repository.SaveDocument(document);
  Require(!save_document.error, "file repository document save should succeed");

  const auto load_document = repository.LoadDocument("page-1");
  Require(load_document.document.has_value(), "file repository should load saved document");
  Require(load_document.document->metadata.updated_by == "editor",
          "file repository should preserve updated_by");
  Require(load_document.document->metadata.content_version == 4,
          "file repository should preserve content_version");

  const auto save_conflict = repository.SaveConflict(MakeConflict());
  Require(!save_conflict.error, "file repository conflict save should succeed");

  const auto load_conflict = repository.LoadConflict("conflict-1");
  Require(load_conflict.conflict.has_value(), "file repository should load saved conflict");
  Require(load_conflict.conflict->remote_updated_by == "bob",
          "file repository should preserve remote_updated_by");

  const auto list_conflicts = repository.ListConflicts();
  Require(!list_conflicts.error, "file repository conflict list should succeed");
  Require(list_conflicts.conflicts.size() == 1, "file repository should list saved conflict");

  const auto resolve_conflict = repository.ResolveConflict("conflict-1");
  Require(!resolve_conflict.error, "file repository should resolve conflict");
  Require(repository.LoadConflict("conflict-1").conflict->resolution_state == "resolved",
          "file repository should persist resolved state");

  const auto dismiss_conflict = repository.DismissConflict("conflict-1");
  Require(!dismiss_conflict.error, "file repository should dismiss conflict");
  Require(repository.LoadConflict("conflict-1").conflict->resolution_state == "dismissed",
          "file repository should persist dismissed state");

  const auto sync_status = repository.GetSyncStatus();
  Require(!sync_status.has_conflicts,
          "file repository sync status should ignore non-pending conflicts");
  Require(sync_status.conflict_count == 0,
          "file repository sync status should count only pending conflicts");

  const auto delete_document = repository.DeleteDocument("page-1");
  Require(!delete_document.error, "file repository document delete should succeed");
  Require(!repository.LoadDocument("page-1").document.has_value(),
          "file repository should delete document");
  Require(!repository.LoadConflict("conflict-1").conflict.has_value(),
          "file repository should delete related conflicts with document");

  std::filesystem::remove_all(storage_directory);
}

// ADR-017: DocumentKind must round-trip through the file repository for every kind, not just
// the (default) wiki page — the on-disk DTO only started carrying this field with this change,
// so this also guards against regressing the kWikiPage-fallback behavior for older records.
auto TestFileRepositoryRoundTripsDocumentKind() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-file-repo-kind-test";
  std::filesystem::remove_all(storage_directory);

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  const auto kinds = std::vector<cppwiki::document::DocumentKind>{
      cppwiki::document::DocumentKind::kWikiPage,
      cppwiki::document::DocumentKind::kJupyterNotebook,
      cppwiki::document::DocumentKind::kExcalidrawCanvas,
  };

  for (const auto kind : kinds) {
    auto document = MakeDocument();
    document.metadata.id = "kind-page-" + cppwiki::document::ToDocumentKindKey(kind);
    document.metadata.kind = kind;

    const auto save_result = repository.SaveDocument(document);
    Require(!save_result.error, "file repository should save a document of any kind");

    const auto load_result = repository.LoadDocument(document.metadata.id);
    Require(load_result.document.has_value(), "file repository should load a document of any kind");
    Require(load_result.document->metadata.kind == kind,
            "file repository should round-trip the saved DocumentKind exactly");
  }

  std::filesystem::remove_all(storage_directory);
}

// Reported after #78-#81: newly created Jupyter notebook/Excalidraw canvas documents appeared
// broken after saving. TestFileRepositoryRoundTripsDocumentKind above only checks the `kind`
// field round-trips; this checks the actual nbformat/Excalidraw-shaped raw_snapshot_json content
// (not the generic BlockNote-shaped fixture from MakeDocument()) survives a real disk
// save/load cycle byte-for-byte, ruling out any reflect-cpp JSON-in-JSON escaping issue specific
// to the file-based repository (as opposed to the in-memory FakeDocumentRepository used by the
// bridge-level regression tests in editor_bridge_test.cc, which already pass).
auto TestFileRepositoryRoundTripsNonWikiPageRawSnapshotContent() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-file-repo-raw-content-test";
  std::filesystem::remove_all(storage_directory);

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  const auto notebook_json =
      std::string(R"({"cells":[{"cell_type":"markdown","source":["Hi"],"metadata":{}}],)"
                  R"("metadata":{},"nbformat":4,"nbformat_minor":5})");
  auto notebook_document = MakeDocument();
  notebook_document.metadata.id = "notebook-content-test";
  notebook_document.metadata.kind = cppwiki::document::DocumentKind::kJupyterNotebook;
  notebook_document.raw_snapshot_json = notebook_json;
  Require(!repository.SaveDocument(notebook_document).error,
          "file repository should save a notebook document");
  const auto loaded_notebook = repository.LoadDocument("notebook-content-test");
  Require(loaded_notebook.document.has_value(), "file repository should load the notebook back");
  Require(loaded_notebook.document->raw_snapshot_json == notebook_json,
          "notebook raw_snapshot_json must round-trip byte-for-byte through the file repository");

  const auto excalidraw_json = std::string(
      R"({"type":"excalidraw","version":2,"elements":[{"id":"el1","type":"rectangle"}],)"
      R"("appState":{"viewBackgroundColor":"#ffffff"},"files":{}})");
  auto excalidraw_document = MakeDocument();
  excalidraw_document.metadata.id = "excalidraw-content-test";
  excalidraw_document.metadata.kind = cppwiki::document::DocumentKind::kExcalidrawCanvas;
  excalidraw_document.raw_snapshot_json = excalidraw_json;
  Require(!repository.SaveDocument(excalidraw_document).error,
          "file repository should save an excalidraw document");
  const auto loaded_excalidraw = repository.LoadDocument("excalidraw-content-test");
  Require(loaded_excalidraw.document.has_value(),
          "file repository should load the excalidraw canvas back");
  Require(loaded_excalidraw.document->raw_snapshot_json == excalidraw_json,
          "excalidraw raw_snapshot_json must round-trip byte-for-byte through the file "
          "repository");

  // Issue #95: Mermaid source is saved as a normal BlockNote inline-content snapshot. Keep a
  // multi-line source in this real file-repository round trip so Enter-created newlines cannot
  // be lost through JSON-in-JSON serialization or a later load from disk.
  const auto mermaid_snapshot =
      std::string(R"([{"id":"mermaid-1","type":"mermaid","props":{},"content":[)"
                  R"({"type":"text","text":"graph TD\n  A[Start] --> B[Finish]","styles":{}}],)"
                  R"("children":[]}])");
  auto mermaid_document = MakeDocument();
  mermaid_document.metadata.id = "mermaid-content-test";
  mermaid_document.raw_snapshot_json = mermaid_snapshot;
  Require(!repository.SaveDocument(mermaid_document).error,
          "file repository should save a multi-line Mermaid snapshot");
  const auto loaded_mermaid = repository.LoadDocument("mermaid-content-test");
  Require(loaded_mermaid.document.has_value(), "file repository should load Mermaid back");
  Require(loaded_mermaid.document->raw_snapshot_json == mermaid_snapshot,
          "multi-line Mermaid raw_snapshot_json must round-trip byte-for-byte through disk");

  std::filesystem::remove_all(storage_directory);
}

// Issue #162: simulate a crash between WriteFileAtomically()'s two rename steps -- the previous
// content has already been renamed to `<page>.json.backup`, and the new content is fully written
// to `<page>.json.tmp`, but the final rename that swaps it into place never ran. Opening the
// repository (constructing FileDocumentRepository, not just calling SaveDocument/LoadDocument)
// must recover by completing that swap, so the in-flight edit isn't silently lost.
auto TestFileRepositoryCompletesInterruptedWriteFromTempFile() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-file-repo-crash-tmp-test";
  std::filesystem::remove_all(storage_directory);
  std::filesystem::create_directories(storage_directory / "pages");

  const auto page_path = storage_directory / "pages" / "page-1.json";
  const auto old_content = std::string(R"({"id":"page-1","title":"Old"})");
  const auto new_content = std::string(R"({"id":"page-1","title":"New"})");

  {
    std::ofstream backup_file(page_path.string() + ".backup", std::ios::binary);
    backup_file << old_content;
  }
  {
    std::ofstream temp_file(page_path.string() + ".tmp", std::ios::binary);
    temp_file << new_content;
  }
  Require(!std::filesystem::exists(page_path),
          "test setup should leave the target file itself missing, matching the crash window");

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  Require(std::filesystem::exists(page_path),
          "opening the repository should complete the interrupted write");
  Require(!std::filesystem::exists(page_path.string() + ".tmp"),
          "the temp file should be consumed by the recovery");
  Require(!std::filesystem::exists(page_path.string() + ".backup"),
          "the stale backup should be removed once the newer content is in place");

  std::ifstream recovered(page_path, std::ios::binary);
  const auto recovered_content =
      std::string(std::istreambuf_iterator<char>(recovered), std::istreambuf_iterator<char>());
  Require(recovered_content == new_content,
          "recovery should keep the newer (fully-written) content, not the older backup");

  std::filesystem::remove_all(storage_directory);
}

// Crash right after WriteFileAtomically() renamed the target to `.backup`, with no `.tmp` file to
// recover (e.g. the temp write step itself never reached disk before the kill). The only
// recoverable content is the backup -- restore it rather than leaving the page missing entirely.
auto TestFileRepositoryRestoresFromBackupWhenNoTempFileExists() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-file-repo-crash-backup-test";
  std::filesystem::remove_all(storage_directory);
  std::filesystem::create_directories(storage_directory / "pages");

  const auto page_path = storage_directory / "pages" / "page-1.json";
  const auto backed_up_content = std::string(R"({"id":"page-1","title":"Backed Up"})");
  {
    std::ofstream backup_file(page_path.string() + ".backup", std::ios::binary);
    backup_file << backed_up_content;
  }

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  Require(std::filesystem::exists(page_path),
          "opening the repository should restore the page from its backup");
  Require(!std::filesystem::exists(page_path.string() + ".backup"),
          "the backup should be consumed by the restore");

  std::ifstream recovered(page_path, std::ios::binary);
  const auto recovered_content =
      std::string(std::istreambuf_iterator<char>(recovered), std::istreambuf_iterator<char>());
  Require(recovered_content == backed_up_content,
          "recovery should restore exactly the backed-up content");

  std::filesystem::remove_all(storage_directory);
}

// Crash after the final rename completed but before WriteFileAtomically() removed the now-stale
// backup: the target already holds the newest content, so recovery must only clean up the leftover
// backup, never restore it (which would silently revert the page to its older content).
auto TestFileRepositoryCleansUpStaleBackupWithoutOverwritingNewerTarget() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-file-repo-crash-cleanup-test";
  std::filesystem::remove_all(storage_directory);
  std::filesystem::create_directories(storage_directory / "pages");

  const auto page_path = storage_directory / "pages" / "page-1.json";
  const auto current_content = std::string(R"({"id":"page-1","title":"Current"})");
  const auto stale_backup_content = std::string(R"({"id":"page-1","title":"Stale"})");
  {
    std::ofstream target_file(page_path, std::ios::binary);
    target_file << current_content;
  }
  {
    std::ofstream backup_file(page_path.string() + ".backup", std::ios::binary);
    backup_file << stale_backup_content;
  }

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  Require(!std::filesystem::exists(page_path.string() + ".backup"),
          "the stale backup should be cleaned up");

  std::ifstream target(page_path, std::ios::binary);
  const auto target_content =
      std::string(std::istreambuf_iterator<char>(target), std::istreambuf_iterator<char>());
  Require(target_content == current_content,
          "the already-current target must not be overwritten by a stale backup");

  std::filesystem::remove_all(storage_directory);
}

// Issue #167: a page file that exists but fails to parse (corrupted/unparseable JSON) must not
// take down ListDocuments() for the rest of the workspace, and must not just silently vanish
// either -- it should come back as a placeholder DocumentSummary the caller can recognize and
// show as an error in the tree, instead of the page disappearing with no trace.
auto TestFileRepositoryReportsCorruptedDocumentAsPlaceholderInsteadOfHidingIt() -> void {
  const auto storage_directory =
      std::filesystem::temp_directory_path() / "cppwiki-file-repo-corrupted-test";
  std::filesystem::remove_all(storage_directory);

  cppwiki::storage::FileDocumentRepository repository(
      cppwiki::storage::FileDocumentRepositoryOptions{.storage_directory = storage_directory});

  auto healthy_document = MakeDocument();
  healthy_document.metadata.id = "healthy-page";
  Require(!repository.SaveDocument(healthy_document).error,
          "file repository should save the healthy document");

  // Write the corrupted page directly to disk (not through SaveDocument/WriteFileAtomically),
  // simulating disk-level corruption rather than an interrupted write.
  const auto corrupted_page_path = storage_directory / "pages" / "corrupted-page.json";
  {
    std::ofstream corrupted_file(corrupted_page_path, std::ios::binary);
    corrupted_file << "{not valid json";
  }

  const auto list_result = repository.ListDocuments();
  Require(!list_result.error, "listing documents should still succeed with a corrupted entry");
  Require(list_result.documents.size() == 2,
          "the healthy document and a placeholder for the corrupted one should both be listed");

  const auto healthy_summary = std::ranges::find_if(
      list_result.documents, [](const auto& doc) { return doc.id == "healthy-page"; });
  Require(healthy_summary != list_result.documents.end(),
          "the healthy document should still be listed");
  Require(!healthy_summary->is_corrupted, "the healthy document should not be marked corrupted");

  const auto corrupted_summary = std::ranges::find_if(
      list_result.documents, [](const auto& doc) { return doc.id == "corrupted-page"; });
  Require(corrupted_summary != list_result.documents.end(),
          "the corrupted document should appear as a placeholder, not be silently dropped");
  Require(corrupted_summary->is_corrupted,
          "the corrupted document's placeholder should be flagged as corrupted");
  Require(corrupted_summary->load_error.has_value(),
          "the corrupted document's placeholder should carry the underlying load error");

  const auto load_result = repository.LoadDocument("corrupted-page");
  Require(!load_result.document.has_value(),
          "loading the corrupted document directly should still fail");
  Require(load_result.error.has_value(),
          "loading the corrupted document directly should still report the error");

  std::filesystem::remove_all(storage_directory);
}

}  // namespace

auto main() -> int {
  TestFileRepositoryPersistsDocumentsAndConflicts();
  TestFileRepositoryRoundTripsDocumentKind();
  TestFileRepositoryRoundTripsNonWikiPageRawSnapshotContent();
  TestFileRepositoryCompletesInterruptedWriteFromTempFile();
  TestFileRepositoryRestoresFromBackupWhenNoTempFileExists();
  TestFileRepositoryCleansUpStaleBackupWithoutOverwritingNewerTarget();
  TestFileRepositoryReportsCorruptedDocumentAsPlaceholderInsteadOfHidingIt();
  spdlog::info("cppwiki_file_document_repository_tests passed");
  return EXIT_SUCCESS;
}
