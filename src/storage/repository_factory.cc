#include "storage/repository_factory.h"

#include <spdlog/spdlog.h>

#include "storage/file_document_repository.h"

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
#include "storage/cblite_document_repository.h"
#endif

namespace cppwiki::storage {

std::shared_ptr<LocalDocumentRepository> RepositoryFactory::Create(
    const ProgramSettings& settings) {
#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
  try {
    auto repo = CreateCBLiteRepository(settings);
    spdlog::info("Using CBLite document repository");
    return repo;
  } catch (const std::exception& e) {
    spdlog::warn("CBLite repository creation failed ({}), falling back to file-based storage", e.what());
  }
#endif
  return CreateFileRepository(settings);
}

std::shared_ptr<LocalDocumentRepository> RepositoryFactory::CreateFileRepository(
    const ProgramSettings& settings) {
  spdlog::info("Using file-based JSON document repository");
  FileDocumentRepositoryOptions options{
      .storage_directory = settings.DatabaseDirectory().toStdString(),
  };
  return std::make_shared<FileDocumentRepository>(std::move(options));
}

#ifdef CPPWIKI_ENABLE_CBLITE_STORAGE
std::shared_ptr<LocalDocumentRepository> RepositoryFactory::CreateCBLiteRepository(
    const ProgramSettings& settings) {
  CbliteDocumentRepositoryOptions options{
      .database_directory = settings.DatabaseDirectory().toStdString(),
      .database_name = "cppwiki",
  };
  return std::make_shared<CbliteDocumentRepository>(std::move(options));
}
#endif

}  // namespace cppwiki::storage
