#include "storage/file_document_repository.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>
#include <string>
#include <utility>
#include <vector>

namespace cppwiki::storage {
namespace file_repository {

auto MakeError(RepositoryErrorCode code, std::string message) -> RepositoryError {
  return RepositoryError{
      .code = code,
      .message = std::move(message),
  };
}

auto MakePageFilePath(const std::filesystem::path& storage_dir, std::string_view page_id)
    -> std::filesystem::path {
  // Sanitize page_id for filesystem usage (basic version)
  std::string sanitized;
  for (char c : page_id) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
      sanitized += c;
    } else {
      sanitized += '_';
    }
  }
  return storage_dir / "pages" / (sanitized + ".json");
}

auto MakeConflictFilePath(const std::filesystem::path& storage_dir, std::string_view conflict_id)
    -> std::filesystem::path {
  std::string sanitized;
  for (char c : conflict_id) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
      sanitized += c;
    } else {
      sanitized += '_';
    }
  }
  return storage_dir / "conflicts" / (sanitized + ".json");
}

// Issue #166: flat like MakePageFilePath/MakeConflictFilePath above -- one file per revision,
// keyed by the revision's own id (not nested under its document_id), so DeleteDocumentRevision()
// only needs a revision id, matching every other single-id delete in this repository.
auto MakeRevisionFilePath(const std::filesystem::path& storage_dir, std::string_view revision_id)
    -> std::filesystem::path {
  std::string sanitized;
  for (char c : revision_id) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
      sanitized += c;
    } else {
      sanitized += '_';
    }
  }
  return storage_dir / "revisions" / (sanitized + ".json");
}

auto MakeKnowledgeFilePath(const std::filesystem::path& storage_dir, std::string_view directory,
                           std::string_view record_id) -> std::filesystem::path {
  std::string sanitized;
  for (char character : record_id) {
    if (std::isalnum(static_cast<unsigned char>(character)) || character == '-' ||
        character == '_') {
      sanitized += character;
    } else {
      sanitized += '_';
    }
  }
  return storage_dir / directory / (sanitized + ".json");
}

auto MakeAttachmentMetadataFilePath(const std::filesystem::path& storage_dir,
                                    std::string_view attachment_id) -> std::filesystem::path {
  return storage_dir / "attachments" / (std::string(attachment_id) + ".json");
}

auto MakeAttachmentBlobFilePath(const std::filesystem::path& storage_dir,
                                std::string_view attachment_id) -> std::filesystem::path {
  return storage_dir / "attachments" / (std::string(attachment_id) + ".blob");
}

auto MakeBackupPath(const std::filesystem::path& page_path) -> std::filesystem::path {
  return page_path.string() + ".backup";
}

auto MakeTempPath(const std::filesystem::path& page_path) -> std::filesystem::path {
  return page_path.string() + ".tmp";
}

auto ReadFileToString(const std::filesystem::path& path) -> std::optional<std::string> {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return std::nullopt;
  }

  const auto size = file.tellg();
  if (size < 0) {
    return std::nullopt;
  }

  file.seekg(0, std::ios::beg);
  std::string content(static_cast<std::size_t>(size), '\0');
  if (!file.read(content.data(), size)) {
    return std::nullopt;
  }

  return content;
}

auto WriteFileAtomically(const std::filesystem::path& target_path, std::string_view content)
    -> bool {
  try {
    // Ensure parent directory exists
    std::filesystem::create_directories(target_path.parent_path());

    const auto temp_path = MakeTempPath(target_path);
    const auto backup_path = MakeBackupPath(target_path);

    // Write to temp file
    {
      std::ofstream temp_file(temp_path, std::ios::binary);
      if (!temp_file.is_open()) {
        return false;
      }
      temp_file.write(content.data(), static_cast<std::streamsize>(content.size()));
      if (!temp_file.good()) {
        std::filesystem::remove(temp_path);
        return false;
      }
    }

    // If target exists, create backup first
    if (std::filesystem::exists(target_path)) {
      std::filesystem::rename(target_path, backup_path);
    }

    // Atomic rename temp to target
    std::filesystem::rename(temp_path, target_path);

    // Success - remove backup
    if (std::filesystem::exists(backup_path)) {
      std::filesystem::remove(backup_path);
    }

    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to write file atomically: {}", e.what());
    return false;
  }
}

auto RestoreFromBackup(const std::filesystem::path& target_path) -> bool {
  const auto backup_path = MakeBackupPath(target_path);
  if (!std::filesystem::exists(backup_path)) {
    return false;
  }

  try {
    std::filesystem::rename(backup_path, target_path);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to restore from backup: {}", e.what());
    return false;
  }
}

// Reconciles a WriteFileAtomically() call for `target_path` that may have been interrupted (crash,
// kill -9, power loss) partway through its rename steps: temp write -> rename target to `.backup`
// -> rename temp into place -> remove `.backup`. Exactly which files exist tells us which step the
// crash happened after:
//   - target missing, `.tmp` present: crashed after the backup rename but before the final rename.
//     The temp file is guaranteed fully written (WriteFileAtomically only reaches that rename after
//     the temp ofstream reports good()), so finish the swap forward rather than falling back to the
//     older `.backup` and silently losing the edit that was being saved.
//   - target missing, only `.backup` present: the temp file is gone (or never existed for this
//     reconciliation), so the backup is the only recoverable content -- restore it.
//   - target present: the write itself already completed (or never started); any leftover
//     `.tmp`/`.backup` sibling is just cleanup from a crash between the final rename and
//     WriteFileAtomically's own post-write cleanup, not something to act on.
auto ReconcileInterruptedWrite(const std::filesystem::path& target_path) -> void {
  const auto temp_path = MakeTempPath(target_path);
  const auto backup_path = MakeBackupPath(target_path);
  const auto target_exists = std::filesystem::exists(target_path);
  const auto temp_exists = std::filesystem::exists(temp_path);
  const auto backup_exists = std::filesystem::exists(backup_path);

  if (!temp_exists && !backup_exists) {
    return;
  }

  try {
    if (!target_exists && temp_exists) {
      std::filesystem::rename(temp_path, target_path);
      spdlog::warn("Recovered an interrupted write by completing it: {}", target_path.string());
      if (backup_exists) {
        std::filesystem::remove(backup_path);
      }
      return;
    }

    if (!target_exists && backup_exists) {
      std::filesystem::rename(backup_path, target_path);
      spdlog::warn("Recovered an interrupted write by restoring its backup: {}",
                   target_path.string());
      return;
    }

    if (temp_exists) {
      std::filesystem::remove(temp_path);
    }
    if (backup_exists) {
      std::filesystem::remove(backup_path);
    }
  } catch (const std::exception& e) {
    spdlog::error("Failed to reconcile interrupted write for {}: {}", target_path.string(),
                  e.what());
  }
}

// Scans `directory` for `.backup`/`.tmp` files left by an interrupted WriteFileAtomically() call
// and recovers each one via ReconcileInterruptedWrite(). Safe to call on a directory that doesn't
// exist yet or holds no such files. Collects target paths before reconciling any of them, rather
// than mutating files while std::filesystem::directory_iterator is still walking the directory.
auto SweepInterruptedWrites(const std::filesystem::path& directory) -> void {
  if (!std::filesystem::exists(directory)) {
    return;
  }

  std::vector<std::filesystem::path> target_paths;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto& path = entry.path();
    if (path.extension() == ".backup" || path.extension() == ".tmp") {
      target_paths.push_back(path.parent_path() / path.stem());
    }
  }

  // The same target can be queued twice (once via its `.backup` entry, once via its `.tmp`
  // entry); ReconcileInterruptedWrite() is idempotent (a second call sees neither sibling left
  // and returns immediately), so no dedup pass is needed.
  for (const auto& target_path : target_paths) {
    ReconcileInterruptedWrite(target_path);
  }
}

struct FileDocumentRecordDto {
  std::string id;
  std::int32_t schema_version{};
  // Optional (not a plain std::string) so records written before DocumentKind existed still
  // parse: a missing/absent field reads as std::nullopt, and FromDto() below maps that (and
  // any unrecognized key) to DocumentKind::kWikiPage via DocumentKindFromKey().
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
  // Issue #165: absent means the record predates trash support (or is simply live) -- FromDto()
  // maps that to std::nullopt, matching PageMetadata::trashed_at's "not trashed" state.
  std::optional<std::string> trashed_at;
  std::string raw_snapshot_json;
};

struct FileConflictRecordDto {
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

struct FileDocumentRevisionDto {
  std::string id;
  std::string document_id;
  std::string workspace_id;
  std::string raw_snapshot_json;
  std::string title;
  std::string saved_at;
};

struct FileAuditMetadataDto {
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
};

struct FilePropertyDefinitionDto {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::optional<std::string> group_name;
  std::int32_t value_kind{};
  std::vector<std::string> options;
  std::int32_t state{};
  FileAuditMetadataDto audit;
};

struct FilePagePropertyValueDto {
  std::string id;
  std::string workspace_id;
  std::string page_id;
  std::string property_definition_id;
  std::vector<std::string> values;
  FileAuditMetadataDto audit;
};

struct FileRelationTypeDto {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::optional<std::string> inverse_name;
  std::int32_t direction{};
  std::int32_t state{};
  FileAuditMetadataDto audit;
};

struct FilePageRelationDto {
  std::string id;
  std::string workspace_id;
  std::string relation_type_id;
  std::string source_page_id;
  std::string target_page_id;
  FileAuditMetadataDto audit;
};

struct FileRepositoryArtifactDto {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::string remote_url;
  std::string default_branch;
  std::int32_t state{};
  FileAuditMetadataDto audit;
};

struct FileAgentRunDto {
  std::string id;
  std::string workspace_id;
  std::string task_id;
  std::string context_pack_ref;
  std::string runtime_id;
  std::int32_t status{};
  std::string started_at;
  std::optional<std::string> completed_at;
  FileAuditMetadataDto audit;
};

struct FileResultReferenceDto {
  std::string id;
  std::string workspace_id;
  std::string agent_run_id;
  std::int32_t kind{};
  std::string locator;
  std::optional<std::string> summary;
  std::string created_at;
  std::string created_by;
};

auto ToDto(const knowledge::AuditMetadata& audit) -> FileAuditMetadataDto {
  return {.created_at = audit.created_at,
          .updated_at = audit.updated_at,
          .created_by = audit.created_by,
          .updated_by = audit.updated_by};
}

auto FromDto(FileAuditMetadataDto dto) -> knowledge::AuditMetadata {
  return {.created_at = std::move(dto.created_at),
          .updated_at = std::move(dto.updated_at),
          .created_by = std::move(dto.created_by),
          .updated_by = std::move(dto.updated_by)};
}

auto ToDto(const knowledge::PropertyDefinition& definition) -> FilePropertyDefinitionDto {
  return {.id = definition.id,
          .workspace_id = definition.workspace_id,
          .name = definition.name,
          .group_name = definition.group_name,
          .value_kind = static_cast<std::int32_t>(definition.value_kind),
          .options = definition.options,
          .state = static_cast<std::int32_t>(definition.state),
          .audit = ToDto(definition.audit)};
}

auto FromDto(FilePropertyDefinitionDto dto) -> knowledge::PropertyDefinition {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .name = std::move(dto.name),
          .group_name = std::move(dto.group_name),
          .value_kind = static_cast<knowledge::PropertyValueKind>(dto.value_kind),
          .options = std::move(dto.options),
          .state = static_cast<knowledge::RecordState>(dto.state),
          .audit = FromDto(std::move(dto.audit))};
}

auto ToDto(const knowledge::PagePropertyValue& value) -> FilePagePropertyValueDto {
  return {.id = value.id,
          .workspace_id = value.workspace_id,
          .page_id = value.page_id,
          .property_definition_id = value.property_definition_id,
          .values = value.values,
          .audit = ToDto(value.audit)};
}

auto FromDto(FilePagePropertyValueDto dto) -> knowledge::PagePropertyValue {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .page_id = std::move(dto.page_id),
          .property_definition_id = std::move(dto.property_definition_id),
          .values = std::move(dto.values),
          .audit = FromDto(std::move(dto.audit))};
}

auto ToDto(const knowledge::RelationType& type) -> FileRelationTypeDto {
  return {.id = type.id,
          .workspace_id = type.workspace_id,
          .name = type.name,
          .inverse_name = type.inverse_name,
          .direction = static_cast<std::int32_t>(type.direction),
          .state = static_cast<std::int32_t>(type.state),
          .audit = ToDto(type.audit)};
}

auto FromDto(FileRelationTypeDto dto) -> knowledge::RelationType {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .name = std::move(dto.name),
          .inverse_name = std::move(dto.inverse_name),
          .direction = static_cast<knowledge::RelationDirection>(dto.direction),
          .state = static_cast<knowledge::RecordState>(dto.state),
          .audit = FromDto(std::move(dto.audit))};
}

auto ToDto(const knowledge::PageRelation& relation) -> FilePageRelationDto {
  return {.id = relation.id,
          .workspace_id = relation.workspace_id,
          .relation_type_id = relation.relation_type_id,
          .source_page_id = relation.source_page_id,
          .target_page_id = relation.target_page_id,
          .audit = ToDto(relation.audit)};
}

auto FromDto(FilePageRelationDto dto) -> knowledge::PageRelation {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .relation_type_id = std::move(dto.relation_type_id),
          .source_page_id = std::move(dto.source_page_id),
          .target_page_id = std::move(dto.target_page_id),
          .audit = FromDto(std::move(dto.audit))};
}

auto ToDto(const knowledge::RepositoryArtifact& artifact) -> FileRepositoryArtifactDto {
  return {.id = artifact.id,
          .workspace_id = artifact.workspace_id,
          .name = artifact.name,
          .remote_url = artifact.remote_url,
          .default_branch = artifact.default_branch,
          .state = static_cast<std::int32_t>(artifact.state),
          .audit = ToDto(artifact.audit)};
}

auto FromDto(FileRepositoryArtifactDto dto) -> knowledge::RepositoryArtifact {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .name = std::move(dto.name),
          .remote_url = std::move(dto.remote_url),
          .default_branch = std::move(dto.default_branch),
          .state = static_cast<knowledge::RecordState>(dto.state),
          .audit = FromDto(std::move(dto.audit))};
}

auto ToDto(const knowledge::AgentRun& run) -> FileAgentRunDto {
  return {.id = run.id,
          .workspace_id = run.workspace_id,
          .task_id = run.task_id,
          .context_pack_ref = run.context_pack_ref,
          .runtime_id = run.runtime_id,
          .status = static_cast<std::int32_t>(run.status),
          .started_at = run.started_at,
          .completed_at = run.completed_at,
          .audit = ToDto(run.audit)};
}

auto FromDto(FileAgentRunDto dto) -> knowledge::AgentRun {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .task_id = std::move(dto.task_id),
          .context_pack_ref = std::move(dto.context_pack_ref),
          .runtime_id = std::move(dto.runtime_id),
          .status = static_cast<knowledge::AgentRunStatus>(dto.status),
          .started_at = std::move(dto.started_at),
          .completed_at = std::move(dto.completed_at),
          .audit = FromDto(std::move(dto.audit))};
}

auto ToDto(const knowledge::ResultReference& reference) -> FileResultReferenceDto {
  return {.id = reference.id,
          .workspace_id = reference.workspace_id,
          .agent_run_id = reference.agent_run_id,
          .kind = static_cast<std::int32_t>(reference.kind),
          .locator = reference.locator,
          .summary = reference.summary,
          .created_at = reference.created_at,
          .created_by = reference.created_by};
}

auto FromDto(FileResultReferenceDto dto) -> knowledge::ResultReference {
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .agent_run_id = std::move(dto.agent_run_id),
          .kind = static_cast<knowledge::ResultReferenceKind>(dto.kind),
          .locator = std::move(dto.locator),
          .summary = std::move(dto.summary),
          .created_at = std::move(dto.created_at),
          .created_by = std::move(dto.created_by)};
}

struct FileContextPackItemDto {
  std::string id;
  std::string relation_id;
  std::int32_t artifact_kind{};
  std::string artifact_id;
  bool included{};
};

struct FileContextPackDto {
  std::string id;
  std::string workspace_id;
  std::string task_id;
  std::string task_intent;
  std::vector<FileContextPackItemDto> items;
  std::optional<std::string> repository_guidance;
  std::int32_t state{};
  FileAuditMetadataDto audit;
};

auto ToDto(const knowledge::ContextPackItem& item) -> FileContextPackItemDto {
  return {.id = item.id,
          .relation_id = item.relation_id,
          .artifact_kind = static_cast<std::int32_t>(item.artifact_kind),
          .artifact_id = item.artifact_id,
          .included = item.included};
}

auto FromDto(FileContextPackItemDto dto) -> knowledge::ContextPackItem {
  return {.id = std::move(dto.id),
          .relation_id = std::move(dto.relation_id),
          .artifact_kind = static_cast<knowledge::ArtifactKind>(dto.artifact_kind),
          .artifact_id = std::move(dto.artifact_id),
          .included = dto.included};
}

auto ToDto(const knowledge::ContextPack& pack) -> FileContextPackDto {
  std::vector<FileContextPackItemDto> items;
  items.reserve(pack.items.size());
  for (const auto& item : pack.items) items.push_back(ToDto(item));
  return {.id = pack.id,
          .workspace_id = pack.workspace_id,
          .task_id = pack.task_id,
          .task_intent = pack.task_intent,
          .items = std::move(items),
          .repository_guidance = pack.repository_guidance,
          .state = static_cast<std::int32_t>(pack.state),
          .audit = ToDto(pack.audit)};
}

auto FromDto(FileContextPackDto dto) -> knowledge::ContextPack {
  std::vector<knowledge::ContextPackItem> items;
  items.reserve(dto.items.size());
  for (auto& item : dto.items) items.push_back(FromDto(std::move(item)));
  return {.id = std::move(dto.id),
          .workspace_id = std::move(dto.workspace_id),
          .task_id = std::move(dto.task_id),
          .task_intent = std::move(dto.task_intent),
          .items = std::move(items),
          .repository_guidance = std::move(dto.repository_guidance),
          .state = static_cast<knowledge::ContextPackState>(dto.state),
          .audit = FromDto(std::move(dto.audit))};
}

struct FileAttachmentDto {
  std::string id;
  std::string workspace_id;
  std::string filename;
  std::string mime_type;
  std::uint64_t size_bytes{};
  std::string sha256;
  std::string created_at;
  std::string created_by;
};

auto ToDto(const AttachmentMetadata& attachment) -> FileAttachmentDto {
  return FileAttachmentDto{
      .id = attachment.id,
      .workspace_id = attachment.workspace_id,
      .filename = attachment.filename,
      .mime_type = attachment.mime_type,
      .size_bytes = attachment.size_bytes,
      .sha256 = attachment.sha256,
      .created_at = attachment.created_at,
      .created_by = attachment.created_by,
  };
}

auto FromDto(FileAttachmentDto dto) -> AttachmentMetadata {
  return AttachmentMetadata{
      .id = std::move(dto.id),
      .workspace_id = std::move(dto.workspace_id),
      .filename = std::move(dto.filename),
      .mime_type = std::move(dto.mime_type),
      .size_bytes = dto.size_bytes,
      .sha256 = std::move(dto.sha256),
      .created_at = std::move(dto.created_at),
      .created_by = std::move(dto.created_by),
  };
}

auto ToDto(const DocumentRecord& document) -> FileDocumentRecordDto {
  return FileDocumentRecordDto{
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
      .trashed_at = document.metadata.trashed_at,
      .raw_snapshot_json = document.raw_snapshot_json,
  };
}

auto FromDto(FileDocumentRecordDto dto) -> DocumentRecord {
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
              .trashed_at = std::move(dto.trashed_at),
          },
      .snapshot = document::BlockNoteDocumentSnapshot{},
      .raw_snapshot_json = std::move(dto.raw_snapshot_json),
  };
}

auto ToDto(const DocumentConflictRecord& conflict) -> FileConflictRecordDto {
  return FileConflictRecordDto{
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

auto FromDto(FileConflictRecordDto dto) -> DocumentConflictRecord {
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

auto ToDto(const DocumentRevisionRecord& revision) -> FileDocumentRevisionDto {
  return FileDocumentRevisionDto{
      .id = revision.id,
      .document_id = revision.document_id,
      .workspace_id = revision.workspace_id,
      .raw_snapshot_json = revision.raw_snapshot_json,
      .title = revision.title,
      .saved_at = revision.saved_at,
  };
}

auto FromDto(FileDocumentRevisionDto dto) -> DocumentRevisionRecord {
  return DocumentRevisionRecord{
      .id = std::move(dto.id),
      .document_id = std::move(dto.document_id),
      .workspace_id = std::move(dto.workspace_id),
      .raw_snapshot_json = std::move(dto.raw_snapshot_json),
      .title = std::move(dto.title),
      .saved_at = std::move(dto.saved_at),
  };
}

auto IsPendingConflict(const DocumentConflictRecord& conflict) -> bool {
  return conflict.resolution_state == "pending";
}

auto IsValidResolutionState(std::string_view resolution_state) -> bool {
  return resolution_state == "pending" || resolution_state == "resolved" ||
         resolution_state == "dismissed";
}

}  // namespace file_repository

using namespace file_repository;

class FileDocumentRepository::Impl {
 public:
  explicit Impl(FileDocumentRepositoryOptions options) : options_(std::move(options)) {
    // Startup integrity sweep (issue #162): recover any page/conflict write left interrupted by
    // a crash before this repository was last closed -- see SweepInterruptedWrites().
    SweepInterruptedWrites(options_.storage_directory / "pages");
    SweepInterruptedWrites(options_.storage_directory / "conflicts");
    SweepInterruptedWrites(options_.storage_directory / "attachments");
    SweepInterruptedWrites(options_.storage_directory / "property-definitions");
    SweepInterruptedWrites(options_.storage_directory / "page-property-values");
    SweepInterruptedWrites(options_.storage_directory / "relation-types");
    SweepInterruptedWrites(options_.storage_directory / "page-relations");
  }

  [[nodiscard]] auto SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
    try {
      const auto page_path = MakePageFilePath(options_.storage_directory, document.metadata.id);

      // Serialize document to JSON
      const auto json_content = SerializeDocument(document);

      if (!WriteFileAtomically(page_path, json_content)) {
        // Try to restore from backup if write failed
        RestoreFromBackup(page_path);
        return SaveDocumentResult{
            .error = MakeError(RepositoryErrorCode::kWriteFailed, "Failed to write document file"),
        };
      }

      return SaveDocumentResult{};
    } catch (const std::exception& e) {
      return SaveDocumentResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto DeleteDocument(std::string_view page_id) -> DeleteDocumentResult {
    try {
      const auto document = LoadDocument(page_id);
      if (document.document) {
        if (const auto cleanup =
                DeleteKnowledgeForPage(document.document->metadata.workspace_id, page_id);
            cleanup.error) {
          return {.error = cleanup.error};
        }
      } else if (document.error && document.error->code != RepositoryErrorCode::kReadFailed) {
        return {.error = document.error};
      }
      auto conflicts = ListConflicts();
      if (conflicts.error) {
        return DeleteDocumentResult{.error = std::move(conflicts.error)};
      }
      for (const auto& conflict : conflicts.conflicts) {
        if (conflict.document_id == page_id) {
          if (auto delete_conflict = DeleteConflict(conflict.id); delete_conflict.error) {
            return DeleteDocumentResult{.error = std::move(delete_conflict.error)};
          }
        }
      }

      // Issue #166: a permanently-deleted document's revision history is meaningless (nothing
      // left to restore it onto) and would otherwise leak as orphaned files forever.
      auto revisions = ListDocumentRevisions(page_id);
      if (revisions.error) {
        return DeleteDocumentResult{.error = std::move(revisions.error)};
      }
      for (const auto& revision : revisions.revisions) {
        if (auto delete_revision = DeleteDocumentRevision(revision.id); delete_revision.error) {
          return DeleteDocumentResult{.error = std::move(delete_revision.error)};
        }
      }

      const auto page_path = MakePageFilePath(options_.storage_directory, page_id);
      if (!std::filesystem::exists(page_path)) {
        return DeleteDocumentResult{};
      }
      std::filesystem::remove(page_path);
      return DeleteDocumentResult{};
    } catch (const std::exception& e) {
      return DeleteDocumentResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto LoadDocument(std::string_view page_id) -> LoadDocumentResult {
    try {
      const auto page_path = MakePageFilePath(options_.storage_directory, page_id);

      if (!std::filesystem::exists(page_path)) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Document not found"),
        };
      }

      const auto content = ReadFileToString(page_path);
      if (!content) {
        return LoadDocumentResult{
            .document = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Failed to read document file"),
        };
      }

      return DeserializeDocument(*content, page_id);
    } catch (const std::exception& e) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto ListDocuments() -> ListDocumentsResult {
    try {
      const auto pages_directory = options_.storage_directory / "pages";
      if (!std::filesystem::exists(pages_directory)) {
        return ListDocumentsResult{};
      }

      std::vector<DocumentSummary> documents;
      for (const auto& entry : std::filesystem::directory_iterator(pages_directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
          continue;
        }

        const auto page_id = entry.path().stem().string();
        auto loaded = LoadDocument(page_id);
        if (!loaded.document) {
          // Issue #167: a page file exists but couldn't be loaded (corrupted/unparseable JSON --
          // disk-level corruption, a manual edit, or any other write path this repository
          // doesn't itself protect against). Report a placeholder summary instead of silently
          // dropping it, so the document doesn't just vanish from the workspace tree with no
          // trace: DeleteDocument still resolves it by id, and LoadDocument still returns the
          // underlying RepositoryError if something tries to open it.
          spdlog::warn("Skipping corrupted document {}: {}", page_id,
                       loaded.error ? loaded.error->message : "unknown error");
          documents.push_back(DocumentSummary{
              .id = page_id,
              .title = "(Corrupted document)",
              .is_corrupted = true,
              .load_error = loaded.error ? std::make_optional(loaded.error->message) : std::nullopt,
          });
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
    } catch (const std::exception& e) {
      return ListDocumentsResult{
          .documents = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto SavePropertyDefinition(const knowledge::PropertyDefinition& definition)
      -> SaveKnowledgeRecordResult {
    if (const auto validation = knowledge::ValidatePropertyDefinition(definition); validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path =
        MakeKnowledgeFilePath(options_.storage_directory, "property-definitions", definition.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(definition)))) {
      RestoreFromBackup(path);
      return {.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                 "Failed to write property definition file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeletePropertyDefinition(std::string_view definition_id)
      -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "property-definitions", definition_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListPropertyDefinitions(std::string_view workspace_id)
      -> ListPropertyDefinitionsResult {
    const auto directory = options_.storage_directory / "property-definitions";
    if (!std::filesystem::exists(directory)) {
      return {};
    }
    try {
      std::vector<knowledge::PropertyDefinition> definitions;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FilePropertyDefinitionDto>(*content);
        if (!parsed) {
          return {.definitions = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse property definition file.")};
        }
        auto definition = FromDto(parsed.value());
        if (definition.id != entry.path().stem().string() ||
            knowledge::ValidatePropertyDefinition(definition)) {
          return {.definitions = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Property definition file contains an invalid record.")};
        }
        if (definition.workspace_id == workspace_id)
          definitions.push_back(std::move(definition));
      }
      std::ranges::sort(definitions, [](const auto& left, const auto& right) {
        return left.name == right.name ? left.id < right.id : left.name < right.name;
      });
      return {.definitions = std::move(definitions), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.definitions = {},
              .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SavePagePropertyValue(const knowledge::PagePropertyValue& value)
      -> SaveKnowledgeRecordResult {
    const auto definitions = ListPropertyDefinitions(value.workspace_id);
    if (definitions.error)
      return {.error = definitions.error};
    const auto definition = std::ranges::find_if(
        definitions.definitions,
        [&value](const auto& item) { return item.id == value.property_definition_id; });
    if (definition == definitions.definitions.end()) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                 "Page property value refers to an unknown property definition.")};
    }
    if (const auto validation = knowledge::ValidatePagePropertyValue(value, *definition);
        validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path =
        MakeKnowledgeFilePath(options_.storage_directory, "page-property-values", value.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(value)))) {
      RestoreFromBackup(path);
      return {.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                 "Failed to write page property value file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeletePagePropertyValue(std::string_view value_id)
      -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "page-property-values", value_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListPagePropertyValues(std::string_view workspace_id, std::string_view page_id)
      -> ListPagePropertyValuesResult {
    const auto directory = options_.storage_directory / "page-property-values";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::PagePropertyValue> values;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FilePagePropertyValueDto>(*content);
        if (!parsed)
          return {.values = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse page property value file.")};
        auto value = FromDto(parsed.value());
        if (value.id != entry.path().stem().string())
          return {.values = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Page property value file id does not match its name.")};
        if (value.workspace_id == workspace_id && value.page_id == page_id) {
          values.push_back(std::move(value));
        }
      }
      std::ranges::sort(values,
                        [](const auto& left, const auto& right) { return left.id < right.id; });
      return {.values = std::move(values), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.values = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SaveRelationType(const knowledge::RelationType& relation_type)
      -> SaveKnowledgeRecordResult {
    if (const auto validation = knowledge::ValidateRelationType(relation_type); validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path =
        MakeKnowledgeFilePath(options_.storage_directory, "relation-types", relation_type.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(relation_type)))) {
      RestoreFromBackup(path);
      return {.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                 "Failed to write relation type file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeleteRelationType(std::string_view relation_type_id)
      -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "relation-types", relation_type_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListRelationTypes(std::string_view workspace_id) -> ListRelationTypesResult {
    const auto directory = options_.storage_directory / "relation-types";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::RelationType> relation_types;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FileRelationTypeDto>(*content);
        if (!parsed)
          return {.relation_types = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse relation type file.")};
        auto relation_type = FromDto(parsed.value());
        if (relation_type.id != entry.path().stem().string() ||
            knowledge::ValidateRelationType(relation_type)) {
          return {.relation_types = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Relation type file contains an invalid record.")};
        }
        if (relation_type.workspace_id == workspace_id)
          relation_types.push_back(std::move(relation_type));
      }
      std::ranges::sort(relation_types, [](const auto& left, const auto& right) {
        return left.name == right.name ? left.id < right.id : left.name < right.name;
      });
      return {.relation_types = std::move(relation_types), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.relation_types = {},
              .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SavePageRelation(const knowledge::PageRelation& input)
      -> SaveKnowledgeRecordResult {
    const auto types = ListRelationTypes(input.workspace_id);
    if (types.error)
      return {.error = types.error};
    const auto type = std::ranges::find_if(types.relation_types, [&input](const auto& item) {
      return item.id == input.relation_type_id;
    });
    if (type == types.relation_types.end()) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                 "Page relation refers to an unknown relation type.")};
    }
    auto relation = input;
    if (const auto validation = knowledge::NormalizeAndValidatePageRelation(&relation, *type);
        validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path =
        MakeKnowledgeFilePath(options_.storage_directory, "page-relations", relation.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(relation)))) {
      RestoreFromBackup(path);
      return {.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                 "Failed to write page relation file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeletePageRelation(std::string_view relation_id)
      -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "page-relations", relation_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListPageRelations(std::string_view workspace_id, std::string_view page_id)
      -> ListPageRelationsResult {
    const auto directory = options_.storage_directory / "page-relations";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::PageRelation> relations;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FilePageRelationDto>(*content);
        if (!parsed)
          return {.relations = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse page relation file.")};
        auto relation = FromDto(parsed.value());
        if (relation.id != entry.path().stem().string())
          return {.relations = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Page relation file id does not match its name.")};
        if (relation.workspace_id == workspace_id &&
            (relation.source_page_id == page_id || relation.target_page_id == page_id)) {
          relations.push_back(std::move(relation));
        }
      }
      std::ranges::sort(relations,
                        [](const auto& left, const auto& right) { return left.id < right.id; });
      return {.relations = std::move(relations), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.relations = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto DeleteKnowledgeForPage(std::string_view workspace_id, std::string_view page_id)
      -> DeleteKnowledgeForPageResult {
    const auto values = ListPagePropertyValues(workspace_id, page_id);
    if (values.error)
      return {.error = values.error};
    for (const auto& value : values.values) {
      if (const auto deleted = DeletePagePropertyValue(value.id); deleted.error) {
        return {.error = deleted.error};
      }
    }
    const auto relations = ListPageRelations(workspace_id, page_id);
    if (relations.error)
      return {.error = relations.error};
    for (const auto& relation : relations.relations) {
      if (const auto deleted = DeletePageRelation(relation.id); deleted.error) {
        return {.error = deleted.error};
      }
    }
    return {};
  }

  [[nodiscard]] auto SaveRepositoryArtifact(const knowledge::RepositoryArtifact& artifact)
      -> SaveKnowledgeRecordResult {
    if (const auto validation = knowledge::ValidateRepositoryArtifact(artifact); validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path =
        MakeKnowledgeFilePath(options_.storage_directory, "repository-artifacts", artifact.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(artifact)))) {
      RestoreFromBackup(path);
      return {.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                 "Failed to write repository artifact file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeleteRepositoryArtifact(std::string_view artifact_id)
      -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "repository-artifacts", artifact_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListRepositoryArtifacts(std::string_view workspace_id)
      -> ListRepositoryArtifactsResult {
    const auto directory = options_.storage_directory / "repository-artifacts";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::RepositoryArtifact> artifacts;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FileRepositoryArtifactDto>(*content);
        if (!parsed)
          return {.artifacts = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse repository artifact file.")};
        auto artifact = FromDto(parsed.value());
        if (artifact.id != entry.path().stem().string() ||
            knowledge::ValidateRepositoryArtifact(artifact)) {
          return {.artifacts = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Repository artifact file contains an invalid record.")};
        }
        if (artifact.workspace_id == workspace_id)
          artifacts.push_back(std::move(artifact));
      }
      std::ranges::sort(artifacts,
                        [](const auto& left, const auto& right) { return left.id < right.id; });
      return {.artifacts = std::move(artifacts), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.artifacts = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SaveAgentRun(const knowledge::AgentRun& run) -> SaveKnowledgeRecordResult {
    if (const auto validation = knowledge::ValidateAgentRun(run); validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path = MakeKnowledgeFilePath(options_.storage_directory, "agent-runs", run.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(run)))) {
      RestoreFromBackup(path);
      return {.error =
                  MakeError(RepositoryErrorCode::kWriteFailed, "Failed to write agent run file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeleteAgentRun(std::string_view run_id) -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(MakeKnowledgeFilePath(options_.storage_directory, "agent-runs", run_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListAgentRuns(std::string_view workspace_id) -> ListAgentRunsResult {
    const auto directory = options_.storage_directory / "agent-runs";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::AgentRun> runs;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FileAgentRunDto>(*content);
        if (!parsed)
          return {.runs = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse agent run file.")};
        auto run = FromDto(parsed.value());
        if (run.id != entry.path().stem().string() || knowledge::ValidateAgentRun(run)) {
          return {.runs = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Agent run file contains an invalid record.")};
        }
        if (run.workspace_id == workspace_id)
          runs.push_back(std::move(run));
      }
      std::ranges::sort(runs,
                        [](const auto& left, const auto& right) { return left.id < right.id; });
      return {.runs = std::move(runs), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.runs = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SaveResultReference(const knowledge::ResultReference& reference)
      -> SaveKnowledgeRecordResult {
    const auto runs = ListAgentRuns(reference.workspace_id);
    if (runs.error)
      return {.error = runs.error};
    const auto run = std::ranges::find_if(
        runs.runs, [&reference](const auto& item) { return item.id == reference.agent_run_id; });
    if (run == runs.runs.end()) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                 "Result reference refers to an unknown agent run.")};
    }
    if (const auto validation = knowledge::ValidateResultReference(reference, *run); validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path =
        MakeKnowledgeFilePath(options_.storage_directory, "result-references", reference.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(reference)))) {
      RestoreFromBackup(path);
      return {.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                 "Failed to write result reference file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeleteResultReference(std::string_view reference_id)
      -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "result-references", reference_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListResultReferences(std::string_view workspace_id,
                                          std::string_view agent_run_id)
      -> ListResultReferencesResult {
    const auto directory = options_.storage_directory / "result-references";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::ResultReference> references;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FileResultReferenceDto>(*content);
        if (!parsed)
          return {.references = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse result reference file.")};
        auto reference = FromDto(parsed.value());
        if (reference.id != entry.path().stem().string())
          return {.references = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Result reference file id does not match its name.")};
        if (reference.workspace_id == workspace_id && reference.agent_run_id == agent_run_id) {
          references.push_back(std::move(reference));
        }
      }
      std::ranges::sort(references,
                        [](const auto& left, const auto& right) { return left.id < right.id; });
      return {.references = std::move(references), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.references = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SaveContextPack(const knowledge::ContextPack& pack)
      -> SaveKnowledgeRecordResult {
    if (const auto validation = knowledge::ValidateContextPack(pack); validation) {
      return {.error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    const auto path = MakeKnowledgeFilePath(options_.storage_directory, "context-packs", pack.id);
    if (!WriteFileAtomically(path, rfl::json::write(ToDto(pack)))) {
      RestoreFromBackup(path);
      return {.error =
                  MakeError(RepositoryErrorCode::kWriteFailed, "Failed to write context pack file.")};
    }
    return {};
  }

  [[nodiscard]] auto DeleteContextPack(std::string_view pack_id) -> DeleteKnowledgeRecordResult {
    try {
      std::filesystem::remove(
          MakeKnowledgeFilePath(options_.storage_directory, "context-packs", pack_id));
      return {};
    } catch (const std::exception& error) {
      return {.error = MakeError(RepositoryErrorCode::kDeleteFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListContextPacks(std::string_view workspace_id, std::string_view task_id)
      -> ListContextPacksResult {
    const auto directory = options_.storage_directory / "context-packs";
    if (!std::filesystem::exists(directory))
      return {};
    try {
      std::vector<knowledge::ContextPack> packs;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
          continue;
        const auto content = ReadFileToString(entry.path());
        if (!content)
          continue;
        const auto parsed = rfl::json::read<FileContextPackDto>(*content);
        if (!parsed)
          return {.packs = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Failed to parse context pack file.")};
        auto pack = FromDto(parsed.value());
        if (pack.id != entry.path().stem().string() || knowledge::ValidateContextPack(pack)) {
          return {.packs = {},
                  .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                     "Context pack file contains an invalid record.")};
        }
        if (pack.workspace_id == workspace_id && pack.task_id == task_id) {
          packs.push_back(std::move(pack));
        }
      }
      std::ranges::sort(packs,
                        [](const auto& left, const auto& right) { return left.id < right.id; });
      return {.packs = std::move(packs), .error = std::nullopt};
    } catch (const std::exception& error) {
      return {.packs = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SaveAttachment(const AttachmentData& attachment) -> SaveAttachmentResult {
    if (const auto validation = ValidateAttachmentMetadata(attachment.metadata); validation) {
      return SaveAttachmentResult{.error =
                                      MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
    }
    if (attachment.metadata.size_bytes != attachment.bytes.size()) {
      return SaveAttachmentResult{
          .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                             "Attachment metadata size does not match its byte payload.")};
    }

    try {
      const auto metadata_path =
          MakeAttachmentMetadataFilePath(options_.storage_directory, attachment.metadata.id);
      const auto blob_path =
          MakeAttachmentBlobFilePath(options_.storage_directory, attachment.metadata.id);
      if (std::filesystem::exists(metadata_path) || std::filesystem::exists(blob_path)) {
        return SaveAttachmentResult{
            .error = MakeError(RepositoryErrorCode::kWriteFailed, "Attachment already exists.")};
      }

      const std::string blob_content(attachment.bytes.begin(), attachment.bytes.end());
      if (!WriteFileAtomically(blob_path, blob_content)) {
        return SaveAttachmentResult{.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                                       "Failed to write attachment bytes.")};
      }
      const auto metadata_content = rfl::json::write(ToDto(attachment.metadata));
      if (!WriteFileAtomically(metadata_path, metadata_content)) {
        std::filesystem::remove(blob_path);
        return SaveAttachmentResult{.error = MakeError(RepositoryErrorCode::kWriteFailed,
                                                       "Failed to write attachment metadata.")};
      }
      return SaveAttachmentResult{};
    } catch (const std::exception& error) {
      return SaveAttachmentResult{.error =
                                      MakeError(RepositoryErrorCode::kWriteFailed, error.what())};
    }
  }

  [[nodiscard]] auto LoadAttachment(std::string_view attachment_id, std::string_view workspace_id)
      -> LoadAttachmentResult {
    try {
      const auto metadata_path =
          MakeAttachmentMetadataFilePath(options_.storage_directory, attachment_id);
      const auto blob_path = MakeAttachmentBlobFilePath(options_.storage_directory, attachment_id);
      const auto metadata_content = ReadFileToString(metadata_path);
      const auto blob_content = ReadFileToString(blob_path);
      if (!metadata_content || !blob_content) {
        return LoadAttachmentResult{
            .attachment = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Attachment not found.")};
      }
      auto parsed = rfl::json::read<FileAttachmentDto>(*metadata_content);
      if (!parsed) {
        return LoadAttachmentResult{.attachment = std::nullopt,
                                    .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                                                       "Attachment metadata is invalid.")};
      }
      auto metadata = FromDto(std::move(parsed.value()));
      if (metadata.id != attachment_id || metadata.workspace_id != workspace_id) {
        return LoadAttachmentResult{
            .attachment = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Attachment not found.")};
      }
      if (const auto validation = ValidateAttachmentMetadata(metadata); validation) {
        return LoadAttachmentResult{
            .attachment = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord, *validation)};
      }
      std::vector<std::uint8_t> bytes(blob_content->begin(), blob_content->end());
      if (metadata.size_bytes != bytes.size()) {
        return LoadAttachmentResult{
            .attachment = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               "Attachment metadata size does not match stored bytes.")};
      }
      return LoadAttachmentResult{
          .attachment = AttachmentData{.metadata = std::move(metadata), .bytes = std::move(bytes)},
          .error = std::nullopt};
    } catch (const std::exception& error) {
      return LoadAttachmentResult{
          .attachment = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto ListAttachments(std::string_view workspace_id) -> ListAttachmentsResult {
    try {
      const auto directory = options_.storage_directory / "attachments";
      if (!std::filesystem::exists(directory)) {
        return ListAttachmentsResult{};
      }
      std::vector<AttachmentMetadata> attachments;
      for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
          continue;
        }
        const auto attachment_id = entry.path().stem().string();
        const auto loaded = LoadAttachment(attachment_id, workspace_id);
        if (loaded.attachment) {
          attachments.push_back(loaded.attachment->metadata);
        }
      }
      return ListAttachmentsResult{.attachments = std::move(attachments), .error = std::nullopt};
    } catch (const std::exception& error) {
      return ListAttachmentsResult{
          .attachments = {}, .error = MakeError(RepositoryErrorCode::kReadFailed, error.what())};
    }
  }

  [[nodiscard]] auto SaveConflict(const DocumentConflictRecord& conflict) -> SaveConflictResult {
    try {
      const auto conflict_path = MakeConflictFilePath(options_.storage_directory, conflict.id);
      const auto json_content = rfl::json::write(ToDto(conflict));

      if (!WriteFileAtomically(conflict_path, json_content)) {
        RestoreFromBackup(conflict_path);
        return SaveConflictResult{
            .error = MakeError(RepositoryErrorCode::kWriteFailed, "Failed to write conflict file"),
        };
      }

      return SaveConflictResult{};
    } catch (const std::exception& e) {
      return SaveConflictResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto DeleteConflict(std::string_view conflict_id) -> DeleteConflictResult {
    try {
      const auto conflict_path = MakeConflictFilePath(options_.storage_directory, conflict_id);
      if (std::filesystem::exists(conflict_path)) {
        std::filesystem::remove(conflict_path);
      }
      return DeleteConflictResult{};
    } catch (const std::exception& e) {
      return DeleteConflictResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto LoadConflict(std::string_view conflict_id) -> LoadConflictResult {
    try {
      const auto conflict_path = MakeConflictFilePath(options_.storage_directory, conflict_id);
      if (!std::filesystem::exists(conflict_path)) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Conflict not found"),
        };
      }

      const auto content = ReadFileToString(conflict_path);
      if (!content) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kReadFailed, "Failed to read conflict file"),
        };
      }

      auto parsed = rfl::json::read<FileConflictRecordDto>(*content);
      if (!parsed) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error =
                MakeError(RepositoryErrorCode::kInvalidRecord,
                          std::string("Failed to parse conflict file: ") + parsed.error().what()),
        };
      }

      auto record = FromDto(std::move(parsed.value()));
      if (record.id != conflict_id) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               "Conflict id does not match file name."),
        };
      }
      if (!IsValidResolutionState(record.resolution_state)) {
        return LoadConflictResult{
            .conflict = std::nullopt,
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               "Conflict resolution state is invalid."),
        };
      }

      return LoadConflictResult{
          .conflict = std::make_optional(std::move(record)),
          .error = std::nullopt,
      };
    } catch (const std::exception& e) {
      return LoadConflictResult{
          .conflict = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kReadFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto ListConflicts() -> ListConflictsResult {
    try {
      const auto conflicts_directory = options_.storage_directory / "conflicts";
      if (!std::filesystem::exists(conflicts_directory)) {
        return ListConflictsResult{};
      }

      std::vector<DocumentConflictRecord> conflicts;
      for (const auto& entry : std::filesystem::directory_iterator(conflicts_directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
          continue;
        }

        const auto conflict_id = entry.path().stem().string();
        auto loaded = LoadConflict(conflict_id);
        if (!loaded.conflict) {
          continue;
        }

        conflicts.push_back(std::move(*loaded.conflict));
      }

      std::ranges::sort(conflicts,
                        [](const DocumentConflictRecord& lhs, const DocumentConflictRecord& rhs) {
                          if (lhs.detected_at != rhs.detected_at) {
                            return lhs.detected_at < rhs.detected_at;
                          }
                          return lhs.id < rhs.id;
                        });

      return ListConflictsResult{
          .conflicts = std::move(conflicts),
          .error = std::nullopt,
      };
    } catch (const std::exception& e) {
      return ListConflictsResult{
          .conflicts = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto SaveDocumentRevision(const DocumentRevisionRecord& revision)
      -> SaveDocumentRevisionResult {
    try {
      const auto revision_path = MakeRevisionFilePath(options_.storage_directory, revision.id);
      const auto json_content = rfl::json::write(ToDto(revision));

      if (!WriteFileAtomically(revision_path, json_content)) {
        RestoreFromBackup(revision_path);
        return SaveDocumentRevisionResult{
            .error = MakeError(RepositoryErrorCode::kWriteFailed, "Failed to write revision file"),
        };
      }

      return SaveDocumentRevisionResult{};
    } catch (const std::exception& e) {
      return SaveDocumentRevisionResult{
          .error = MakeError(RepositoryErrorCode::kWriteFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto ListDocumentRevisions(std::string_view document_id)
      -> ListDocumentRevisionsResult {
    try {
      const auto revisions_directory = options_.storage_directory / "revisions";
      if (!std::filesystem::exists(revisions_directory)) {
        return ListDocumentRevisionsResult{};
      }

      std::vector<DocumentRevisionRecord> revisions;
      for (const auto& entry : std::filesystem::directory_iterator(revisions_directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
          continue;
        }

        const auto content = ReadFileToString(entry.path());
        if (!content) {
          continue;
        }
        auto parsed = rfl::json::read<FileDocumentRevisionDto>(*content);
        if (!parsed) {
          continue;
        }

        auto record = FromDto(std::move(parsed.value()));
        if (record.document_id != document_id) {
          continue;
        }
        revisions.push_back(std::move(record));
      }

      // Newest first.
      std::ranges::sort(revisions,
                        [](const DocumentRevisionRecord& lhs, const DocumentRevisionRecord& rhs) {
                          if (lhs.saved_at != rhs.saved_at) {
                            return lhs.saved_at > rhs.saved_at;
                          }
                          return lhs.id > rhs.id;
                        });

      return ListDocumentRevisionsResult{
          .revisions = std::move(revisions),
          .error = std::nullopt,
      };
    } catch (const std::exception& e) {
      return ListDocumentRevisionsResult{
          .revisions = {},
          .error = MakeError(RepositoryErrorCode::kReadFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto DeleteDocumentRevision(std::string_view revision_id)
      -> DeleteDocumentRevisionResult {
    try {
      const auto revision_path = MakeRevisionFilePath(options_.storage_directory, revision_id);
      if (std::filesystem::exists(revision_path)) {
        std::filesystem::remove(revision_path);
      }
      return DeleteDocumentRevisionResult{};
    } catch (const std::exception& e) {
      return DeleteDocumentRevisionResult{
          .error = MakeError(RepositoryErrorCode::kDeleteFailed, e.what()),
      };
    }
  }

  [[nodiscard]] auto GetSyncStatus() -> SyncStatus {
    auto conflicts = ListConflicts();
    if (conflicts.error) {
      return SyncStatus{
          .state = SyncLifecycleState::kDisabled,
          .status_text = "Sync unsupported",
      };
    }

    const auto pending_conflicts =
        static_cast<std::size_t>(std::ranges::count_if(conflicts.conflicts, IsPendingConflict));
    return SyncStatus{
        .state = SyncLifecycleState::kDisabled,
        .status_text = "Sync unsupported",
        .has_conflicts = pending_conflicts > 0,
        .conflict_count = pending_conflicts,
    };
  }

  [[nodiscard]] auto ResolveConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult {
    return UpdateConflictResolutionState(conflict_id, "resolved");
  }

  [[nodiscard]] auto DismissConflict(std::string_view conflict_id)
      -> UpdateConflictResolutionResult {
    return UpdateConflictResolutionState(conflict_id, "dismissed");
  }

 private:
  [[nodiscard]] auto UpdateConflictResolutionState(std::string_view conflict_id,
                                                   std::string resolution_state)
      -> UpdateConflictResolutionResult {
    auto loaded = LoadConflict(conflict_id);
    if (!loaded.conflict) {
      return UpdateConflictResolutionResult{.error = std::move(loaded.error)};
    }

    loaded.conflict->resolution_state = std::move(resolution_state);
    auto saved = SaveConflict(*loaded.conflict);
    return UpdateConflictResolutionResult{.error = std::move(saved.error)};
  }

  [[nodiscard]] auto SerializeDocument(const DocumentRecord& document) -> std::string {
    return rfl::json::write(ToDto(document));
  }

  [[nodiscard]] auto DeserializeDocument(const std::string& content, std::string_view expected_id)
      -> LoadDocumentResult {
    auto parsed = rfl::json::read<FileDocumentRecordDto>(content);
    if (!parsed) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error =
              MakeError(RepositoryErrorCode::kInvalidRecord,
                        std::string("Failed to parse document file: ") + parsed.error().what()),
      };
    }

    auto record = FromDto(std::move(parsed.value()));
    if (record.metadata.id != expected_id) {
      return LoadDocumentResult{
          .document = std::nullopt,
          .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                             "Document id does not match file name."),
      };
    }

    return LoadDocumentResult{
        .document = std::make_optional(std::move(record)),
        .error = std::nullopt,
    };
  }

  FileDocumentRepositoryOptions options_;
};

FileDocumentRepository::FileDocumentRepository(FileDocumentRepositoryOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}

FileDocumentRepository::~FileDocumentRepository() = default;

auto FileDocumentRepository::SaveDocument(const DocumentRecord& document) -> SaveDocumentResult {
  return impl_->SaveDocument(document);
}

auto FileDocumentRepository::DeleteDocument(std::string_view page_id) -> DeleteDocumentResult {
  return impl_->DeleteDocument(page_id);
}

auto FileDocumentRepository::LoadDocument(std::string_view page_id) -> LoadDocumentResult {
  return impl_->LoadDocument(page_id);
}

auto FileDocumentRepository::ListDocuments() -> ListDocumentsResult {
  return impl_->ListDocuments();
}

auto FileDocumentRepository::SavePropertyDefinition(const knowledge::PropertyDefinition& definition)
    -> SaveKnowledgeRecordResult {
  return impl_->SavePropertyDefinition(definition);
}

auto FileDocumentRepository::DeletePropertyDefinition(std::string_view definition_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeletePropertyDefinition(definition_id);
}

auto FileDocumentRepository::ListPropertyDefinitions(std::string_view workspace_id)
    -> ListPropertyDefinitionsResult {
  return impl_->ListPropertyDefinitions(workspace_id);
}

auto FileDocumentRepository::SavePagePropertyValue(const knowledge::PagePropertyValue& value)
    -> SaveKnowledgeRecordResult {
  return impl_->SavePagePropertyValue(value);
}

auto FileDocumentRepository::DeletePagePropertyValue(std::string_view value_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeletePagePropertyValue(value_id);
}

auto FileDocumentRepository::ListPagePropertyValues(std::string_view workspace_id,
                                                    std::string_view page_id)
    -> ListPagePropertyValuesResult {
  return impl_->ListPagePropertyValues(workspace_id, page_id);
}

auto FileDocumentRepository::SaveRelationType(const knowledge::RelationType& relation_type)
    -> SaveKnowledgeRecordResult {
  return impl_->SaveRelationType(relation_type);
}

auto FileDocumentRepository::DeleteRelationType(std::string_view relation_type_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeleteRelationType(relation_type_id);
}

auto FileDocumentRepository::ListRelationTypes(std::string_view workspace_id)
    -> ListRelationTypesResult {
  return impl_->ListRelationTypes(workspace_id);
}

auto FileDocumentRepository::SavePageRelation(const knowledge::PageRelation& relation)
    -> SaveKnowledgeRecordResult {
  return impl_->SavePageRelation(relation);
}

auto FileDocumentRepository::DeletePageRelation(std::string_view relation_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeletePageRelation(relation_id);
}

auto FileDocumentRepository::ListPageRelations(std::string_view workspace_id,
                                               std::string_view page_id)
    -> ListPageRelationsResult {
  return impl_->ListPageRelations(workspace_id, page_id);
}

auto FileDocumentRepository::DeleteKnowledgeForPage(std::string_view workspace_id,
                                                    std::string_view page_id)
    -> DeleteKnowledgeForPageResult {
  return impl_->DeleteKnowledgeForPage(workspace_id, page_id);
}

auto FileDocumentRepository::SaveRepositoryArtifact(const knowledge::RepositoryArtifact& artifact)
    -> SaveKnowledgeRecordResult {
  return impl_->SaveRepositoryArtifact(artifact);
}

auto FileDocumentRepository::DeleteRepositoryArtifact(std::string_view artifact_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeleteRepositoryArtifact(artifact_id);
}

auto FileDocumentRepository::ListRepositoryArtifacts(std::string_view workspace_id)
    -> ListRepositoryArtifactsResult {
  return impl_->ListRepositoryArtifacts(workspace_id);
}

auto FileDocumentRepository::SaveAgentRun(const knowledge::AgentRun& run)
    -> SaveKnowledgeRecordResult {
  return impl_->SaveAgentRun(run);
}

auto FileDocumentRepository::DeleteAgentRun(std::string_view run_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeleteAgentRun(run_id);
}

auto FileDocumentRepository::ListAgentRuns(std::string_view workspace_id) -> ListAgentRunsResult {
  return impl_->ListAgentRuns(workspace_id);
}

auto FileDocumentRepository::SaveResultReference(const knowledge::ResultReference& reference)
    -> SaveKnowledgeRecordResult {
  return impl_->SaveResultReference(reference);
}

auto FileDocumentRepository::DeleteResultReference(std::string_view reference_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeleteResultReference(reference_id);
}

auto FileDocumentRepository::ListResultReferences(std::string_view workspace_id,
                                                  std::string_view agent_run_id)
    -> ListResultReferencesResult {
  return impl_->ListResultReferences(workspace_id, agent_run_id);
}

auto FileDocumentRepository::SaveContextPack(const knowledge::ContextPack& pack)
    -> SaveKnowledgeRecordResult {
  return impl_->SaveContextPack(pack);
}

auto FileDocumentRepository::DeleteContextPack(std::string_view pack_id)
    -> DeleteKnowledgeRecordResult {
  return impl_->DeleteContextPack(pack_id);
}

auto FileDocumentRepository::ListContextPacks(std::string_view workspace_id,
                                              std::string_view task_id)
    -> ListContextPacksResult {
  return impl_->ListContextPacks(workspace_id, task_id);
}

auto FileDocumentRepository::SaveAttachment(const AttachmentData& attachment)
    -> SaveAttachmentResult {
  return impl_->SaveAttachment(attachment);
}

auto FileDocumentRepository::LoadAttachment(std::string_view attachment_id,
                                            std::string_view workspace_id) -> LoadAttachmentResult {
  return impl_->LoadAttachment(attachment_id, workspace_id);
}

auto FileDocumentRepository::ListAttachments(std::string_view workspace_id)
    -> ListAttachmentsResult {
  return impl_->ListAttachments(workspace_id);
}

auto FileDocumentRepository::SaveConflict(const DocumentConflictRecord& conflict)
    -> SaveConflictResult {
  return impl_->SaveConflict(conflict);
}

auto FileDocumentRepository::DeleteConflict(std::string_view conflict_id) -> DeleteConflictResult {
  return impl_->DeleteConflict(conflict_id);
}

auto FileDocumentRepository::LoadConflict(std::string_view conflict_id) -> LoadConflictResult {
  return impl_->LoadConflict(conflict_id);
}

auto FileDocumentRepository::ListConflicts() -> ListConflictsResult {
  return impl_->ListConflicts();
}

auto FileDocumentRepository::ResolveConflict(std::string_view conflict_id)
    -> UpdateConflictResolutionResult {
  return impl_->ResolveConflict(conflict_id);
}

auto FileDocumentRepository::DismissConflict(std::string_view conflict_id)
    -> UpdateConflictResolutionResult {
  return impl_->DismissConflict(conflict_id);
}

auto FileDocumentRepository::GetSyncStatus() const -> SyncStatus {
  return impl_->GetSyncStatus();
}

auto FileDocumentRepository::SaveDocumentRevision(const DocumentRevisionRecord& revision)
    -> SaveDocumentRevisionResult {
  return impl_->SaveDocumentRevision(revision);
}

auto FileDocumentRepository::ListDocumentRevisions(std::string_view document_id)
    -> ListDocumentRevisionsResult {
  return impl_->ListDocumentRevisions(document_id);
}

auto FileDocumentRepository::DeleteDocumentRevision(std::string_view revision_id)
    -> DeleteDocumentRevisionResult {
  return impl_->DeleteDocumentRevision(revision_id);
}

}  // namespace cppwiki::storage
