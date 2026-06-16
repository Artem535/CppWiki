#ifndef CPPWIKI_SRC_STORAGE_CBLITE_DOCUMENT_REPOSITORY_H_
#define CPPWIKI_SRC_STORAGE_CBLITE_DOCUMENT_REPOSITORY_H_

#include <filesystem>
#include <memory>
#include <string>

#include "storage/local_document_repository.h"

namespace cppwiki::storage {

struct CbliteDocumentRepositoryOptions {
  std::filesystem::path database_directory;
  std::string database_name{"cppwiki"};
};

class CbliteDocumentRepository final : public LocalDocumentRepository {
 public:
  explicit CbliteDocumentRepository(CbliteDocumentRepositoryOptions options);
  ~CbliteDocumentRepository() override;

  CbliteDocumentRepository(const CbliteDocumentRepository&) = delete;
  auto operator=(const CbliteDocumentRepository&) -> CbliteDocumentRepository& = delete;
  CbliteDocumentRepository(CbliteDocumentRepository&&) = delete;
  auto operator=(CbliteDocumentRepository&&) -> CbliteDocumentRepository& = delete;

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult override;
  [[nodiscard]] auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult override;
  [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult override;
  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_CBLITE_DOCUMENT_REPOSITORY_H_
