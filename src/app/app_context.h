#ifndef CPPWIKI_SRC_APP_APP_CONTEXT_H_
#define CPPWIKI_SRC_APP_APP_CONTEXT_H_

#include <memory>

#include "app/program_settings.h"
#include "storage/local_document_repository.h"

namespace cppwiki {

// Application context that provides access to shared services and configuration.
// This is passed to GUI components instead of individual dependencies.
struct AppContext {
  ProgramSettings settings;
  std::shared_ptr<storage::LocalDocumentRepository> document_repository;

  AppContext(ProgramSettings settings,
             std::shared_ptr<storage::LocalDocumentRepository> repository)
      : settings(std::move(settings)), document_repository(std::move(repository)) {}
};

}  // namespace cppwiki

#endif  // CPPWIKI_SRC_APP_APP_CONTEXT_H_
