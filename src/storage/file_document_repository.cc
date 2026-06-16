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
  std::optional<std::string> parent_id;
  std::int32_t sort_order{};
  std::string created_at;
  std::string updated_at;
  std::string raw_snapshot_json;
};

auto ToDto(const DocumentRecord& document) -> FileDocumentRecordDto {
  return FileDocumentRecordDto{
      .id = document.metadata.id,
      .schema_version = static_cast<std::int32_t>(document.metadata.schema_version),
      .title = document.metadata.title,
      .parent_id = document.metadata.parent_id,
      .sort_order = document.metadata.sort_order,
      .created_at = document.metadata.created_at,
      .updated_at = document.metadata.updated_at,
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
              .parent_id = std::move(dto.parent_id),
              .sort_order = dto.sort_order,
              .created_at = std::move(dto.created_at),
              .updated_at = std::move(dto.updated_at),
          },
      .snapshot = document::BlockNoteDocumentSnapshot{},
      .raw_snapshot_json = std::move(dto.raw_snapshot_json),
  };
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

 private:
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

auto FileDocumentRepository::LoadDocument(std::string_view page_id) -> LoadDocumentResult {
  return impl_->LoadDocument(page_id);
}

auto FileDocumentRepository::ListDocuments() -> ListDocumentsResult {
  return impl_->ListDocuments();
}

}  // namespace cppwiki::storage
