// Issue #202: proves the TextFoundryEngine dependency (vendored as a plain Block/Composition/
// Renderer library, no ObjectBox -- see CMakeLists.txt) actually links and works end to end,
// before any Context Pack code is written against it.

#include <tf/composition.h>
#include <tf/renderer.h>
#include <tf/version.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

// A composition made only of StaticText/Separator fragments needs no BlockRef resolution, so
// this proves Composition/Renderer link and run without pulling in any storage layer.
auto TestRendersAStaticCompositionWithoutABlockCache() -> void {
  tf::Composition composition("context-pack-smoke");
  composition.AddStaticText("Task: fix the login bug");
  composition.AddSeparator(tf::SeparatorType::Paragraph);
  composition.AddStaticText("Repository: CppWiki");

  const auto publish_error = composition.publish(tf::Version{1, 0});
  Require(!publish_error.is_error(), "publishing a valid static composition must succeed");

  const tf::Renderer renderer;
  const auto result = renderer.Render(composition);
  Require(result.HasValue(), "rendering a published static composition must succeed");
  Require(result.value().text.find("fix the login bug") != std::string::npos,
          "rendered text must contain the static content");
  Require(result.value().compositionId == "context-pack-smoke",
          "render result must report the source composition id");
  Require(result.value().compositionVersion == tf::Version{1, 0},
          "render result must report the source composition version");
  Require(result.value().blocksUsed.empty(),
          "a static-only composition must report no blocks used");
}

}  // namespace

auto main() -> int {
  TestRendersAStaticCompositionWithoutABlockCache();
  std::cout << "cppwiki_text_foundry_engine_smoke_tests passed\n";
  return EXIT_SUCCESS;
}
