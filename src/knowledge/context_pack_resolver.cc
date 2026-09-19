#include "knowledge/context_pack_resolver.h"

#include <rfl/json/read.hpp>

namespace cppwiki::knowledge {

auto ResolveContextPackItemContents(const ContextPack& pack,
                                    storage::LocalDocumentRepository& repository)
    -> std::variant<std::vector<ContextPackItemContent>, std::string> {
  std::vector<ContextPackItemContent> contents;
  for (const auto& item : pack.items) {
    if (!item.included) {
      continue;
    }
    if (item.artifact_kind != ArtifactKind::kPage) {
      return std::string(
          "Context pack item " + item.id +
          " has an artifact kind that cannot be resolved into content yet.");
    }
    auto loaded = repository.LoadDocument(item.artifact_id);
    if (loaded.error || !loaded.document) {
      return std::string("Context pack item " + item.id +
                         " refers to a page that could not be loaded: " + item.artifact_id);
    }
    // LocalDocumentRepository backends persist a document's content as raw_snapshot_json, not
    // the parsed `snapshot` field (which LoadDocument leaves default-constructed) -- see
    // FileDocumentRepository/CbliteDocumentRepository's DocumentRecord round-trip.
    const auto snapshot =
        rfl::json::read<document::BlockNoteDocumentSnapshot>(loaded.document->raw_snapshot_json);
    if (!snapshot) {
      return std::string("Context pack item " + item.id +
                         " refers to a page whose content could not be parsed: " +
                         item.artifact_id);
    }
    contents.push_back(ContextPackItemContent{
        .item_id = item.id,
        .text = ExtractPlainText(snapshot.value()),
    });
  }
  return contents;
}

}  // namespace cppwiki::knowledge
