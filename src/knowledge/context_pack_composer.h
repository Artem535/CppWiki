#ifndef CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_COMPOSER_H_
#define CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_COMPOSER_H_

#include <tf/composition.h>
#include <tf/error.h>
#include <tf/version.h>

#include <string>
#include <vector>

#include "document/block_note_snapshot.h"
#include "knowledge/knowledge_record.h"

namespace cppwiki::knowledge {

// Base block-walk plain text extraction (#202): paragraph/heading/list-item/checkbox text, with
// nested children flattened in document order (no indentation, no inline marks/formatting). Good
// enough for an external agent to read; a faithful BlockNote renderer is separate follow-up work,
// not this slice's job.
[[nodiscard]] auto ExtractPlainText(const document::BlockNoteDocumentSnapshot& snapshot)
    -> std::string;

// A resolved plain-text excerpt for one included ContextPackItem. How artifact_kind/artifact_id
// was turned into text is the resolver's job (context_pack_resolver.h) -- this module only
// assembles already-resolved content into a Composition.
struct ContextPackItemContent {
  std::string item_id;
  std::string text;
};

// Assembles a TextFoundryEngine Composition from a Context Pack: task intent, each included
// item's resolved content (in pack.items order), and repository guidance -- then publishes it
// under `version`, making it immutable. `item_contents` must have exactly one entry per included
// item (matched by item_id); any mismatch is a caller bug (resolution didn't cover every
// included item), reported as an error rather than silently skipped or padded.
[[nodiscard]] auto ComposeContextPack(const ContextPack& pack,
                                      const std::vector<ContextPackItemContent>& item_contents,
                                      tf::Version version) -> tf::Result<tf::Composition>;

}  // namespace cppwiki::knowledge

#endif  // CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_COMPOSER_H_
