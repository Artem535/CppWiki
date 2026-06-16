// Suppress upstream header warnings before including CBLite headers
#include "core/constants.h"
#include "storage/cblite_document_repository.h"

#include <cbl++/CouchbaseLite.hh>
#include <rfl/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <filesystem>
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
  const auto message = cbl::alloc_slice(CBLError_Message(&error)).asString();
  std::string result = "Couchbase Lite error domain=" +
                       std::to_string(static_cast<int>(error.domain)) +
                       " code=" + std::to_string(error.code);
  if (!message.empty()) {
    result += " message=" + message;
  }
  return result;
}

auto EnsureDirectoryExists(const std::filesystem::path& directory_path)
    -> std::optional<RepositoryError> {
  try {
    if (directory_path.empty()) {
      return MakeError(RepositoryErrorCode::kOpenFailed,
                       "Couchbase Lite database directory path is empty.");
    }

    if (std::filesystem::exists(directory_path)) {
      if (!std::filesystem::is_directory(directory_path)) {
        return MakeError(RepositoryErrorCode::kOpenFailed,
                         "Couchbase Lite database path exists but is not a directory: " +
                             directory_path.string());
      }
      return std::nullopt;
    }

    std::filesystem::create_directories(directory_path);
    return std::nullopt;
  } catch (const std::exception& error) {
    return MakeError(RepositoryErrorCode::kOpenFailed,
                     "Failed to create Couchbase Lite database directory '" +
                         directory_path.string() + "': " + error.what());
  }
}

auto GetMutableDocument(const cbl::Collection& collection, std::string_view document_id)
    -> cbl::MutableDocument {
  auto existing = collection.getDocument(Slice(document_id));
  if (existing) {
    return existing.mutableCopy();
  }
  return cbl::MutableDocument(Slice(document_id));
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
      spdlog::debug("CBLite SaveDocument: id={} parent={} sort_order={}",
                    document.metadata.id,
                    document.metadata.parent_id.value_or("<root>"),
                    document.metadata.sort_order);
      auto doc = GetMutableDocument(*collection_, document.metadata.id);
      doc.set("title", Slice(document.metadata.title));
      if (document.metadata.parent_id) {
        doc.set("parent_id", Slice(*document.metadata.parent_id));
      } else {
        doc.properties().remove("parent_id");
      }
      doc.set("sort_order", document.metadata.sort_order);
      doc.set("created_at", Slice(document.metadata.created_at));
      doc.set("updated_at", Slice(document.metadata.updated_at));
      doc.set("raw_snapshot", Slice(document.raw_snapshot_json));

      collection_->saveDocument(doc);
      SaveDocumentIndexEntry(document.metadata);
      return SaveDocumentResult{};
    } catch (const CBLError& error) {
      return SaveDocumentResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return SaveDocumentResult{.error =
                                    MakeError(RepositoryErrorCode::kWriteFailed, error.what())};
    }
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return DeleteDocumentResult{.error = error};
    }

    try {
      spdlog::debug("CBLite DeleteDocument: id={}", page_id);
      auto doc = collection_->getDocument(Slice(page_id));
      if (doc) {
        collection_->deleteDocument(doc);
      }
      RemoveDocumentIndexEntry(page_id);
      return DeleteDocumentResult{};
    } catch (const CBLError& error) {
      return DeleteDocumentResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, CbliteErrorMessage(error))};
    } catch (const std::exception& error) {
      return DeleteDocumentResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
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
      spdlog::debug("CBLite LoadDocument: id={}", page_id);
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
      if (const auto parent_id = props["parent_id"]; parent_id) {
        record.metadata.parent_id = std::string(parent_id.asString());
      }
      record.metadata.sort_order = static_cast<std::int32_t>(props["sort_order"].asInt());
      record.metadata.created_at = std::string(props["created_at"].asString());
      record.metadata.updated_at = std::string(props["updated_at"].asString());
      record.raw_snapshot_json = std::string(props["raw_snapshot"].asString());

      try {
        auto result =
            rfl::json::read<document::BlockNoteDocumentSnapshot>(record.raw_snapshot_json);
        if (!result) {
          return LoadDocumentResult{
              .document = std::nullopt,
              .error = MakeError(
                  RepositoryErrorCode::kInvalidRecord,
                  std::string("Failed to deserialize snapshot: ") + result.error().what()),
          };
        }
        record.snapshot = result.value();
      } catch (const std::exception& e) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               std::string("Failed to deserialize snapshot: ") + e.what()),
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

  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult {
    if (const auto error = EnsureDatabaseOpen()) {
      return ListDocumentsResult{
          .documents = {},
          .error = error,
      };
    }

    try {
      std::vector<DocumentSummary> documents;

      spdlog::debug("CBLite ListDocuments: loading index doc {}", constants::kDocumentsIndexDocumentId);
      auto doc = collection_->getDocument(Slice(constants::kDocumentsIndexDocumentId));
      if (!doc) {
        spdlog::debug("CBLite ListDocuments: index doc not found, returning empty list");
        return ListDocumentsResult{};
      }

      auto props = doc.properties();
      for (auto it = props.begin(); it != props.end(); ++it) {
        auto loaded = LoadDocument(it.keyString().asString());
        if (!loaded.document) {
          continue;
        }
        documents.push_back(DocumentSummaryFromMetadata(loaded.document->metadata));
      }

      std::ranges::sort(documents, [](const DocumentSummary& lhs, const DocumentSummary& rhs) {
        if (lhs.sort_order != rhs.sort_order) {
          return lhs.sort_order < rhs.sort_order;
        }
        return lhs.title < rhs.title;
      });

      return ListDocumentsResult{
          .documents = std::move(documents),
          .error = std::nullopt,
      };
    } catch (const CBLError& error) {
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, CbliteErrorMessage(error)),
      };
    } catch (const std::exception& error) {
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what()),
      };
    }
  }

 private:
  void SaveDocumentIndexEntry(const document::PageMetadata& metadata) {
    spdlog::debug("CBLite SaveDocumentIndexEntry: id={} title={}", metadata.id, metadata.title);
    auto index_doc = GetMutableDocument(*collection_, constants::kDocumentsIndexDocumentId);
    index_doc.set(Slice(metadata.id), Slice(metadata.title));
    collection_->saveDocument(index_doc);
  }

  void RemoveDocumentIndexEntry(std::string_view document_id) {
    spdlog::debug("CBLite RemoveDocumentIndexEntry: id={}", document_id);
    auto index_doc = GetMutableDocument(*collection_, constants::kDocumentsIndexDocumentId);
    index_doc.properties().remove(Slice(document_id));
    collection_->saveDocument(index_doc);
  }

  [[nodiscard]] auto EnsureDatabaseOpen() -> std::optional<RepositoryError> {
    if (database_) {
      return std::nullopt;
    }

    try {
      const auto normalized_directory =
          std::filesystem::absolute(options_.database_directory).lexically_normal();
      if (const auto error = EnsureDirectoryExists(normalized_directory)) {
        return error;
      }

      auto config = CBLDatabaseConfiguration_Default();
      database_directory_ = normalized_directory.string();
      config.directory = Slice(database_directory_);
      spdlog::info("CBLite opening database: name={} directory={}",
                   options_.database_name,
                   database_directory_);
      database_ = std::make_unique<cbl::Database>(Slice(options_.database_name), config);
      if (!static_cast<bool>(*database_)) {
        return MakeError(RepositoryErrorCode::kOpenFailed, "Couchbase Lite database did not open.");
      }
      spdlog::info("CBLite database opened: path={}", database_->path());
      spdlog::info("CBLite creating/opening collection: {}", constants::kDocumentsCollectionName);
      collection_ = std::make_unique<cbl::Collection>(
          database_->createCollection(Slice(constants::kDocumentsCollectionName)));
      spdlog::info("CBLite collection ready: {}", constants::kDocumentsCollectionName);
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

auto CbliteDocumentRepository::SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
  return impl_->SaveDocument(document);
}

auto CbliteDocumentRepository::DeleteDocument(std::string_view page_id) -> DeleteDocumentResult {
  return impl_->DeleteDocument(page_id);
}

auto CbliteDocumentRepository::LoadDocument(std::string_view page_id) -> LoadDocumentResult {
  return impl_->LoadDocument(page_id);
}

auto CbliteDocumentRepository::ListDocuments() -> ListDocumentsResult {
  return impl_->ListDocuments();
}

}  // namespace cppwiki::storage
