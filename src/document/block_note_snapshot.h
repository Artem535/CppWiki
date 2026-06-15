#ifndef CPPWIKI_SRC_DOCUMENT_BLOCK_NOTE_SNAPSHOT_H_
#define CPPWIKI_SRC_DOCUMENT_BLOCK_NOTE_SNAPSHOT_H_

#include <rfl/Generic.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cppwiki::document {

struct BlockNoteBlockProps {
  std::optional<std::int8_t> level;
  std::optional<bool> checked;
};

struct BlockNoteBlockSnapshot {
  std::optional<std::string> id;
  std::optional<std::string> type;
  std::optional<rfl::Generic> content;
  std::optional<BlockNoteBlockProps> props;
  std::optional<bool> checked;
  std::optional<std::vector<BlockNoteBlockSnapshot>> children;
};

struct BlockNoteDocumentSnapshot {
  std::optional<std::string> id;
  std::optional<std::int32_t> schema_version;
  std::optional<std::string> title;
  std::optional<std::vector<BlockNoteBlockSnapshot>> blocks;
};

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_BLOCK_NOTE_SNAPSHOT_H_
