#include "knowledge/context_pack_render.h"

#include <tf/renderer.h>
#include <tf/version.h>

#include "knowledge/context_pack_composer.h"
#include "knowledge/context_pack_resolver.h"

namespace cppwiki::knowledge {
namespace {

constexpr tf::Version kContextPackCompositionVersion{1, 0};

}  // namespace

auto RenderApprovedContextPack(const ContextPack& pack,
                               storage::LocalDocumentRepository& repository)
    -> std::variant<ContextPackRenderResult, std::string> {
  if (const auto validation = ValidateContextPack(pack); validation) {
    return "Context pack is invalid: " + *validation;
  }
  if (pack.state != ContextPackState::kApproved) {
    return std::string("Only an approved context pack can be rendered.");
  }

  const auto resolved = ResolveContextPackItemContents(pack, repository);
  if (std::holds_alternative<std::string>(resolved)) {
    return std::get<std::string>(resolved);
  }
  const auto& item_contents = std::get<std::vector<ContextPackItemContent>>(resolved);

  auto composed = ComposeContextPack(pack, item_contents, kContextPackCompositionVersion);
  if (composed.HasError()) {
    return composed.error().message;
  }

  const tf::Renderer renderer;
  const auto rendered = renderer.Render(composed.value());
  if (rendered.HasError()) {
    return rendered.error().message;
  }

  return ContextPackRenderResult{
      .context_pack_ref = pack.id + "@" + kContextPackCompositionVersion.ToString(),
      .text = rendered.value().text,
  };
}

}  // namespace cppwiki::knowledge
