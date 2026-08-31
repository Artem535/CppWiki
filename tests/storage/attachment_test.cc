#include "storage/attachment.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string_view>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto MakeMetadata() -> cppwiki::storage::AttachmentMetadata {
  return cppwiki::storage::AttachmentMetadata{
      .id = "a1dcb4e6-8442-4ee4-94b4-8cef8d5c1f16",
      .workspace_id = "engineering",
      .filename = "architecture.png",
      .mime_type = "image/png",
      .size_bytes = 1024,
      .sha256 = "0123456789abcdef",
      .created_at = "2026-08-31T10:00:00Z",
      .created_by = "tester",
  };
}

auto TestAttachmentUriRoundTrip() -> void {
  const auto uri = cppwiki::storage::MakeAttachmentUri("a1dcb4e6-8442-4ee4-94b4-8cef8d5c1f16");
  Require(uri == "cppwiki-attachment://a1dcb4e6-8442-4ee4-94b4-8cef8d5c1f16",
          "attachment URI must use the private scheme");

  const auto parsed = cppwiki::storage::ParseAttachmentUri(uri);
  Require(parsed.has_value(), "private attachment URI must parse");
  Require(*parsed == "a1dcb4e6-8442-4ee4-94b4-8cef8d5c1f16", "parsed attachment ID must match");

  Require(!cppwiki::storage::ParseAttachmentUri("file:///tmp/architecture.png").has_value(),
          "filesystem URL must not be accepted");
  Require(!cppwiki::storage::ParseAttachmentUri("https://example.test/image.png").has_value(),
          "network URL must not be accepted");
  Require(!cppwiki::storage::ParseAttachmentUri("cppwiki-attachment://").has_value(),
          "attachment URI must contain an ID");
}

auto TestAttachmentSizeBoundary() -> void {
  Require(!cppwiki::storage::ValidateAttachmentSize(cppwiki::storage::kMaxAttachmentSizeBytes)
               .has_value(),
          "25 MiB attachment must be accepted");
  Require(cppwiki::storage::ValidateAttachmentSize(cppwiki::storage::kMaxAttachmentSizeBytes + 1)
              .has_value(),
          "attachment above 25 MiB must be rejected");
}

auto TestAttachmentMetadataRejectsUnsafeFilename() -> void {
  auto metadata = MakeMetadata();
  Require(!cppwiki::storage::ValidateAttachmentMetadata(metadata).has_value(),
          "valid metadata must be accepted");

  metadata.filename = "../secret.txt";
  Require(cppwiki::storage::ValidateAttachmentMetadata(metadata).has_value(),
          "filename traversal must be rejected");

  metadata = MakeMetadata();
  metadata.filename = "dir/file.txt";
  Require(cppwiki::storage::ValidateAttachmentMetadata(metadata).has_value(),
          "path separator in filename must be rejected");

  metadata = MakeMetadata();
  metadata.id.clear();
  Require(cppwiki::storage::ValidateAttachmentMetadata(metadata).has_value(),
          "empty attachment ID must be rejected");
}

}  // namespace

auto main() -> int {
  TestAttachmentUriRoundTrip();
  TestAttachmentSizeBoundary();
  TestAttachmentMetadataRejectsUnsafeFilename();
  return EXIT_SUCCESS;
}
