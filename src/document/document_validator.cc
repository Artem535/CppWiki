#include "document/document_validator.h"

#include <rfl/json/read.hpp>
#include <spdlog/spdlog.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>

namespace cppwiki::document {

namespace {

struct BlockResult {
  std::optional<Block> block;
  std::optional<ValidationError> error;
};

auto ConvertBlock(const BlockNoteBlockSnapshot& snapshot,
                  std::unordered_set<std::string>& seen_ids) -> BlockResult;

auto MakeValidationError(ValidationErrorCode code, std::string message) -> ValidationError {
  return ValidationError{
      .code = code,
      .message = std::move(message),
  };
}

auto MakeError(ValidationErrorCode code, std::string message) -> DocumentValidator::Result {
  return DocumentValidator::Result{
      .document = std::nullopt,
      .snapshot = std::nullopt,
      .raw_snapshot_json = "",
      .error = MakeValidationError(code, std::move(message)),
  };
}

auto MakeBlockError(ValidationErrorCode code, std::string message) -> BlockResult {
  return BlockResult{
      .block = std::nullopt,
      .error = MakeValidationError(code, std::move(message)),
  };
}

[[nodiscard]] auto ToJsonView(const QByteArray& snapshot_json) -> std::string_view {
  return std::string_view(snapshot_json.constData(),
                          static_cast<std::size_t>(snapshot_json.size()));
}

[[nodiscard]] auto ParseBlockType(std::string_view type) -> std::optional<BlockType> {
  constexpr std::array kBlockTypes = {
      std::pair{std::string_view{"paragraph"}, BlockType::kParagraph},
      std::pair{std::string_view{"heading"}, BlockType::kHeading},
      std::pair{std::string_view{"bulletListItem"}, BlockType::kBulletListItem},
      std::pair{std::string_view{"numberedListItem"}, BlockType::kNumberedListItem},
      std::pair{std::string_view{"checkListItem"}, BlockType::kCheckListItem},
      std::pair{std::string_view{"quote"}, BlockType::kQuote},
      std::pair{std::string_view{"toggleListItem"}, BlockType::kToggleListItem},
      std::pair{std::string_view{"codeBlock"}, BlockType::kCodeBlock},
      std::pair{std::string_view{"table"}, BlockType::kTable},
      std::pair{std::string_view{"image"}, BlockType::kImage},
      std::pair{std::string_view{"video"}, BlockType::kVideo},
      std::pair{std::string_view{"audio"}, BlockType::kAudio},
      std::pair{std::string_view{"file"}, BlockType::kFile},
      std::pair{std::string_view{"divider"}, BlockType::kDivider},
      std::pair{std::string_view{"pageBreak"}, BlockType::kPageBreak},
  };

  for (const auto& [block_type_name, block_type] : kBlockTypes) {
    if (type == block_type_name) {
      return block_type;
    }
  }

  return std::nullopt;
}

auto ExtractInlineText(const rfl::Generic& value) -> std::string {
  if (const auto text = rfl::to_string(value)) {
    return *text;
  }

  const auto content_items = rfl::to_array(value);
  if (!content_items) {
    return "";
  }

  std::string text_content;
  for (const auto& content_item : *content_items) {
    const auto content_object = rfl::to_object(content_item);
    if (!content_object) {
      continue;
    }

    const auto text_value = content_object->get("text");
    if (!text_value) {
      continue;
    }

    const auto text = rfl::to_string(*text_value);
    if (text) {
      text_content += *text;
    }
  }

  return text_content;
}

auto ConvertChildren(const std::optional<std::vector<BlockNoteBlockSnapshot>>& snapshots,
                     std::unordered_set<std::string>& seen_ids)
    -> std::variant<std::vector<Block>, ValidationError> {
  std::vector<Block> children;
  if (!snapshots) {
    return children;
  }

  for (const auto& snapshot : *snapshots) {
    auto child = ConvertBlock(snapshot, seen_ids);
    if (child.error) {
      return std::move(*child.error);
    }
    children.push_back(std::move(*child.block));
  }
  return children;
}

auto ConvertBlock(const BlockNoteBlockSnapshot& snapshot,
                  std::unordered_set<std::string>& seen_ids) -> BlockResult {
  if (!snapshot.id) {
    return MakeBlockError(ValidationErrorCode::kMissingBlockId, "Block is missing an id.");
  }
  if (snapshot.id->empty()) {
    return MakeBlockError(ValidationErrorCode::kEmptyBlockId, "Block id must not be empty.");
  }
  if (!seen_ids.insert(*snapshot.id).second) {
    return MakeBlockError(ValidationErrorCode::kDuplicateBlockId,
                          "Duplicate block id: " + *snapshot.id);
  }

  if (!snapshot.type) {
    return MakeBlockError(ValidationErrorCode::kUnsupportedBlockType,
                          "Unsupported block type: <missing>");
  }

  const auto block_type = ParseBlockType(*snapshot.type);
  if (!block_type) {
    const auto type = snapshot.type.value_or("<missing>");
    return MakeBlockError(ValidationErrorCode::kUnsupportedBlockType,
                          "Unsupported block type: " + type);
  }

  auto children = ConvertChildren(snapshot.children, seen_ids);
  if (std::holds_alternative<ValidationError>(children)) {
    return BlockResult{
        .block = std::nullopt,
        .error = std::move(std::get<ValidationError>(children)),
    };
  }

  Block block{
      .id = *snapshot.id,
      .type = *block_type,
      .text_content = snapshot.content ? ExtractInlineText(*snapshot.content) : "",
      .heading_props = std::nullopt,
      .checked = std::nullopt,
      .children = std::move(std::get<std::vector<Block>>(children)),
  };

  if (*block_type == BlockType::kHeading) {
    if (!snapshot.props || !snapshot.props->level) {
      return MakeBlockError(ValidationErrorCode::kInvalidHeadingLevel,
                            "Heading block is missing props.level.");
    }
    const auto level = *snapshot.props->level;
    if (level < 1 || level > 6) {
      return MakeBlockError(ValidationErrorCode::kInvalidHeadingLevel,
                            "Heading level must be between 1 and 6.");
    }
    block.heading_props = HeadingProps{.level = level};
  }

  if (*block_type == BlockType::kCheckListItem) {
    block.checked = snapshot.checked.value_or(
        snapshot.props && snapshot.props->checked ? *snapshot.props->checked : false);
  }

  return BlockResult{
      .block = std::move(block),
      .error = std::nullopt,
  };
}

auto ConvertBlocks(const std::vector<BlockNoteBlockSnapshot>& snapshots,
                   std::string raw_snapshot_json) -> DocumentValidator::Result {
  std::unordered_set<std::string> seen_ids;
  std::vector<Block> blocks;

  for (const auto& snapshot : snapshots) {
    auto block = ConvertBlock(snapshot, seen_ids);
    if (block.error) {
      return DocumentValidator::Result{
          .document = std::nullopt,
          .snapshot = std::nullopt,
          .raw_snapshot_json = "",
          .error = std::move(block.error),
      };
    }
    blocks.push_back(std::move(*block.block));
  }

  return DocumentValidator::Result{
      .document =
          Document{
              .metadata =
                  PageMetadata{
                      .id = "",
                      .schema_version = SchemaVersion::kV1,
                      .title = "",
                      .workspace_id = "",
                      .parent_id = std::nullopt,
                      .sort_order = 0,
                      .created_at = "",
                      .updated_at = "",
                      .created_by = "",
                  },
              .blocks = std::move(blocks),
          },
      .snapshot =
          BlockNoteDocumentSnapshot{
              .id = std::nullopt,
              .schema_version = std::nullopt,
              .title = std::nullopt,
              .blocks = snapshots,
          },
      .raw_snapshot_json = std::move(raw_snapshot_json),
      .error = std::nullopt,
  };
}

auto ConvertDocument(const BlockNoteDocumentSnapshot& snapshot,
                     std::string raw_snapshot_json) -> DocumentValidator::Result {
  if (!snapshot.schema_version) {
    return MakeError(ValidationErrorCode::kMissingSchemaVersion,
                     "Document object is missing schema_version.");
  }
  if (*snapshot.schema_version != static_cast<std::int32_t>(SchemaVersion::kV1)) {
    return MakeError(ValidationErrorCode::kMissingSchemaVersion,
                     "Document schema_version is not supported.");
  }
  if (!snapshot.id || snapshot.id->empty()) {
    return MakeError(ValidationErrorCode::kMissingPageId, "Document object is missing id.");
  }
  if (!snapshot.blocks) {
    return MakeError(ValidationErrorCode::kInvalidRoot, "Document object is missing blocks.");
  }

  auto result = ConvertBlocks(*snapshot.blocks, std::move(raw_snapshot_json));
  if (result.error) {
    return result;
  }

  result.document->metadata.id = *snapshot.id;
  result.document->metadata.title = snapshot.title.value_or("");
  result.snapshot = snapshot;
  return result;
}

}  // namespace

auto ToString(ValidationErrorCode code) -> std::string {
  switch (code) {
    case ValidationErrorCode::kInvalidJson:
      return "invalid_json";
    case ValidationErrorCode::kInvalidRoot:
      return "invalid_root";
    case ValidationErrorCode::kMissingSchemaVersion:
      return "missing_schema_version";
    case ValidationErrorCode::kMissingPageId:
      return "missing_page_id";
    case ValidationErrorCode::kMissingBlockId:
      return "missing_block_id";
    case ValidationErrorCode::kEmptyBlockId:
      return "empty_block_id";
    case ValidationErrorCode::kDuplicateBlockId:
      return "duplicate_block_id";
    case ValidationErrorCode::kUnsupportedBlockType:
      return "unsupported_block_type";
    case ValidationErrorCode::kInvalidHeadingLevel:
      return "invalid_heading_level";
    case ValidationErrorCode::kInvalidChildren:
      return "invalid_children";
  }
  return "unknown_validation_error";
}

auto DocumentValidator::ParseAndValidateSnapshot(const QByteArray& snapshot_json) -> Result {
  const auto json_view = ToJsonView(snapshot_json);
  const auto raw_snapshot_json = std::string(json_view);

  auto document_result = rfl::json::read<BlockNoteDocumentSnapshot>(json_view);
  if (document_result) {
    return ConvertDocument(*document_result, raw_snapshot_json);
  }

  auto blocks_result = rfl::json::read<std::vector<BlockNoteBlockSnapshot>>(json_view);
  if (blocks_result) {
    return ConvertBlocks(*blocks_result, raw_snapshot_json);
  }

  const auto document_error = document_result.error().what();
  const auto blocks_error = blocks_result.error().what();
  spdlog::warn("document snapshot rejected by reflect-cpp: document={}, blocks={}",
               document_error,
               blocks_error);

  if (document_error.find("Could not parse document") != std::string::npos ||
      blocks_error.find("Could not parse document") != std::string::npos) {
    return MakeError(ValidationErrorCode::kInvalidJson,
                     "Snapshot payload is not valid JSON.");
  }

  return MakeError(ValidationErrorCode::kInvalidRoot,
                   "Snapshot root must be a document object or a block array.");
}

}  // namespace cppwiki::document
