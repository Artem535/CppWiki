#include "knowledge/context_pack_composer.h"

#include <tf/renderer.h>
#include <tf/version.h>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <rfl/json/read.hpp>
#include <string>
#include <string_view>

namespace {

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}

auto ParseSnapshot(std::string_view json) -> cppwiki::document::BlockNoteDocumentSnapshot {
  const auto parsed = rfl::json::read<cppwiki::document::BlockNoteDocumentSnapshot>(std::string(json));
  if (!parsed) {
    std::cerr << "FAIL: could not parse fixture snapshot JSON\n";
    std::exit(EXIT_FAILURE);
  }
  return parsed.value();
}

auto TestExtractPlainTextFlattensParagraphsAndNestedChildren() -> void {
  const auto snapshot = ParseSnapshot(R"({
    "blocks": [
      {"id": "b1", "type": "paragraph", "content": [{"type": "text", "text": "Task: fix the login bug"}]},
      {"id": "b2", "type": "bulletListItem", "content": [{"type": "text", "text": "top item"}],
       "children": [
         {"id": "b2a", "type": "paragraph", "content": [{"type": "text", "text": "nested item"}]}
       ]}
    ]
  })");

  const auto text = cppwiki::knowledge::ExtractPlainText(snapshot);
  Require(text.find("Task: fix the login bug") != std::string::npos,
          "extracted text must contain the first paragraph's text");
  Require(text.find("top item") != std::string::npos,
          "extracted text must contain the list item's text");
  Require(text.find("nested item") != std::string::npos,
          "extracted text must contain a nested child's text, flattened");
}

auto TestExtractPlainTextIgnoresBlocksWithoutContent() -> void {
  const auto snapshot = ParseSnapshot(R"({
    "blocks": [
      {"id": "b1", "type": "divider"},
      {"id": "b2", "type": "paragraph", "content": [{"type": "text", "text": "visible"}]}
    ]
  })");

  const auto text = cppwiki::knowledge::ExtractPlainText(snapshot);
  Require(text.find("visible") != std::string::npos, "content-bearing block text must survive");
}

auto MakeAudit() -> cppwiki::knowledge::AuditMetadata {
  return {
      .created_at = "2026-09-19T10:00:00.000Z",
      .updated_at = "2026-09-19T10:00:00.000Z",
      .created_by = "tester",
      .updated_by = "tester",
  };
}

auto MakeApprovedPack() -> cppwiki::knowledge::ContextPack {
  return {
      .id = "pack-a",
      .workspace_id = "engineering",
      .task_id = "page-task-a",
      .task_intent = "Fix the login bug",
      .items =
          {
              cppwiki::knowledge::ContextPackItem{
                  .id = "item-1",
                  .relation_id = "edge-1",
                  .artifact_kind = cppwiki::knowledge::ArtifactKind::kPage,
                  .artifact_id = "page-auth",
                  .included = true,
              },
              cppwiki::knowledge::ContextPackItem{
                  .id = "item-2",
                  .relation_id = "edge-2",
                  .artifact_kind = cppwiki::knowledge::ArtifactKind::kPage,
                  .artifact_id = "page-notes",
                  .included = false,
              },
          },
      .repository_guidance = "Follow the existing auth module conventions.",
      .state = cppwiki::knowledge::ContextPackState::kApproved,
      .audit = MakeAudit(),
  };
}

auto TestComposeContextPackIncludesTaskIntentItemsAndGuidanceInOrder() -> void {
  const auto pack = MakeApprovedPack();
  const std::vector<cppwiki::knowledge::ContextPackItemContent> item_contents{
      {.item_id = "item-1", .text = "The auth page explains session handling."},
  };

  auto composed = cppwiki::knowledge::ComposeContextPack(pack, item_contents, tf::Version{1, 0});
  Require(composed.HasValue(), "composing a well-formed approved pack must succeed");

  const tf::Renderer renderer;
  const auto rendered = renderer.Render(composed.value());
  Require(rendered.HasValue(), "rendering the composed pack must succeed");
  Require(rendered.value().text.find("Fix the login bug") != std::string::npos,
          "rendered text must contain the task intent");
  Require(rendered.value().text.find("The auth page explains session handling.") !=
              std::string::npos,
          "rendered text must contain the included item's content");
  Require(rendered.value().text.find("Follow the existing auth module conventions.") !=
              std::string::npos,
          "rendered text must contain the repository guidance");
  Require(rendered.value().compositionId == "pack-a",
          "the composition id must be the context pack's id");
  Require(rendered.value().compositionVersion == tf::Version{1, 0},
          "the composition version must be the one it was published under");
}

auto TestComposeContextPackRejectsMismatchedItemContents() -> void {
  const auto pack = MakeApprovedPack();
  const std::vector<cppwiki::knowledge::ContextPackItemContent> empty_contents;

  const auto composed = cppwiki::knowledge::ComposeContextPack(pack, empty_contents, tf::Version{1, 0});
  Require(composed.HasError(),
          "composing must fail when an included item has no resolved content");
}

}  // namespace

auto main() -> int {
  TestExtractPlainTextFlattensParagraphsAndNestedChildren();
  TestExtractPlainTextIgnoresBlocksWithoutContent();
  TestComposeContextPackIncludesTaskIntentItemsAndGuidanceInOrder();
  TestComposeContextPackRejectsMismatchedItemContents();
  std::cout << "cppwiki_context_pack_composer_tests passed\n";
  return EXIT_SUCCESS;
}
