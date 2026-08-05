#include "storage/workspace_archive.h"

#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "document/document.h"

namespace cppwiki::storage {
namespace {

constexpr std::int32_t kArchiveSchemaVersion = 1;

struct ArchiveWorkspaceDto {
  std::string workspace_id;
  std::string title;
  std::string created_at;
  std::int64_t schema_version{1};
};

// Mirrors FileDocumentRecordDto's fields (see file_document_repository.cc) -- a separate struct
// on purpose, since the archive format is its own concern and shouldn't be coupled to
// FileDocumentRepository's specific on-disk layout.
struct ArchiveDocumentDto {
  std::string id;
  std::int32_t schema_version{};
  std::optional<std::string> kind;
  std::string title;
  std::string workspace_id;
  std::optional<std::string> parent_id;
  std::int32_t sort_order{};
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
  std::int64_t content_version{1};
  std::string raw_snapshot_json;
};

struct ArchiveConflictDto {
  std::string id;
  std::string document_id;
  std::string workspace_id;
  std::int64_t base_version{};
  std::string local_snapshot;
  std::string remote_snapshot;
  std::string local_updated_by;
  std::string remote_updated_by;
  std::string detected_at;
  std::string resolution_state{"pending"};
};

struct WorkspaceArchiveDto {
  std::int32_t archive_schema_version{kArchiveSchemaVersion};
  std::optional<ArchiveWorkspaceDto> workspace;
  std::vector<ArchiveDocumentDto> documents;
  std::vector<ArchiveConflictDto> conflicts;
};

auto ToDto(const DocumentRecord& document) -> ArchiveDocumentDto {
  return ArchiveDocumentDto{
      .id = document.metadata.id,
      .schema_version = static_cast<std::int32_t>(document.metadata.schema_version),
      .kind = document::ToDocumentKindKey(document.metadata.kind),
      .title = document.metadata.title,
      .workspace_id = document.metadata.workspace_id,
      .parent_id = document.metadata.parent_id,
      .sort_order = document.metadata.sort_order,
      .created_at = document.metadata.created_at,
      .updated_at = document.metadata.updated_at,
      .created_by = document.metadata.created_by,
      .updated_by = document.metadata.updated_by,
      .content_version = document.metadata.content_version,
      .raw_snapshot_json = document.raw_snapshot_json,
  };
}

auto FromDto(ArchiveDocumentDto dto) -> DocumentRecord {
  return DocumentRecord{
      .metadata =
          document::PageMetadata{
              .id = std::move(dto.id),
              .schema_version = document::SchemaVersion::kV1,
              .kind = document::DocumentKindFromKey(dto.kind.value_or(std::string{})),
              .title = std::move(dto.title),
              .workspace_id = std::move(dto.workspace_id),
              .parent_id = std::move(dto.parent_id),
              .sort_order = dto.sort_order,
              .created_at = std::move(dto.created_at),
              .updated_at = std::move(dto.updated_at),
              .created_by = std::move(dto.created_by),
              .updated_by = std::move(dto.updated_by),
              .content_version = dto.content_version,
          },
      .snapshot = document::BlockNoteDocumentSnapshot{},
      .raw_snapshot_json = std::move(dto.raw_snapshot_json),
  };
}

auto ToDto(const DocumentConflictRecord& conflict) -> ArchiveConflictDto {
  return ArchiveConflictDto{
      .id = conflict.id,
      .document_id = conflict.document_id,
      .workspace_id = conflict.workspace_id,
      .base_version = conflict.base_version,
      .local_snapshot = conflict.local_snapshot,
      .remote_snapshot = conflict.remote_snapshot,
      .local_updated_by = conflict.local_updated_by,
      .remote_updated_by = conflict.remote_updated_by,
      .detected_at = conflict.detected_at,
      .resolution_state = conflict.resolution_state,
  };
}

auto FromDto(ArchiveConflictDto dto) -> DocumentConflictRecord {
  return DocumentConflictRecord{
      .id = std::move(dto.id),
      .document_id = std::move(dto.document_id),
      .workspace_id = std::move(dto.workspace_id),
      .base_version = dto.base_version,
      .local_snapshot = std::move(dto.local_snapshot),
      .remote_snapshot = std::move(dto.remote_snapshot),
      .local_updated_by = std::move(dto.local_updated_by),
      .remote_updated_by = std::move(dto.remote_updated_by),
      .detected_at = std::move(dto.detected_at),
      .resolution_state = std::move(dto.resolution_state),
  };
}

auto MakeError(RepositoryErrorCode code, std::string message) -> RepositoryError {
  return RepositoryError{.code = code, .message = std::move(message)};
}

}  // namespace

auto ExportWorkspaceToFile(LocalDocumentRepository& repository, std::string_view workspace_id,
                           const std::string& destination_path) -> ExportWorkspaceResult {
  WorkspaceArchiveDto archive;

  if (const auto root = repository.LoadWorkspaceRoot(workspace_id); root.has_value()) {
    archive.workspace = ArchiveWorkspaceDto{
        .workspace_id = root->workspace_id,
        .title = root->title,
        .created_at = root->created_at,
        .schema_version = root->schema_version,
    };
  }

  const auto documents = repository.ListDocuments();
  if (documents.error && documents.error->code != RepositoryErrorCode::kUnsupported) {
    return ExportWorkspaceResult{.error = documents.error};
  }
  for (const auto& summary : documents.documents) {
    if (summary.workspace_id != workspace_id) {
      continue;
    }
    auto loaded = repository.LoadDocument(summary.id);
    if (loaded.error || !loaded.document) {
      spdlog::warn("Skipping document {} in workspace export: {}", summary.id,
                   loaded.error ? loaded.error->message : "not found");
      continue;
    }
    archive.documents.push_back(ToDto(*loaded.document));
  }

  const auto conflicts = repository.ListConflicts();
  if (conflicts.error && conflicts.error->code != RepositoryErrorCode::kUnsupported) {
    return ExportWorkspaceResult{.error = conflicts.error};
  }
  for (const auto& conflict : conflicts.conflicts) {
    if (conflict.workspace_id == workspace_id) {
      archive.conflicts.push_back(ToDto(conflict));
    }
  }

  std::ofstream out(destination_path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return ExportWorkspaceResult{
        .error = MakeError(RepositoryErrorCode::kWriteFailed,
                           "Could not open " + destination_path + " for writing.")};
  }
  const auto json = rfl::json::write(archive);
  out << json;
  if (!out.good()) {
    return ExportWorkspaceResult{
        .error = MakeError(RepositoryErrorCode::kWriteFailed,
                           "Failed writing workspace archive to " + destination_path)};
  }
  return ExportWorkspaceResult{};
}

auto ImportWorkspaceFromFile(LocalDocumentRepository& repository, const std::string& source_path)
    -> ImportWorkspaceResult {
  std::ifstream in(source_path, std::ios::binary);
  if (!in.is_open()) {
    return ImportWorkspaceResult{
        .error = MakeError(RepositoryErrorCode::kReadFailed,
                           "Could not open " + source_path + " for reading.")};
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();

  auto parsed = rfl::json::read<WorkspaceArchiveDto>(buffer.str());
  if (!parsed) {
    return ImportWorkspaceResult{
        .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                           source_path + " is not a valid workspace archive.")};
  }
  auto archive = std::move(parsed).value();

  std::optional<std::string> workspace_id;
  if (archive.workspace) {
    workspace_id = archive.workspace->workspace_id;
    auto save_result = repository.SaveWorkspaceRoot(WorkspaceRootRecord{
        .workspace_id = archive.workspace->workspace_id,
        .title = archive.workspace->title,
        .created_at = archive.workspace->created_at,
        .schema_version = archive.workspace->schema_version,
    });
    if (save_result.error && save_result.error->code != RepositoryErrorCode::kUnsupported) {
      spdlog::warn("Failed to restore workspace root record: {}", save_result.error->message);
    }
  }

  for (auto& document_dto : archive.documents) {
    if (!workspace_id) {
      workspace_id = document_dto.workspace_id;
    }
    auto save_result = repository.SaveDocument(FromDto(std::move(document_dto)));
    if (save_result.error) {
      spdlog::warn("Failed to restore a document from workspace archive: {}",
                   save_result.error->message);
    }
  }

  for (auto& conflict_dto : archive.conflicts) {
    auto save_result = repository.SaveConflict(FromDto(std::move(conflict_dto)));
    if (save_result.error) {
      spdlog::warn("Failed to restore a conflict from workspace archive: {}",
                   save_result.error->message);
    }
  }

  if (!workspace_id) {
    return ImportWorkspaceResult{
        .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                           source_path + " contains no workspace data to restore.")};
  }
  return ImportWorkspaceResult{.workspace_id = workspace_id};
}

}  // namespace cppwiki::storage
