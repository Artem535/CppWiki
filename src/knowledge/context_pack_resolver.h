#ifndef CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_RESOLVER_H_
#define CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_RESOLVER_H_

#include <string>
#include <variant>
#include <vector>

#include "knowledge/context_pack_composer.h"
#include "knowledge/knowledge_record.h"
#include "storage/local_document_repository.h"

namespace cppwiki::knowledge {

// Resolves the plain-text content for every included item in `pack` from `repository`. Only
// ArtifactKind::kPage is resolvable today -- PageRelation (the only source of item provenance
// right now; see NormalizeAndValidatePageRelation in knowledge_record.cc) doesn't allow any
// other artifact kind yet, so no ContextPackItem can legitimately carry one.
[[nodiscard]] auto ResolveContextPackItemContents(const ContextPack& pack,
                                                   storage::LocalDocumentRepository& repository)
    -> std::variant<std::vector<ContextPackItemContent>, std::string>;

}  // namespace cppwiki::knowledge

#endif  // CPPWIKI_SRC_KNOWLEDGE_CONTEXT_PACK_RESOLVER_H_
