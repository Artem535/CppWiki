#include "storage/workspace_archive.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <optional>
#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "document/document.h"
#include "knowledge/knowledge_record.h"

namespace cppwiki::storage {
// Named (not anonymous) so these DTOs have external linkage -- reflect-cpp's JSON introspection
// (rfl::internal::any's templated conversion operator, used transitively by rfl::json::write()/
// read()) requires that of any type it's instantiated for. An anonymous namespace here compiled
// fine with GCC but is a hard error under Apple Clang ("used but not defined in this translation
// unit, and cannot be defined in any other translation unit because its type does not have
// linkage") -- matches the existing convention in file_document_repository.cc's named
// `file_repository` namespace, not an anonymous one.
namespace workspace_archive_internal {

constexpr std::int32_t kArchiveSchemaVersion = 2;

struct ArchiveWorkspaceDto {
  std::string workspace_id;
  std::string title;
  std::string created_at;
  std::int64_t schema_version{1};
};

// Issue #185: the archive format carries the four knowledge record types (property definitions,
// page property values, relation types, page relations) as their own arrays, mirroring the
// distinct on-disk records FileDocumentRepository uses. Each type gets a dedicated DTO rather
// than sharing FileDocumentRepository's DTOs, since the archive is its own concern (same
// convention as ArchiveDocumentDto above).
struct ArchiveAuditMetadataDto {
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
};

struct ArchivePropertyDefinitionDto {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::optional<std::string> group_name;
  std::int32_t value_kind{};
  std::vector<std::string> options;
  std::int32_t state{};
  ArchiveAuditMetadataDto audit;
};

struct ArchivePagePropertyValueDto {
  std::string id;
  std::string workspace_id;
  std::string page_id;
  std::string property_definition_id;
  std::vector<std::string> values;
  ArchiveAuditMetadataDto audit;
};

struct ArchiveRelationTypeDto {
  std::string id;
  std::string workspace_id;
  std::string name;
  std::optional<std::string> inverse_name;
  std::int32_t direction{};
  std::int32_t state{};
  ArchiveAuditMetadataDto audit;
};

struct ArchivePageRelationDto {
  std::string id;
  std::string workspace_id;
  std::string relation_type_id;
  std::string source_page_id;
  std::string target_page_id;
  ArchiveAuditMetadataDto audit;
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

struct ArchiveAttachmentDto {
  std::string id;
  std::string workspace_id;
  std::string filename;
  std::string mime_type;
  std::uint64_t size_bytes{};
  std::string sha256;
  std::string created_at;
  std::string created_by;
  std::string base64_bytes;
};

struct WorkspaceArchiveDto {
  std::int32_t archive_schema_version{kArchiveSchemaVersion};
  std::optional<ArchiveWorkspaceDto> workspace;
  std::vector<ArchiveDocumentDto> documents;
  std::vector<ArchiveConflictDto> conflicts;
  std::vector<ArchiveAttachmentDto> attachments;
  // Issue #185: knowledge arrays are optional in the DTO so an archive that predates them (an
  // older schema-v1 export, which simply has no knowledge records) still reads back as empty
  // collections rather than failing the whole import -- the contract's "readers tolerate their
  // absence as empty collections" rule. reflect-cpp only tolerates absent JSON fields that map
  // to std::optional members; a plain vector that is missing from the JSON fails the read.
  std::optional<std::vector<ArchivePropertyDefinitionDto>> property_definitions;
  std::optional<std::vector<ArchivePagePropertyValueDto>> page_property_values;
  std::optional<std::vector<ArchiveRelationTypeDto>> relation_types;
  std::optional<std::vector<ArchivePageRelationDto>> page_relations;
};

auto EncodeBase64(const std::vector<std::uint8_t>& bytes) -> std::string {
  constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;
  encoded.reserve(((bytes.size() + 2U) / 3U) * 4U);
  for (std::size_t index = 0; index < bytes.size(); index += 3U) {
    const auto first = bytes[index];
    const auto second = index + 1U < bytes.size() ? bytes[index + 1U] : 0U;
    const auto third = index + 2U < bytes.size() ? bytes[index + 2U] : 0U;
    const auto value = (static_cast<std::uint32_t>(first) << 16U) |
                       (static_cast<std::uint32_t>(second) << 8U) |
                       static_cast<std::uint32_t>(third);
    encoded.push_back(alphabet[(value >> 18U) & 0x3FU]);
    encoded.push_back(alphabet[(value >> 12U) & 0x3FU]);
    encoded.push_back(index + 1U < bytes.size() ? alphabet[(value >> 6U) & 0x3FU] : '=');
    encoded.push_back(index + 2U < bytes.size() ? alphabet[value & 0x3FU] : '=');
  }
  return encoded;
}

auto DecodeBase64(std::string_view encoded) -> std::optional<std::vector<std::uint8_t>> {
  if (encoded.size() % 4U != 0U) {
    return std::nullopt;
  }
  const auto value = [](char character) -> int {
    if (character >= 'A' && character <= 'Z')
      return character - 'A';
    if (character >= 'a' && character <= 'z')
      return character - 'a' + 26;
    if (character >= '0' && character <= '9')
      return character - '0' + 52;
    if (character == '+')
      return 62;
    if (character == '/')
      return 63;
    return -1;
  };
  std::vector<std::uint8_t> bytes;
  bytes.reserve((encoded.size() / 4U) * 3U);
  for (std::size_t index = 0; index < encoded.size(); index += 4U) {
    const bool third_padding = encoded[index + 2U] == '=';
    const bool fourth_padding = encoded[index + 3U] == '=';
    if ((third_padding && !fourth_padding) ||
        ((third_padding || fourth_padding) && index + 4U != encoded.size())) {
      return std::nullopt;
    }
    const auto first = value(encoded[index]);
    const auto second = value(encoded[index + 1U]);
    const auto third = third_padding ? 0 : value(encoded[index + 2U]);
    const auto fourth = fourth_padding ? 0 : value(encoded[index + 3U]);
    if (first < 0 || second < 0 || third < 0 || fourth < 0) {
      return std::nullopt;
    }
    const auto packed =
        (static_cast<std::uint32_t>(first) << 18U) | (static_cast<std::uint32_t>(second) << 12U) |
        (static_cast<std::uint32_t>(third) << 6U) | static_cast<std::uint32_t>(fourth);
    bytes.push_back(static_cast<std::uint8_t>((packed >> 16U) & 0xFFU));
    if (!third_padding)
      bytes.push_back(static_cast<std::uint8_t>((packed >> 8U) & 0xFFU));
    if (!fourth_padding)
      bytes.push_back(static_cast<std::uint8_t>(packed & 0xFFU));
  }
  return bytes;
}

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

auto ToDto(const AttachmentData& attachment) -> ArchiveAttachmentDto {
  return ArchiveAttachmentDto{
      .id = attachment.metadata.id,
      .workspace_id = attachment.metadata.workspace_id,
      .filename = attachment.metadata.filename,
      .mime_type = attachment.metadata.mime_type,
      .size_bytes = attachment.metadata.size_bytes,
      .sha256 = attachment.metadata.sha256,
      .created_at = attachment.metadata.created_at,
      .created_by = attachment.metadata.created_by,
      .base64_bytes = EncodeBase64(attachment.bytes),
  };
}

auto ToDto(const knowledge::AuditMetadata& audit) -> ArchiveAuditMetadataDto {
  return ArchiveAuditMetadataDto{
      .created_at = audit.created_at,
      .updated_at = audit.updated_at,
      .created_by = audit.created_by,
      .updated_by = audit.updated_by,
  };
}

auto FromDto(ArchiveAuditMetadataDto dto) -> knowledge::AuditMetadata {
  return knowledge::AuditMetadata{
      .created_at = std::move(dto.created_at),
      .updated_at = std::move(dto.updated_at),
      .created_by = std::move(dto.created_by),
      .updated_by = std::move(dto.updated_by),
  };
}

auto ToDto(const knowledge::PropertyDefinition& definition) -> ArchivePropertyDefinitionDto {
  return ArchivePropertyDefinitionDto{
      .id = definition.id,
      .workspace_id = definition.workspace_id,
      .name = definition.name,
      .group_name = definition.group_name,
      .value_kind = static_cast<std::int32_t>(definition.value_kind),
      .options = definition.options,
      .state = static_cast<std::int32_t>(definition.state),
      .audit = ToDto(definition.audit),
  };
}

auto FromDto(ArchivePropertyDefinitionDto dto) -> knowledge::PropertyDefinition {
  return knowledge::PropertyDefinition{
      .id = std::move(dto.id),
      .workspace_id = std::move(dto.workspace_id),
      .name = std::move(dto.name),
      .group_name = std::move(dto.group_name),
      .value_kind = static_cast<knowledge::PropertyValueKind>(dto.value_kind),
      .options = std::move(dto.options),
      .state = static_cast<knowledge::RecordState>(dto.state),
      .audit = FromDto(std::move(dto.audit)),
  };
}

auto ToDto(const knowledge::PagePropertyValue& value) -> ArchivePagePropertyValueDto {
  return ArchivePagePropertyValueDto{
      .id = value.id,
      .workspace_id = value.workspace_id,
      .page_id = value.page_id,
      .property_definition_id = value.property_definition_id,
      .values = value.values,
      .audit = ToDto(value.audit),
  };
}

auto FromDto(ArchivePagePropertyValueDto dto) -> knowledge::PagePropertyValue {
  return knowledge::PagePropertyValue{
      .id = std::move(dto.id),
      .workspace_id = std::move(dto.workspace_id),
      .page_id = std::move(dto.page_id),
      .property_definition_id = std::move(dto.property_definition_id),
      .values = std::move(dto.values),
      .audit = FromDto(std::move(dto.audit)),
  };
}

auto ToDto(const knowledge::RelationType& type) -> ArchiveRelationTypeDto {
  return ArchiveRelationTypeDto{
      .id = type.id,
      .workspace_id = type.workspace_id,
      .name = type.name,
      .inverse_name = type.inverse_name,
      .direction = static_cast<std::int32_t>(type.direction),
      .state = static_cast<std::int32_t>(type.state),
      .audit = ToDto(type.audit),
  };
}

auto FromDto(ArchiveRelationTypeDto dto) -> knowledge::RelationType {
  return knowledge::RelationType{
      .id = std::move(dto.id),
      .workspace_id = std::move(dto.workspace_id),
      .name = std::move(dto.name),
      .inverse_name = std::move(dto.inverse_name),
      .direction = static_cast<knowledge::RelationDirection>(dto.direction),
      .state = static_cast<knowledge::RecordState>(dto.state),
      .audit = FromDto(std::move(dto.audit)),
  };
}

auto ToDto(const knowledge::PageRelation& relation) -> ArchivePageRelationDto {
  return ArchivePageRelationDto{
      .id = relation.id,
      .workspace_id = relation.workspace_id,
      .relation_type_id = relation.relation_type_id,
      .source_page_id = relation.source_page_id,
      .target_page_id = relation.target_page_id,
      .audit = ToDto(relation.audit),
  };
}

auto FromDto(ArchivePageRelationDto dto) -> knowledge::PageRelation {
  return knowledge::PageRelation{
      .id = std::move(dto.id),
      .workspace_id = std::move(dto.workspace_id),
      .relation_type_id = std::move(dto.relation_type_id),
      .source_page_id = std::move(dto.source_page_id),
      .target_page_id = std::move(dto.target_page_id),
      .audit = FromDto(std::move(dto.audit)),
  };
}

auto MakeError(RepositoryErrorCode code, std::string message) -> RepositoryError {
  return RepositoryError{.code = code, .message = std::move(message)};
}

}  // namespace workspace_archive_internal

using namespace workspace_archive_internal;

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

  const auto attachments = repository.ListAttachments(workspace_id);
  if (attachments.error && attachments.error->code != RepositoryErrorCode::kUnsupported) {
    return ExportWorkspaceResult{.error = attachments.error};
  }
  for (const auto& metadata : attachments.attachments) {
    const auto loaded = repository.LoadAttachment(metadata.id, workspace_id);
    if (loaded.error || !loaded.attachment) {
      return ExportWorkspaceResult{
          .error = loaded.error.value_or(MakeError(RepositoryErrorCode::kReadFailed,
                                                   "Attachment disappeared during export."))};
    }
    archive.attachments.push_back(ToDto(*loaded.attachment));
  }

  // Issue #185: knowledge records are workspace-scoped, so the workspace-wide list methods give
  // us everything for this workspace directly. Values and relations are keyed per page, so we
  // walk the exported workspace's pages and collect each record once (deduplicated by id: a
  // relation stored once is reachable from either endpoint page, so both pages would list it).
  const auto property_definitions = repository.ListPropertyDefinitions(workspace_id);
  if (property_definitions.error &&
      property_definitions.error->code != RepositoryErrorCode::kUnsupported) {
    return ExportWorkspaceResult{.error = property_definitions.error};
  }
  std::vector<ArchivePropertyDefinitionDto> property_definition_dtos;
  for (const auto& definition : property_definitions.definitions) {
    property_definition_dtos.push_back(ToDto(definition));
  }
  archive.property_definitions = std::move(property_definition_dtos);

  const auto relation_types = repository.ListRelationTypes(workspace_id);
  if (relation_types.error && relation_types.error->code != RepositoryErrorCode::kUnsupported) {
    return ExportWorkspaceResult{.error = relation_types.error};
  }
  std::vector<ArchiveRelationTypeDto> relation_type_dtos;
  for (const auto& type : relation_types.relation_types) {
    relation_type_dtos.push_back(ToDto(type));
  }
  archive.relation_types = std::move(relation_type_dtos);

  std::vector<std::string> seen_value_ids;
  std::vector<std::string> seen_relation_ids;
  std::vector<ArchivePagePropertyValueDto> value_dtos;
  std::vector<ArchivePageRelationDto> relation_dtos;
  for (const auto& summary : documents.documents) {
    if (summary.workspace_id != workspace_id) {
      continue;
    }
    const auto values = repository.ListPagePropertyValues(workspace_id, summary.id);
    if (values.error && values.error->code != RepositoryErrorCode::kUnsupported) {
      return ExportWorkspaceResult{.error = values.error};
    }
    for (const auto& value : values.values) {
      if (std::find(seen_value_ids.begin(), seen_value_ids.end(), value.id) ==
          seen_value_ids.end()) {
        seen_value_ids.push_back(value.id);
        value_dtos.push_back(ToDto(value));
      }
    }
    const auto relations = repository.ListPageRelations(workspace_id, summary.id);
    if (relations.error && relations.error->code != RepositoryErrorCode::kUnsupported) {
      return ExportWorkspaceResult{.error = relations.error};
    }
    for (const auto& relation : relations.relations) {
      if (std::find(seen_relation_ids.begin(), seen_relation_ids.end(), relation.id) ==
          seen_relation_ids.end()) {
        seen_relation_ids.push_back(relation.id);
        relation_dtos.push_back(ToDto(relation));
      }
    }
  }
  archive.page_property_values = std::move(value_dtos);
  archive.page_relations = std::move(relation_dtos);

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
    return ImportWorkspaceResult{.error =
                                     MakeError(RepositoryErrorCode::kReadFailed,
                                               "Could not open " + source_path + " for reading.")};
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();

  auto parsed = rfl::json::read<WorkspaceArchiveDto>(buffer.str());
  if (!parsed) {
    return ImportWorkspaceResult{.error =
                                     MakeError(RepositoryErrorCode::kInvalidRecord,
                                               source_path + " is not a valid workspace archive.")};
  }
  auto archive = std::move(parsed).value();

  // Determine the target workspace before touching the repository: a knowledge record whose
  // workspace_id disagrees with the rest of the archive is a scope violation, and a value or
  // relation that points at a definition/type/page absent from the archive is dangling. Both
  // reject the whole archive up front (Issue #185), so a broken import never persists a partial
  // knowledge graph. Definitions and relation types must be committed before values/relations,
  // because the repository cross-validates a value against its property definition and a
  // relation against its relation type at save time.
  std::optional<std::string> workspace_id;
  if (archive.workspace) {
    workspace_id = archive.workspace->workspace_id;
  } else if (!archive.documents.empty()) {
    workspace_id = archive.documents.front().workspace_id;
  }

  std::optional<std::string> scope_error;
  if (!workspace_id) {
    scope_error = "archive contains no workspace data to restore";
  }

  // A legacy v1 archive has no knowledge arrays at all; in the DTO they arrived as optional
  // vectors precisely so that absence reads back as empty rather than failing the import.
  const auto property_definition_dtos =
      archive.property_definitions.value_or(std::vector<ArchivePropertyDefinitionDto>{});
  const auto relation_type_dtos =
      archive.relation_types.value_or(std::vector<ArchiveRelationTypeDto>{});
  const auto property_value_dtos =
      archive.page_property_values.value_or(std::vector<ArchivePagePropertyValueDto>{});
  const auto page_relation_dtos =
      archive.page_relations.value_or(std::vector<ArchivePageRelationDto>{});

  std::vector<std::string> definition_ids;
  std::vector<std::string> relation_type_ids;
  std::vector<std::string> page_ids;
  for (const auto& definition : property_definition_dtos) {
    if (definition.workspace_id != workspace_id) {
      scope_error = "archive contains a property definition from another workspace";
      break;
    }
    definition_ids.push_back(definition.id);
  }
  for (const auto& type : relation_type_dtos) {
    if (type.workspace_id != workspace_id) {
      scope_error = "archive contains a relation type from another workspace";
      break;
    }
    relation_type_ids.push_back(type.id);
  }
  for (const auto& document_dto : archive.documents) {
    page_ids.push_back(document_dto.id);
  }
  if (!scope_error) {
    for (const auto& value : property_value_dtos) {
      if (value.workspace_id != workspace_id) {
        scope_error = "archive contains a page property value from another workspace";
        break;
      }
      if (std::find(definition_ids.begin(), definition_ids.end(), value.property_definition_id) ==
          definition_ids.end()) {
        scope_error = "archive contains a page property value with a dangling property definition";
        break;
      }
      if (std::find(page_ids.begin(), page_ids.end(), value.page_id) == page_ids.end()) {
        scope_error = "archive contains a page property value with a dangling page reference";
        break;
      }
    }
  }
  if (!scope_error) {
    for (const auto& relation : page_relation_dtos) {
      if (relation.workspace_id != workspace_id) {
        scope_error = "archive contains a page relation from another workspace";
        break;
      }
      if (std::find(relation_type_ids.begin(), relation_type_ids.end(),
                    relation.relation_type_id) == relation_type_ids.end()) {
        scope_error = "archive contains a page relation with a dangling relation type";
        break;
      }
      if (std::find(page_ids.begin(), page_ids.end(), relation.source_page_id) == page_ids.end() ||
          std::find(page_ids.begin(), page_ids.end(), relation.target_page_id) == page_ids.end()) {
        scope_error = "archive contains a page relation with a dangling page reference";
        break;
      }
    }
  }
  if (scope_error) {
    return ImportWorkspaceResult{
        .error = MakeError(RepositoryErrorCode::kInvalidRecord, *scope_error)};
  }

  if (archive.workspace) {
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

  for (auto& attachment_dto : archive.attachments) {
    if (!workspace_id) {
      workspace_id = attachment_dto.workspace_id;
    }
    if (attachment_dto.workspace_id != *workspace_id) {
      return ImportWorkspaceResult{
          .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                             "Archive attachment belongs to another workspace.")};
    }
    auto bytes = DecodeBase64(attachment_dto.base64_bytes);
    if (!bytes) {
      return ImportWorkspaceResult{.error =
                                       MakeError(RepositoryErrorCode::kInvalidRecord,
                                                 "Archive attachment bytes are not valid base64.")};
    }
    auto saved = repository.SaveAttachment(AttachmentData{
        .metadata =
            AttachmentMetadata{
                .id = std::move(attachment_dto.id),
                .workspace_id = std::move(attachment_dto.workspace_id),
                .filename = std::move(attachment_dto.filename),
                .mime_type = std::move(attachment_dto.mime_type),
                .size_bytes = attachment_dto.size_bytes,
                .sha256 = std::move(attachment_dto.sha256),
                .created_at = std::move(attachment_dto.created_at),
                .created_by = std::move(attachment_dto.created_by),
            },
        .bytes = std::move(*bytes),
    });
    if (saved.error) {
      return ImportWorkspaceResult{.error = std::move(saved.error)};
    }
  }

  // Issue #185: knowledge definitions and relation types are restored before the values and
  // relations that reference them, because the repository cross-validates a value against its
  // property definition and a relation against its relation type at save time. Scope and
  // dangling-reference validation already ran up front, so these saves are expected to succeed;
  // a failure here is still surfaced rather than hidden.
  for (auto& definition_dto : archive.property_definitions.value_or(std::vector<ArchivePropertyDefinitionDto>{})) {
    auto save_result = repository.SavePropertyDefinition(FromDto(std::move(definition_dto)));
    if (save_result.error) {
      return ImportWorkspaceResult{.error = std::move(save_result.error)};
    }
  }
  for (auto& type_dto : archive.relation_types.value_or(std::vector<ArchiveRelationTypeDto>{})) {
    auto save_result = repository.SaveRelationType(FromDto(std::move(type_dto)));
    if (save_result.error) {
      return ImportWorkspaceResult{.error = std::move(save_result.error)};
    }
  }
  for (auto& value_dto :
       archive.page_property_values.value_or(std::vector<ArchivePagePropertyValueDto>{})) {
    auto save_result = repository.SavePagePropertyValue(FromDto(std::move(value_dto)));
    if (save_result.error) {
      return ImportWorkspaceResult{.error = std::move(save_result.error)};
    }
  }
  for (auto& relation_dto : archive.page_relations.value_or(std::vector<ArchivePageRelationDto>{})) {
    auto save_result = repository.SavePageRelation(FromDto(std::move(relation_dto)));
    if (save_result.error) {
      return ImportWorkspaceResult{.error = std::move(save_result.error)};
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
