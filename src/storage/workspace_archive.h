#ifndef CPPWIKI_SRC_STORAGE_WORKSPACE_ARCHIVE_H_
#define CPPWIKI_SRC_STORAGE_WORKSPACE_ARCHIVE_H_

#include <optional>
#include <string>
#include <string_view>

#include "storage/local_document_repository.h"

namespace cppwiki::storage {

struct ExportWorkspaceResult {
  std::optional<RepositoryError> error;
};

struct ImportWorkspaceResult {
  std::optional<std::string> workspace_id;
  std::optional<RepositoryError> error;
};

// Issue #164: a single portable JSON file with everything needed to reconstruct one workspace on
// a clean install -- the workspace root record (if the backend supports one), every document
// (metadata + raw snapshot JSON), and every conflict. Built purely against the
// LocalDocumentRepository interface, so it works with any backend; a backend that doesn't support
// an optional capability (workspace root) just contributes nothing for that part rather than
// failing the whole export/import. Does not include attachments (#148, not yet implemented),
// revision history (#166, not yet merged), or sync/lock state, which have no meaning for a
// local-only backup.
[[nodiscard]] auto ExportWorkspaceToFile(LocalDocumentRepository& repository,
                                         std::string_view workspace_id,
                                         const std::string& destination_path)
    -> ExportWorkspaceResult;

[[nodiscard]] auto ImportWorkspaceFromFile(LocalDocumentRepository& repository,
                                           const std::string& source_path) -> ImportWorkspaceResult;

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_WORKSPACE_ARCHIVE_H_
