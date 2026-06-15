#include "storage/file_document_repository.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>

namespace cppwiki::storage {
namespace {

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
    if (std::isalnum(c) || c == '-' || c == '_') {
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

}  // namespace

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

 private:
  [[nodiscard]] auto SerializeDocument(const DocumentRecord& document) -> std::string {
    // Build JSON manually for simplicity (no external JSON library needed for basic storage)
    // In production, could use reflect-cpp or nlohmann/json
    std::string json = "{";
    json += "\"id\":\"" + EscapeJsonString(document.metadata.id) + "\",";
    json += "\"schema_version\":" +
            std::to_string(static_cast<int>(document.metadata.schema_version)) + ",";
    json += "\"title\":\"" + EscapeJsonString(document.metadata.title) + "\",";
    json += "\"raw_snapshot\":" + document.raw_snapshot_json;
    json += "}";
    return json;
  }

  [[nodiscard]] auto DeserializeDocument(const std::string& content, std::string_view expected_id)
      -> LoadDocumentResult {
    // For now, use a simple approach - parse JSON fields we need
    // The raw_snapshot_json is preserved as-is for the DocumentRecord

    DocumentRecord record;
    record.metadata.id = std::string(expected_id);
    record.metadata.schema_version = document::SchemaVersion::kV1;
    record.raw_snapshot_json = content;  // Store the entire JSON as raw snapshot

    // Try to extract title from the JSON
    // This is a simple parser - in production, use proper JSON library
    const auto title_start = content.find("\"title\":\"");
    if (title_start != std::string::npos) {
      const auto value_start = title_start + 9;
      const auto value_end = content.find("\"", value_start);
      if (value_end != std::string::npos) {
        record.metadata.title = content.substr(value_start, value_end - value_start);
      }
    }

    // Try to find and extract raw_snapshot
    const auto snapshot_start = content.find("\"raw_snapshot\":");
    if (snapshot_start != std::string::npos) {
      const auto value_start = snapshot_start + 15;  // length of "raw_snapshot":
      // Find matching closing brace (this is a simplified approach)
      auto brace_count = 0;
      auto in_string = false;
      auto escape_next = false;
      std::size_t end_pos = value_start;

      for (std::size_t i = value_start; i < content.size(); ++i) {
        const char c = content[i];
        if (escape_next) {
          escape_next = false;
          continue;
        }
        if (c == '\\') {
          escape_next = true;
          continue;
        }
        if (c == '"' && !in_string) {
          in_string = !in_string;
        } else if (c == '"' && in_string) {
          in_string = !in_string;
        } else if (c == '{' && !in_string) {
          brace_count++;
        } else if (c == '}' && !in_string) {
          brace_count--;
          if (brace_count == 0) {
            end_pos = i + 1;
            break;
          }
        }
      }

      if (end_pos > value_start) {
        record.raw_snapshot_json = content.substr(value_start, end_pos - value_start);
      }
    }

    // Note: We don't fully parse the snapshot into BlockNoteDocumentSnapshot here
    // because the validator will do that. We just preserve the JSON.

    return LoadDocumentResult{
        .document = std::make_optional(std::move(record)),
        .error = std::nullopt,
    };
  }

  [[nodiscard]] static auto EscapeJsonString(std::string_view str) -> std::string {
    std::string result;
    for (char c : str) {
      switch (c) {
        case '"':
          result += "\\\"";
          break;
        case '\\':
          result += "\\\\";
          break;
        case '\b':
          result += "\\b";
          break;
        case '\f':
          result += "\\f";
          break;
        case '\n':
          result += "\\n";
          break;
        case '\r':
          result += "\\r";
          break;
        case '\t':
          result += "\\t";
          break;
        default:
          result += c;
      }
    }
    return result;
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

}  // namespace cppwiki::storage
