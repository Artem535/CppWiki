#ifndef CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_
#define CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cppwiki::document {

enum class SchemaVersion : std::int32_t { kV1 = 1 };

enum class BlockType : std::uint8_t {
  kParagraph,
  kHeading,
  kBulletListItem,
  kNumberedListItem,
  kCheckListItem,
  kQuote,
  kToggleListItem,
  kCodeBlock,
  kTable,
  kImage,
  kVideo,
  kAudio,
  kFile,
  kDivider,
  kPageBreak,
};

struct HeadingProps {
  std::int8_t level{};
};

struct Block {
  std::string id;
  BlockType type{};
  std::string text_content;
  std::optional<HeadingProps> heading_props;
  std::optional<bool> checked;
  std::vector<Block> children;
};

struct PageMetadata {
  std::string id;
  SchemaVersion schema_version{SchemaVersion::kV1};
  std::string title;
  std::string workspace_id;
  std::optional<std::string> parent_id;
  std::int32_t sort_order{};
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
  std::int64_t content_version{1};
};

struct Document {
  PageMetadata metadata;
  std::vector<Block> blocks;
};

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_
