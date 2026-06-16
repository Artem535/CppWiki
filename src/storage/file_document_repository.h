#ifndef CPPWIKI_SRC_STORAGE_FILE_DOCUMENT_REPOSITORY_H_
#define CPPWIKI_SRC_STORAGE_FILE_DOCUMENT_REPOSITORY_H_

#include <filesystem>
#include <memory>
#include <string>

#include "storage/local_document_repository.h"

namespace cppwiki::storage {

struct FileDocumentRepositoryOptions {
  std::filesystem::path storage_directory;
};

// File-based JSON document repository.
// Stores documents as individual JSON files with atomic write and backup support.
// This is a fallback implementation when CBLite is not available.
class FileDocumentRepository final : public LocalDocumentRepository {
 public:
  explicit FileDocumentRepository(FileDocumentRepositoryOptions options);
  ~FileDocumentRepository() override;

  FileDocumentRepository(const FileDocumentRepository&) = delete;
  auto operator=(const FileDocumentRepository&) -> FileDocumentRepository& = delete;
  FileDocumentRepository(FileDocumentRepository&&) = delete;
  auto operator=(FileDocumentRepository&&) -> FileDocumentRepository& = delete;

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult override;
  [[nodiscard]] auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult override;
  [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult override;
  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_FILE_DOCUMENT_REPOSITORY_H_
