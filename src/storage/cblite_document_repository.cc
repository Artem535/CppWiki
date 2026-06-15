#include "storage/cblite_document_repository.h"

#include <cbl++/CouchbaseLite.hh>

#include <exception>
#include <utility>

namespace cppwiki::storage {
namespace {

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

    return SaveDocumentResult{
        .error = MakeError(RepositoryErrorCode::kUnsupported,
                           "Couchbase Lite document save is not implemented yet for page '" +
                               document.metadata.id + "'."),
    };
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = error,
      };
    }

    return LoadDocumentResult{
        .document = std::nullopt,
        .error = MakeError(RepositoryErrorCode::kUnsupported,
                           "Couchbase Lite document load is not implemented yet for page '" +
                               std::string(page_id) + "'."),
    };
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
