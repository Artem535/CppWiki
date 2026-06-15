// Suppress upstream header warnings before including CBLite headers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wattributes"

#include "storage/cblite_document_repository.h"

#include <cbl++/CouchbaseLite.hh>

#pragma clang diagnostic pop

#include <rfl/json.hpp>

#include <exception>
#include <utility>

namespace cppwiki::storage {
namespace {

constexpr std::string_view kDocumentsCollectionName = "documents";

auto Slice(std::string_view value) -> cbl::slice {
  return cbl::slice(value.data(), value.size());
}

auto MakeError(RepositoryErrorCode code, std::string message) -> RepositoryError {
  return RepositoryError{
      .code = code,
      .message = std::move(message),
  };
}

auto CbliteErrorMessage(const CBLError& error) -> std::string {
  return "Couchbase Lite error domain=" + std::to_string(static_cast<int>(error.domain)) +
         " code=" + std::to_string(error.code);
}

}  // namespace

class CbliteDocumentRepository::Impl {
 public:
  explicit Impl(CbliteDocumentRepositoryOptions options) : options_(std::move(options)) {}

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return SaveDocumentResult{.error = error};
    }

      try {
        auto doc = collection_->getMutableDocument(Slice(document.metadata.id));
        doc.set("title", Slice(document.metadata.title));
        doc.set("raw_snapshot", Slice(document.raw_snapshot_json));

        collection_->saveDocument(doc);
        return SaveDocumentResult{};
      } catch (const CBLError& error) {
        return SaveDocumentResult{.error = MakeError(RepositoryErrorCode::kWriteFailed, CbliteErrorMessage(error))};
      } catch (const std::exception& error) {
        return SaveDocumentResult{.error = MakeError(RepositoryErrorCode::kWriteFailed, error.what())};
      }
    }

    [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult {
      if (const auto error = EnsureDatabaseOpen()) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = error,
        };
      }

      try {
        auto doc = collection_->getDocument(Slice(page_id));
        if (!doc) {
          return LoadDocumentResult{
              .document = std::nullopt,
              .error = MakeError(RepositoryErrorCode::kReadFailed, "Document not found in CBLite"),
          };
        }

        DocumentRecord record;
        record.metadata.id = std::string(page_id);
        // Document properties are accessed via Dict interface
        auto props = doc.properties();
        record.metadata.title = std::string(props["title"].asString());
        record.raw_snapshot_json = std::string(props["raw_snapshot"].asString());

        try {
          auto result = rfl::json::read<document::BlockNoteDocumentSnapshot>(record.raw_snapshot_json);
        if (!result) {
          return LoadDocumentResult{
              .document = std::nullopt,
              .error = MakeError(RepositoryErrorCode::kInvalidRecord, 
                                 std::string("Failed to deserialize snapshot: ") + result.error().what()),
          };
        }
        record.snapshot = result.value();
      } catch (const std::exception& e) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord, std::string("Failed to deserialize snapshot: ") + e.what()),
        };
      }

      return LoadDocumentResult{
          .document = std::make_optional(std::move(record)),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

 private:
  [[nodiscard]] auto EnsureDatabaseOpen() -> std::optional<RepositoryError> {
    if (database_) {
      return std::nullopt;
    }

    try {
      auto config = CBLDatabaseConfiguration_Default();
      database_directory_ = options_.database_directory.string();
      config.directory = Slice(database_directory_);
      database_ = std::make_unique<cbl::Database>(Slice(options_.database_name), config);
      if (!static_cast<bool>(*database_)) {
        return MakeError(RepositoryErrorCode::kOpenFailed,
                         "Couchbase Lite database did not open.");
      }
      collection_ = std::make_unique<cbl::Collection>(
          database_->createCollection(Slice(kDocumentsCollectionName)));
      return std::nullopt;
    } catch (const CBLError& error) {
      return MakeError(RepositoryErrorCode::kOpenFailed, CbliteErrorMessage(error));
    } catch (const std::exception& error) {
      return MakeError(RepositoryErrorCode::kOpenFailed, error.what());
    }
  }

  CbliteDocumentRepositoryOptions options_;
  std::string database_directory_;
  std::unique_ptr<cbl::Database> database_;
  std::unique_ptr<cbl::Collection> collection_;
};

CbliteDocumentRepository::CbliteDocumentRepository(CbliteDocumentRepositoryOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

CbliteDocumentRepository::~CbliteDocumentRepository() = default;

auto CbliteDocumentRepository::SaveDocument(const DocumentRecord& document)
    -> SaveDocumentResult {
  return impl_->SaveDocument(document);
}

auto CbliteDocumentRepository::LoadDocument(std::string_view page_id) -> LoadDocumentResult {
  return impl_->LoadDocument(page_id);
}

}  // namespace cppwiki::storage
