#include "knowledge/context_pack_composer.h"

#include <algorithm>
#include <rfl/Generic.hpp>

namespace cppwiki::knowledge {
namespace {

auto ExtractInlineText(const rfl::Generic& content) -> std::string {
  if (const auto text = rfl::to_string(content)) {
    return *text;
  }
  const auto items = rfl::to_array(content);
  if (!items) {
    return "";
  }
  std::string text;
  for (const auto& item : *items) {
    const auto object = rfl::to_object(item);
    if (!object) {
      continue;
    }
    const auto text_value = object->get("text");
    if (!text_value) {
      continue;
    }
    if (const auto piece = rfl::to_string(*text_value)) {
      text += *piece;
    }
  }
  return text;
}

auto AppendBlockText(const document::BlockNoteBlockSnapshot& block, std::string& out) -> void {
  if (block.content) {
    const auto text = ExtractInlineText(*block.content);
    if (!text.empty()) {
      if (!out.empty()) {
        out += '\n';
      }
      out += text;
    }
  }
  if (block.children) {
    for (const auto& child : *block.children) {
      AppendBlockText(child, out);
    }
  }
}

auto FindContent(const std::vector<ContextPackItemContent>& contents, const std::string& item_id)
    -> const ContextPackItemContent* {
  const auto found = std::ranges::find_if(
      contents, [&item_id](const auto& content) { return content.item_id == item_id; });
  return found == contents.end() ? nullptr : &*found;
}

}  // namespace

auto ExtractPlainText(const document::BlockNoteDocumentSnapshot& snapshot) -> std::string {
  std::string text;
  if (snapshot.blocks) {
    for (const auto& block : *snapshot.blocks) {
      AppendBlockText(block, text);
    }
  }
  return text;
}

auto ComposeContextPack(const ContextPack& pack,
                        const std::vector<ContextPackItemContent>& item_contents,
                        tf::Version version) -> tf::Result<tf::Composition> {
  std::size_t included_count = 0;
  for (const auto& item : pack.items) {
    included_count += item.included ? 1 : 0;
  }
  if (item_contents.size() != included_count) {
    return tf::Result<tf::Composition>(
        tf::Error{tf::ErrorCode::MissingParam,
                 "Context pack item content does not match its included items."});
  }

  tf::Composition composition(pack.id);
  composition.AddStaticText(pack.task_intent);

  for (const auto& item : pack.items) {
    if (!item.included) {
      continue;
    }
    const auto* content = FindContent(item_contents, item.id);
    if (content == nullptr) {
      return tf::Result<tf::Composition>(
          tf::Error{tf::ErrorCode::MissingParam,
                   "Context pack item content is missing for item " + item.id});
    }
    composition.AddSeparator(tf::SeparatorType::Paragraph);
    composition.AddStaticText(content->text);
  }

  if (pack.repository_guidance) {
    composition.AddSeparator(tf::SeparatorType::Paragraph);
    composition.AddStaticText(*pack.repository_guidance);
  }

  if (const auto publish_error = composition.publish(version); publish_error.is_error()) {
    return tf::Result<tf::Composition>(publish_error);
  }
  return tf::Result<tf::Composition>(std::move(composition));
}

}  // namespace cppwiki::knowledge
