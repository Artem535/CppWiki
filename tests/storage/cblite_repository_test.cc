#include "storage/cblite_document_repository.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
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

auto TestCbliteRepositorySaveLoad() -> void {
  const auto test_directory = std::filesystem::temp_directory_path() / "cppwiki-cblite-test";
  std::filesystem::remove_all(test_directory);
  std::filesystem::create_directories(test_directory);

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
            .parent_id = std::make_optional<std::string>("parent-page"),
            .sort_order = 42,
            .created_at = "2026-06-16T08:00:00.000Z",
            .updated_at = "2026-06-16T08:01:00.000Z",
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
    Require(loaded.metadata.sort_order == 42, "Loaded CBLite sort order should match");
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
    Require(list_result.documents.front().sort_order == 42, "CBLite list sort order should match");
  }

  spdlog::info("CBLite save/load cycle passed successfully");
  std::filesystem::remove_all(test_directory);
}

}  // namespace

auto main() -> int {
  try {
    TestCbliteRepositorySaveLoad();
  } catch (const std::exception& e) {
    spdlog::error("Unhandled exception: {}", e.what());
    return EXIT_FAILURE;
  }

  spdlog::info("cppwiki_cblite_repository_tests passed");
  return EXIT_SUCCESS;
}
