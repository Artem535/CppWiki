#ifndef CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_RENDER_H_
#define CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_RENDER_H_

#include <string>
#include <variant>

#include "knowledge/knowledge_record.h"
#include "storage/local_document_repository.h"

namespace cppwiki::knowledge {

struct ContextPackRenderResult {
  // What AgentRun.context_pack_ref stores: "<pack.id>@<major>.<minor>" -- the exact, immutable
  // composition version this render produced (#202's "how a later Agent Run refers to the exact
  // approved package" acceptance criterion).
  std::string context_pack_ref;
  std::string text;
};

// Freezes an approved Context Pack into an immutable, citable render: resolves each included
// item's content from `repository`, assembles a TextFoundryEngine Composition (task intent, item
// content, repository guidance), publishes it as version 1.0, and renders it. There is no
// re-approval/re-render flow yet (a pack is approved once), so the version is fixed rather than
// caller-supplied -- see the discussion on #202.
//
// `pack.state` must be kApproved: rendering a still-editable Draft would let a selection that can
// change after the fact produce a token an AgentRun could reference, defeating the "exact
// approved package" guarantee.
[[nodiscard]] auto RenderApprovedContextPack(const ContextPack& pack,
                                              storage::LocalDocumentRepository& repository)
    -> std::variant<ContextPackRenderResult, std::string>;

}  // namespace cppwiki::knowledge

#endif  // CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_RENDER_H_
