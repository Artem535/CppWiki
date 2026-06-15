#ifndef CPPWIKI_SRC_STORAGE_REPOSITORY_FACTORY_H_
#define CPPWIKI_SRC_STORAGE_REPOSITORY_FACTORY_H_

#include <memory>

#include "app/program_settings.h"
#include "storage/local_document_repository.h"

namespace cppwiki::storage {

// Factory for creating document repositories.
// Automatically selects the best available storage implementation:
// - CBLite if CPPWIKI_ENABLE_CBLITE_STORAGE is defined and available
// - File-based JSON storage as fallback
class RepositoryFactory {
 public:
  // Creates a document repository based on availability and build configuration.
  // Returns CBLite repository if enabled, otherwise file-based repository.
  [[nodiscard]] static std::shared_ptr<LocalDocumentRepository> Create(
      const ProgramSettings& settings);

  // Explicitly create a file-based repository.
  [[nodiscard]] static std::shared_ptr<LocalDocumentRepository> CreateFileRepository(
      const ProgramSettings& settings);

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
  // Explicitly create a CBLite repository (only available when CBLite is enabled).
  [[nodiscard]] static std::shared_ptr<LocalDocumentRepository> CreateCBLiteRepository(
      const ProgramSettings& settings);
#endif
};

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_REPOSITORY_FACTORY_H_
