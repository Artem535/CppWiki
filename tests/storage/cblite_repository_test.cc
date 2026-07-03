#include "storage/cblite_document_repository.h"
#include "sync/sync_bootstrap.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>
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

auto WaitUntil(const std::function<bool()>& predicate, int attempts = 40,
               std::chrono::milliseconds delay = std::chrono::milliseconds(50)) -> bool {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(delay);
  }
  return false;
}

auto TestCbliteRepositorySaveLoad() -> void {
  const auto test_directory =
      std::filesystem::temp_directory_path() / "cppwiki-cblite-test" / "nested" / "database";
  std::filesystem::remove_all(test_directory);

  cppwiki::storage::CbliteDocumentRepositoryOptions options{
      .database_directory = test_directory,
      .database_name = "test_wiki_db",
  };

  {
    cppwiki::storage::CbliteDocumentRepository repository(options);

    cppwiki::storage::DocumentRecord original_document{
        .metadata = {
            .id = "page-123",
            .schema_version = cppwiki::document::SchemaVersion::kV1,
            .title = "Test Page",
            .workspace_id = "engineering",
            .parent_id = std::make_optional<std::string>("parent-page"),
            .sort_order = 42,
            .created_at = "2026-06-16T08:00:00.000Z",
            .updated_at = "2026-06-16T08:01:00.000Z",
            .created_by = "creator",
            .updated_by = "editor",
            .content_version = 5,
        },
        .snapshot = {
            .id = std::string("page-123"),
            .schema_version = 1,
            .title = std::string("Test Page"),
            .blocks = std::vector<cppwiki::document::BlockNoteBlockSnapshot>{
                {
                    .id = std::string("block-1"),
                    .type = std::string("paragraph"),
                    .content = std::make_optional(rfl::Generic("Hello from CBLite!")),
                    .props = std::nullopt,
                    .checked = std::nullopt,
                    .children = std::nullopt,
                },
            },
        },
        .raw_snapshot_json = R"({"id":"page-123","schema_version":1,"title":"Test Page","blocks":[{"id":"block-1","type":"paragraph","content":"Hello from CBLite!"}]})",
    };

    const auto save_result = repository.SaveDocument(original_document);
    if (save_result.error) {
      spdlog::error("Save failed: {}", save_result.error->message);
      std::exit(EXIT_FAILURE);
    }

    const auto load_result = repository.LoadDocument("page-123");
    if (!load_result.document) {
      spdlog::error("Load failed: {}",
                    load_result.error ? load_result.error->message : "Unknown error");
      std::exit(EXIT_FAILURE);
    }

    const auto& loaded = *load_result.document;
    if (loaded.metadata.id != "page-123") {
      spdlog::error("ID mismatch: expected page-123, got {}", loaded.metadata.id);
      std::exit(EXIT_FAILURE);
    }
    if (loaded.metadata.title != "Test Page") {
      spdlog::error("Title mismatch: expected 'Test Page', got {}", loaded.metadata.title);
      std::exit(EXIT_FAILURE);
    }
    Require(loaded.metadata.parent_id == std::make_optional<std::string>("parent-page"),
            "Loaded CBLite parent id should match");
    Require(loaded.metadata.workspace_id == "engineering",
            "Loaded CBLite workspace id should match");
    Require(loaded.metadata.sort_order == 42, "Loaded CBLite sort order should match");
    Require(loaded.metadata.created_by == "creator", "Loaded CBLite created_by should match");
    Require(loaded.metadata.updated_by == "editor", "Loaded CBLite updated_by should match");
    Require(loaded.metadata.content_version == 5,
            "Loaded CBLite content_version should match");
    if (loaded.raw_snapshot_json != original_document.raw_snapshot_json) {
      spdlog::error("JSON mismatch");
      std::exit(EXIT_FAILURE);
    }

    const auto list_result = repository.ListDocuments();
    if (list_result.error) {
      spdlog::error("List failed: {}", list_result.error->message);
      std::exit(EXIT_FAILURE);
    }
    Require(list_result.documents.size() == 1, "CBLite list should include saved document");
    Require(list_result.documents.front().id == "page-123", "CBLite list id should match");
    Require(list_result.documents.front().title == "Test Page", "CBLite list title should match");
    Require(list_result.documents.front().parent_id ==
                std::make_optional<std::string>("parent-page"),
            "CBLite list parent id should match");
    Require(list_result.documents.front().workspace_id == "engineering",
            "CBLite list workspace id should match");
    Require(list_result.documents.front().sort_order == 42, "CBLite list sort order should match");
    Require(list_result.documents.front().created_by == "creator",
            "CBLite list created_by should match");
    Require(list_result.documents.front().updated_by == "editor",
            "CBLite list updated_by should match");
    Require(list_result.documents.front().content_version == 5,
            "CBLite list content_version should match");

    cppwiki::storage::DocumentConflictRecord conflict{
        .id = "conflict-1",
        .document_id = "page-123",
        .workspace_id = "engineering",
        .base_version = 5,
        .local_snapshot = R"({"title":"Local"})",
        .remote_snapshot = R"({"title":"Remote"})",
        .local_updated_by = "creator",
        .remote_updated_by = "reviewer",
        .detected_at = "2026-06-30T11:00:00.000Z",
        .resolution_state = "pending",
    };

    const auto save_conflict = repository.SaveConflict(conflict);
    Require(!save_conflict.error, "CBLite conflict save should succeed");

    const auto load_conflict = repository.LoadConflict("conflict-1");
    Require(load_conflict.conflict.has_value(), "CBLite conflict should load");
    Require(load_conflict.conflict->document_id == "page-123",
            "Loaded CBLite conflict document id should match");
    Require(load_conflict.conflict->remote_updated_by == "reviewer",
            "Loaded CBLite conflict remote_updated_by should match");

    const auto list_conflicts = repository.ListConflicts();
    Require(!list_conflicts.error, "CBLite conflict list should succeed");
    Require(list_conflicts.conflicts.size() == 1, "CBLite should list saved conflict");

    const auto sync_status = repository.GetSyncStatus();
    Require(sync_status.has_conflicts, "CBLite sync status should expose pending conflicts");
    Require(sync_status.conflict_count == 1,
            "CBLite sync status should expose pending conflict count");

    const auto resolve_conflict = repository.ResolveConflict("conflict-1");
    Require(!resolve_conflict.error, "CBLite should resolve conflict");
    Require(repository.LoadConflict("conflict-1").conflict->resolution_state == "resolved",
            "CBLite should persist resolved state");

    const auto dismissed_conflict = repository.DismissConflict("conflict-1");
    Require(!dismissed_conflict.error, "CBLite should dismiss conflict");
    Require(repository.LoadConflict("conflict-1").conflict->resolution_state == "dismissed",
            "CBLite should persist dismissed state");

    const auto sync_status_after_resolution = repository.GetSyncStatus();
    Require(!sync_status_after_resolution.has_conflicts,
            "CBLite sync status should ignore non-pending conflicts");
    Require(sync_status_after_resolution.conflict_count == 0,
            "CBLite sync status should count only pending conflicts");

    const auto delete_result = repository.DeleteDocument("page-123");
    Require(!delete_result.error, "CBLite delete should succeed");
    const auto deleted_load = repository.LoadDocument("page-123");
    Require(!deleted_load.document.has_value(), "Deleted CBLite document should not load");
    Require(!repository.LoadConflict("conflict-1").conflict.has_value(),
            "Deleting CBLite document should remove related conflict");
  }

  spdlog::info("CBLite save/load cycle passed successfully");
  std::filesystem::remove_all(test_directory.parent_path().parent_path());
}

auto MakeDocument(std::string id, std::string title, std::int32_t sort_order)
    -> cppwiki::storage::DocumentRecord {
  return cppwiki::storage::DocumentRecord{
      .metadata =
          {
              .id = std::move(id),
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = std::move(title),
              .workspace_id = "engineering",
              .parent_id = std::nullopt,
              .sort_order = sort_order,
              .created_at = "2026-07-01T07:00:00.000Z",
              .updated_at = "2026-07-01T07:00:00.000Z",
              .created_by = "tester",
              .updated_by = "tester",
              .content_version = 1,
          },
      .snapshot =
          {
              .id = std::string("snapshot"),
              .schema_version = 1,
              .title = std::string("snapshot"),
              .blocks = {},
          },
      .raw_snapshot_json = R"({"id":"snapshot","schema_version":1,"title":"snapshot","blocks":[]})",
  };
}

auto TestCbliteRepositoryRefreshesStaleIndexAfterExternalWrite() -> void {
  const auto test_directory =
      std::filesystem::temp_directory_path() / "cppwiki-cblite-stale-index" / "nested" / "database";
  std::filesystem::remove_all(test_directory);

  cppwiki::storage::CbliteDocumentRepositoryOptions options{
      .database_directory = test_directory,
      .database_name = "test_wiki_db",
  };

  cppwiki::storage::CbliteDocumentRepository primary(options);
  cppwiki::storage::CbliteDocumentRepository secondary(options);

  Require(!primary.SaveDocument(MakeDocument("page-1", "First", 1)).error,
          "primary seed document should save");

  const auto initial_list = primary.ListDocuments();
  Require(!initial_list.error, "primary initial list should succeed");
  Require(initial_list.documents.size() == 1, "primary initial list should have one document");

  Require(!secondary.SaveDocument(MakeDocument("page-2", "Second", 2)).error,
          "secondary external write should save");

  bool observed_external_document = false;
  for (int attempt = 0; attempt < 20; ++attempt) {
    const auto list_result = primary.ListDocuments();
    Require(!list_result.error, "primary list after external write should succeed");
    if (list_result.documents.size() == 2) {
      observed_external_document = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  Require(observed_external_document,
          "primary repository should rebuild stale index after external collection change");

  std::filesystem::remove_all(test_directory.parent_path().parent_path());
}

auto TestCbliteRepositoryPromotesLocalDocumentsAfterSyncBootstrap() -> void {
  const auto test_directory =
      std::filesystem::temp_directory_path() / "cppwiki-cblite-promote-local" / "nested" / "database";
  std::filesystem::remove_all(test_directory);

  cppwiki::storage::CbliteDocumentRepository repository(
      cppwiki::storage::CbliteDocumentRepositoryOptions{
          .database_directory = test_directory,
          .database_name = "test_wiki_db",
      });

  Require(!repository.SaveDocument(MakeDocument("page-promote", "Offline Draft", 1)).error,
          "local document should save before sync bootstrap");

  cppwiki::sync::SyncBootstrap bootstrap;
  bootstrap.available = true;
  bootstrap.enabled = true;
  bootstrap.gateway_url = QStringLiteral("http://127.0.0.1:4984/cppwiki");
  bootstrap.database_name = QStringLiteral("cppwiki");
  bootstrap.auth_mode = QStringLiteral("oidc_access_token_passthrough");
  bootstrap.token_passthrough = true;
  bootstrap.channels = {QStringLiteral("workspace:engineering")};

  const auto apply_result = repository.ApplySyncBootstrap(bootstrap);
  if (apply_result.error) {
    spdlog::error("ApplySyncBootstrap failed: {}", apply_result.error->message);
  }
  Require(!apply_result.error,
          "sync bootstrap should promote local workspace documents without collection mismatch");

  const auto loaded = repository.LoadDocument("page-promote");
  Require(loaded.document.has_value(), "promoted document should still load");
  Require(loaded.document->metadata.workspace_id == "engineering",
          "promoted document should preserve workspace id");

  std::filesystem::remove_all(test_directory.parent_path().parent_path());
}

auto MakePendingConflict(std::string id, std::string document_id, std::string detected_at,
                         std::string local_updated_by, std::string remote_updated_by)
    -> cppwiki::storage::DocumentConflictRecord {
  return cppwiki::storage::DocumentConflictRecord{
      .id = std::move(id),
      .document_id = std::move(document_id),
      .workspace_id = "engineering",
      .base_version = 2,
      .local_snapshot = R"({"title":"Local version"})",
      .remote_snapshot = R"({"title":"Remote version"})",
      .local_updated_by = std::move(local_updated_by),
      .remote_updated_by = std::move(remote_updated_by),
      .detected_at = std::move(detected_at),
      .resolution_state = "pending",
  };
}

auto TestCbliteRepositoryConflictLifecycleFromExternalPendingRecord() -> void {
  const auto test_directory =
      std::filesystem::temp_directory_path() / "cppwiki-cblite-conflict-lifecycle" / "nested" /
      "database";
  std::filesystem::remove_all(test_directory);

  cppwiki::storage::CbliteDocumentRepositoryOptions options{
      .database_directory = test_directory,
      .database_name = "test_wiki_db",
  };

  cppwiki::storage::CbliteDocumentRepository primary(options);
  cppwiki::storage::CbliteDocumentRepository secondary(options);

  Require(!primary.SaveDocument(MakeDocument("page-conflict", "Base", 1)).error,
          "primary should seed base document for conflict lifecycle test");

  const auto first_pending = MakePendingConflict("conflict-e2e-1", "page-conflict",
                                                 "2026-07-01T11:00:00.000Z", "alice", "bob");
  Require(!secondary.SaveConflict(first_pending).error,
          "secondary should persist externally detected pending conflict");

  const auto saw_pending_conflict = WaitUntil([&primary]() {
    const auto list = primary.ListConflicts();
    return !list.error && list.conflicts.size() == 1 &&
           list.conflicts.front().resolution_state == "pending";
  });
  Require(saw_pending_conflict,
          "primary should observe externally persisted pending conflict through repository index refresh");

  const auto pending_status = primary.GetSyncStatus();
  Require(pending_status.has_conflicts,
          "primary sync status should report pending conflict after external ingestion");
  Require(pending_status.conflict_count == 1,
          "primary sync status should report exactly one pending conflict");

  const auto resolve_result = primary.ResolveConflict("conflict-e2e-1");
  Require(!resolve_result.error, "primary should resolve externally ingested pending conflict");

  const auto resolved_state_replicated = WaitUntil([&secondary]() {
    const auto loaded = secondary.LoadConflict("conflict-e2e-1");
    return loaded.conflict.has_value() && loaded.conflict->resolution_state == "resolved";
  });
  Require(resolved_state_replicated,
          "resolved state should be visible across repository instances sharing the same CBLite db");

  const auto status_after_resolve = primary.GetSyncStatus();
  Require(!status_after_resolve.has_conflicts,
          "resolved conflict should no longer contribute to pending conflict status");
  Require(status_after_resolve.conflict_count == 0,
          "resolved conflict should decrement pending conflict count to zero");

  const auto second_pending = MakePendingConflict("conflict-e2e-2", "page-conflict",
                                                  "2026-07-01T11:05:00.000Z", "carol", "dave");
  Require(!secondary.SaveConflict(second_pending).error,
          "secondary should persist a second pending conflict for dismiss path");

  const auto saw_second_pending = WaitUntil([&primary]() {
    const auto list = primary.ListConflicts();
    if (list.error) {
      return false;
    }
    return std::ranges::any_of(list.conflicts, [](const auto& conflict) {
      return conflict.id == "conflict-e2e-2" && conflict.resolution_state == "pending";
    });
  });
  Require(saw_second_pending,
          "primary should observe second pending conflict before dismiss flow");

  const auto dismiss_result = primary.DismissConflict("conflict-e2e-2");
  Require(!dismiss_result.error, "primary should dismiss externally ingested pending conflict");

  const auto dismissed_state_replicated = WaitUntil([&secondary]() {
    const auto loaded = secondary.LoadConflict("conflict-e2e-2");
    return loaded.conflict.has_value() && loaded.conflict->resolution_state == "dismissed";
  });
  Require(dismissed_state_replicated,
          "dismissed state should be visible across repository instances sharing the same CBLite db");

  const auto status_after_dismiss = primary.GetSyncStatus();
  Require(!status_after_dismiss.has_conflicts,
          "dismissed conflict should not contribute to pending conflict status");
  Require(status_after_dismiss.conflict_count == 0,
          "dismissed conflict should keep pending conflict count at zero");

  std::filesystem::remove_all(test_directory.parent_path().parent_path());
}

auto TestCbliteRepositoryOfflineEditReconnectPushPull() -> void {
  const auto test_directory =
      std::filesystem::temp_directory_path() / "cppwiki-cblite-offline-e2e" / "nested" / "database";
  std::filesystem::remove_all(test_directory);

  cppwiki::storage::CbliteDocumentRepositoryOptions options{
      .database_directory = test_directory,
      .database_name = "test_wiki_db",
  };

  cppwiki::storage::CbliteDocumentRepository client_a(options);
  cppwiki::storage::CbliteDocumentRepository client_b(options);

  // 1. Client A and B are online and see the seeded document
  Require(!client_a.SaveDocument(MakeDocument("page-e2e-1", "Initial Content", 1)).error,
          "Client A seeds initial document");

  const auto list_b = client_b.ListDocuments();
  Require(list_b.documents.size() == 1, "Client B receives the initial document");

  // 2. Client A goes offline (offline edit)
  auto doc_a = client_a.LoadDocument("page-e2e-1");
  Require(doc_a.document.has_value(), "Client A loads document");
  doc_a.document->metadata.title = "Offline Edited Title";
  doc_a.document->metadata.updated_at = "2026-07-01T13:00:00.000Z";
  doc_a.document->metadata.updated_by = "client-a";

  Require(!client_a.SaveDocument(*doc_a.document).error, "Client A saves offline edit");

  // 3. Client B reconnects and pulls changes
  bool b_received_changes = false;
  for (int attempt = 0; attempt < 20; ++attempt) {
    auto doc_b = client_b.LoadDocument("page-e2e-1");
    if (doc_b.document.has_value() && doc_b.document->metadata.title == "Offline Edited Title") {
      b_received_changes = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  Require(b_received_changes, "Client B successfully receives Client A's offline edits");

  std::filesystem::remove_all(test_directory.parent_path().parent_path());
}

}  // namespace

auto main() -> int {
  try {
    TestCbliteRepositorySaveLoad();
    TestCbliteRepositoryRefreshesStaleIndexAfterExternalWrite();
    TestCbliteRepositoryPromotesLocalDocumentsAfterSyncBootstrap();
    TestCbliteRepositoryConflictLifecycleFromExternalPendingRecord();
    TestCbliteRepositoryOfflineEditReconnectPushPull();
  } catch (const std::exception& e) {
    spdlog::error("Unhandled exception: {}", e.what());
    return EXIT_FAILURE;
  }

  spdlog::info("cppwiki_cblite_repository_tests passed");
  return EXIT_SUCCESS;
}
