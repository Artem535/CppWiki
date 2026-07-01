#include "storage/file_document_repository.h"

#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

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

struct FileDocumentRecordDto {
  std::string id;
  std::int32_t schema_version{};
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

auto ToDto(const DocumentRecord& document) -> FileDocumentRecordDto {
  return FileDocumentRecordDto{
      .id = document.metadata.id,
      .schema_version = static_cast<std::int32_t>(document.metadata.schema_version),
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

auto FromDto(FileDocumentRecordDto dto) -> DocumentRecord {
  return DocumentRecord{
      .metadata =
          document::PageMetadata{
              .id = std::move(dto.id),
              .schema_version = document::SchemaVersion::kV1,
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
  explicit Impl(FileDocumentRepositoryOptions options) : options_(std::move(options)) {}

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
            .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                               std::string("Failed to parse conflict file: ") +
                                   parsed.error().what()),
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

      std::ranges::sort(
          conflicts, [](const DocumentConflictRecord& lhs, const DocumentConflictRecord& rhs) {
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
          .error = MakeError(RepositoryErrorCode::kInvalidRecord,
                             std::string("Failed to parse document file: ") +
                                 parsed.error().what()),
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

auto FileDocumentRepository::GetSyncStatus() const -> SyncStatus { return impl_->GetSyncStatus(); }

}  // namespace cppwiki::storage
