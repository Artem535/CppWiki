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
  cppwiki::storage::CbliteDocumentRepository repository(options);

  cppwiki::storage::DocumentRecord original_document{
      .metadata = {
          .id = "page-123",
          .schema_version = cppwiki::document::SchemaVersion::kV1,
          .title = "Test Page",
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
              },
          },
      },
      .raw_snapshot_json = R"({"id":"page-123","schema_version":1,"title":"Test Page","blocks":[{"id":"block-1","type":"paragraph","content":"Hello from CBLite!"}]})",
  };

  // 1. Save
  const auto save_result = repository.SaveDocument(original_document);
  if (save_result.error) {
    spdlog::error("Save failed: {}", save_result.error->message);
    std::exit(EXIT_FAILURE);
  }

  // 2. Load
  const auto load_result = repository.LoadDocument("page-123");
  if (!load_result.document) {
    spdlog::error("Load failed: {}", load_result.error ? load_result.error->message : "Unknown error");
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
  if (loaded.raw_snapshot_json != original_document.raw_snapshot_json) {
    spdlog::error("JSON mismatch");
    std::exit(EXIT_FAILURE);
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