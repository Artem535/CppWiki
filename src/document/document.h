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
};

struct Document {
  PageMetadata metadata;
  std::vector<Block> blocks;
};

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_
