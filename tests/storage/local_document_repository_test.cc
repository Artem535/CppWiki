#include "storage/local_document_repository.h"

#include <spdlog/spdlog.h>

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

 private:
  std::optional<cppwiki::storage::DocumentRecord> document_;
};

auto TestRepositoryInterfaceStoresValidatedRawSnapshot() -> void {
  FakeDocumentRepository repository;

  cppwiki::storage::DocumentRecord document{
      .metadata =
          cppwiki::document::PageMetadata{
              .id = "page-1",
              .schema_version = cppwiki::document::SchemaVersion::kV1,
              .title = "Getting Started",
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
  Require(load_result.document->raw_snapshot_json == document.raw_snapshot_json,
          "loaded raw snapshot should match");
}

}  // namespace

auto main() -> int {
  TestRepositoryInterfaceStoresValidatedRawSnapshot();

  spdlog::info("cppwiki_local_document_repository_tests passed");
  return EXIT_SUCCESS;
}
