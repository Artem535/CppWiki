#ifndef CPPWIKI_SRC_STORAGE_ATTACHMENT_H_
#define CPPWIKI_SRC_STORAGE_ATTACHMENT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppwiki::storage {

inline constexpr std::uint64_t kMaxAttachmentSizeBytes = 25ULL * 1024ULL * 1024ULL;
inline constexpr std::string_view kAttachmentUriScheme = "cppwiki-attachment://";

struct AttachmentMetadata {
  std::string id;
  std::string workspace_id;
  std::string filename;
  std::string mime_type;
  std::uint64_t size_bytes{};
  std::string sha256;
  std::string created_at;
  std::string created_by;
};

struct AttachmentData {
  AttachmentMetadata metadata;
  std::vector<std::uint8_t> bytes;
};

[[nodiscard]] auto MakeAttachmentUri(std::string_view attachment_id) -> std::string;
[[nodiscard]] auto ParseAttachmentUri(std::string_view uri) -> std::optional<std::string>;
[[nodiscard]] auto ValidateAttachmentSize(std::uint64_t size_bytes) -> std::optional<std::string>;
[[nodiscard]] auto ValidateAttachmentMetadata(const AttachmentMetadata& metadata)
    -> std::optional<std::string>;

}  // namespace cppwiki::storage

#endif  // CPPWIKI_SRC_STORAGE_ATTACHMENT_H_
