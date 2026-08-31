#include "storage/attachment.h"

#include <algorithm>

namespace cppwiki::storage {

auto MakeAttachmentUri(std::string_view attachment_id) -> std::string {
  return std::string(kAttachmentUriScheme) + std::string(attachment_id);
}

auto ParseAttachmentUri(std::string_view uri) -> std::optional<std::string> {
  if (!uri.starts_with(kAttachmentUriScheme)) {
    return std::nullopt;
  }

  const auto attachment_id = uri.substr(kAttachmentUriScheme.size());
  if (attachment_id.empty() || attachment_id.find_first_of("/?#") != std::string_view::npos) {
    return std::nullopt;
  }

  return std::string(attachment_id);
}

auto ValidateAttachmentSize(std::uint64_t size_bytes) -> std::optional<std::string> {
  if (size_bytes > kMaxAttachmentSizeBytes) {
    return "Attachment exceeds the 25 MiB size limit.";
  }
  return std::nullopt;
}

auto ValidateAttachmentMetadata(const AttachmentMetadata& metadata) -> std::optional<std::string> {
  if (metadata.id.empty()) {
    return "Attachment ID must not be empty.";
  }
  if (metadata.workspace_id.empty()) {
    return "Attachment workspace ID must not be empty.";
  }
  if (metadata.filename.empty()) {
    return "Attachment filename must not be empty.";
  }
  if (metadata.filename.find_first_of("/\\\0") != std::string::npos) {
    return "Attachment filename must not contain a path separator or NUL.";
  }
  if (metadata.mime_type.empty()) {
    return "Attachment MIME type must not be empty.";
  }
  return ValidateAttachmentSize(metadata.size_bytes);
}

}  // namespace cppwiki::storage
