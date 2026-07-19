#include "document/document_validator.h"

#include <rfl/json/read.hpp>
#include <rfl/json/write.hpp>
#include <spdlog/spdlog.h>

#include <QByteArray>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto ToStringView(const QByteArray& bytes) -> std::string_view {
  return std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

auto Require(bool condition, std::string_view message) -> void {
  if (!condition) {
    spdlog::error("FAIL: {}", message);
    std::exit(EXIT_FAILURE);
  }
}

auto RequireError(
    const cppwiki::document::DocumentValidator::Result& result,
    std::string_view expected_error_substring,
    std::string_view test_name) -> void {
  Require(!result.document.has_value(),
          (std::string("expected no document: ") + std::string(test_name)).c_str());
  Require(result.error.has_value(),
          (std::string("expected error: ") + std::string(test_name)).c_str());
  Require(result.error->message.find(std::string(expected_error_substring)) != std::string::npos,
          (std::string("expected error containing '") +
           std::string(expected_error_substring) + "' in " + std::string(test_name))
              .c_str());
}

auto RequireSuccess(
    const cppwiki::document::DocumentValidator::Result& result,
    std::string_view test_name) -> void {
  Require(result.document.has_value(),
          (std::string("expected document: ") + std::string(test_name)).c_str());
  Require(!result.error.has_value(),
          (std::string("expected no error: ") + std::string(test_name)).c_str());
}

auto TestValidDocumentObject() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      {
        "id": "b1",
        "type": "heading",
        "props": { "level": 1 },
        "content": [{ "type": "text", "text": "Hello", "styles": {} }],
        "children": []
      },
      {
        "id": "b2",
        "type": "paragraph",
        "content": [{ "type": "text", "text": "World", "styles": {} }],
        "children": []
      }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireSuccess(result, "TestValidDocumentObject");
  Require(result.document->metadata.id == "page-1", "document id must be page-1");
  Require(result.document->metadata.title == "Getting Started",
          "document title must be Getting Started");
  Require(result.snapshot.has_value(), "validated document object must keep typed snapshot");
  Require(result.snapshot->id == "page-1", "typed snapshot id must be page-1");
  Require(result.raw_snapshot_json == std::string(ToStringView(json)),
          "validated document object must keep raw snapshot JSON");
  Require(result.document->blocks.size() == 2, "document must have two blocks");
}

auto TestValidBlockArray() -> void {
  const auto json = QByteArray(R"([
    {
      "id": "b1",
      "type": "heading",
      "props": { "level": 2 },
      "content": [{ "type": "text", "text": "Title", "styles": {} }],
      "children": []
    },
    {
      "id": "b2",
      "type": "paragraph",
      "content": [{ "type": "text", "text": "Body", "styles": {} }],
      "children": []
    }
  ])");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireSuccess(result, "TestValidBlockArray");
  Require(result.snapshot.has_value(), "validated block array must keep typed snapshot");
  Require(result.snapshot->blocks.has_value(), "typed snapshot must keep block array");
  Require(result.snapshot->blocks->size() == 2, "typed snapshot must keep two blocks");
  Require(result.raw_snapshot_json == std::string(ToStringView(json)),
          "validated block array must keep raw snapshot JSON");
  Require(result.document->blocks.size() == 2, "block array must wrap to two blocks");
}

auto TestValidBlockNoteInlineContentArray() -> void {
  const auto json = QByteArray(R"([
    {
      "id": "b1",
      "type": "heading",
      "props": { "level": 1 },
      "content": [
        { "type": "text", "text": "CppWiki", "styles": {} }
      ],
      "children": []
    },
    {
      "id": "b2",
      "type": "paragraph",
      "content": [
        { "type": "text", "text": "Hello ", "styles": {} },
        { "type": "text", "text": "BlockNote", "styles": { "bold": true } }
      ],
      "children": []
    }
  ])");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireSuccess(result, "TestValidBlockNoteInlineContentArray");
  Require(result.document->blocks.size() == 2, "BlockNote payload must produce two blocks");
  Require(result.document->blocks[0].text_content == "CppWiki", "heading text must be extracted");
  Require(result.document->blocks[1].text_content == "Hello BlockNote",
          "paragraph inline text must be concatenated");
}

auto TestReflectCppSnapshotRoundTrip() -> void {
  const auto json = QByteArray(R"([
    {
      "id": "b1",
      "type": "paragraph",
      "content": [
        { "type": "text", "text": "Roundtrip", "styles": { "bold": true } }
      ],
      "children": []
    }
  ])");

  const auto parsed =
      rfl::json::read<std::vector<cppwiki::document::BlockNoteBlockSnapshot>>(
          ToStringView(json));
  Require(static_cast<bool>(parsed), "reflect-cpp must read BlockNote block snapshots");

  const auto serialized = rfl::json::write(*parsed);
  const auto reparsed =
      rfl::json::read<std::vector<cppwiki::document::BlockNoteBlockSnapshot>>(serialized);
  Require(static_cast<bool>(reparsed), "reflect-cpp must read serialized BlockNote snapshots");
  Require(reparsed->size() == 1, "roundtrip must preserve block count");
  Require(reparsed->front().id == "b1", "roundtrip must preserve block id");
  Require(reparsed->front().type == "paragraph", "roundtrip must preserve block type");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(
      QByteArray::fromStdString(serialized));
  RequireSuccess(result, "TestReflectCppSnapshotRoundTrip");
  Require(result.document->blocks[0].text_content == "Roundtrip",
          "roundtrip must keep inline text readable");
}

auto TestValidQuoteBlock() -> void {
  const auto json = QByteArray(R"([
    {
      "id": "quote-1",
      "type": "quote",
      "content": [
        { "type": "text", "text": "Quoted text", "styles": {} }
      ],
      "children": []
    }
  ])");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireSuccess(result, "TestValidQuoteBlock");
  Require(result.document->blocks.size() == 1, "quote payload must produce one block");
  Require(result.document->blocks[0].type == cppwiki::document::BlockType::kQuote,
          "quote block type must be preserved");
  Require(result.document->blocks[0].text_content == "Quoted text",
          "quote text must be extracted");
}

auto TestValidDefaultBlockNoteBlockTypes() -> void {
  const auto json = QByteArray(R"([
    {
      "id": "paragraph-1",
      "type": "paragraph",
      "content": [{ "type": "text", "text": "Paragraph", "styles": {} }],
      "children": []
    },
    {
      "id": "heading-1",
      "type": "heading",
      "props": { "level": 2 },
      "content": [{ "type": "text", "text": "Heading", "styles": {} }],
      "children": []
    },
    {
      "id": "quote-1",
      "type": "quote",
      "content": [{ "type": "text", "text": "Quote", "styles": {} }],
      "children": []
    },
    {
      "id": "bullet-1",
      "type": "bulletListItem",
      "content": [{ "type": "text", "text": "Bullet", "styles": {} }],
      "children": []
    },
    {
      "id": "numbered-1",
      "type": "numberedListItem",
      "content": [{ "type": "text", "text": "Numbered", "styles": {} }],
      "children": []
    },
    {
      "id": "check-1",
      "type": "checkListItem",
      "props": { "checked": true },
      "content": [{ "type": "text", "text": "Checked", "styles": {} }],
      "children": []
    },
    {
      "id": "toggle-1",
      "type": "toggleListItem",
      "content": [{ "type": "text", "text": "Toggle", "styles": {} }],
      "children": []
    },
    {
      "id": "code-1",
      "type": "codeBlock",
      "props": { "language": "cpp" },
      "content": [{ "type": "text", "text": "int main() {}", "styles": {} }],
      "children": []
    },
    {
      "id": "table-1",
      "type": "table",
      "content": {
        "type": "tableContent",
        "rows": [
          {
            "cells": [
              [
                { "type": "text", "text": "A1", "styles": {} }
              ]
            ]
          }
        ]
      },
      "children": []
    },
    {
      "id": "image-1",
      "type": "image",
      "props": { "url": "", "caption": "", "name": "", "showPreview": true },
      "children": []
    },
    {
      "id": "video-1",
      "type": "video",
      "props": { "url": "", "caption": "", "name": "", "showPreview": true },
      "children": []
    },
    {
      "id": "audio-1",
      "type": "audio",
      "props": { "url": "", "caption": "", "name": "", "showPreview": true },
      "children": []
    },
    {
      "id": "file-1",
      "type": "file",
      "props": { "url": "", "caption": "", "name": "" },
      "children": []
    },
    {
      "id": "divider-1",
      "type": "divider",
      "children": []
    },
    {
      "id": "page-break-1",
      "type": "pageBreak",
      "children": []
    }
  ])");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireSuccess(result, "TestValidDefaultBlockNoteBlockTypes");
  Require(result.document->blocks.size() == 15, "all known BlockNote block types must be accepted");
  Require(result.document->blocks[6].type == cppwiki::document::BlockType::kToggleListItem,
          "toggle list item type must be preserved");
  Require(result.document->blocks[7].type == cppwiki::document::BlockType::kCodeBlock,
          "code block type must be preserved");
  Require(result.document->blocks[8].type == cppwiki::document::BlockType::kTable,
          "table type must be preserved");
  Require(result.document->blocks[13].type == cppwiki::document::BlockType::kDivider,
          "divider type must be preserved");
  Require(result.document->blocks[14].type == cppwiki::document::BlockType::kPageBreak,
          "page break type must be preserved");
}

auto TestInvalidJson() -> void {
  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(QByteArray("{"));
  RequireError(result, "valid JSON", "TestInvalidJson");
}

auto TestInvalidRoot() -> void {
  const auto result =
      cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(QByteArray("42"));
  RequireError(result, "object or a block array", "TestInvalidRoot");
}

auto TestMissingSchemaVersion() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "title": "Getting Started",
    "blocks": []
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireError(result, "schema_version", "TestMissingSchemaVersion");
}

auto TestMissingBlockId() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      { "type": "paragraph", "content": "No id" }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireError(result, "missing an id", "TestMissingBlockId");
}

auto TestEmptyBlockId() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      { "id": "", "type": "paragraph", "content": "Empty id" }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireError(result, "must not be empty", "TestEmptyBlockId");
}

auto TestDuplicateBlockId() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      { "id": "b1", "type": "paragraph", "content": "First" },
      { "id": "b1", "type": "paragraph", "content": "Duplicate" }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireError(result, "Duplicate block id", "TestDuplicateBlockId");
}

auto TestUnsupportedBlockType() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      { "id": "b1", "type": "callout", "content": "Unsupported" }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireError(result, "Unsupported block type", "TestUnsupportedBlockType");
}

auto TestInvalidHeadingLevel() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      { "id": "b1", "type": "heading", "props": { "level": 7 }, "content": "Too deep" }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  RequireError(result, "Heading level must be between 1 and 6", "TestInvalidHeadingLevel");
}

auto TestListNestingCycle() -> void {
  const auto json = QByteArray(R"({
    "id": "page-1",
    "schema_version": 1,
    "title": "Getting Started",
    "blocks": [
      {
        "id": "b1",
        "type": "bulletListItem",
        "children": [
          {
            "id": "b2",
            "type": "bulletListItem",
            "children": [
              { "id": "b1", "type": "paragraph", "content": "Cycle back to b1" }
            ]
          }
        ]
      }
    ]
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(json);
  // Duplicate ID in nested structures should be detected
  RequireError(result, "Duplicate block id", "TestListNestingCycle");
}

// ADR-017: non-kWikiPage kinds skip BlockNote block validation entirely — arbitrary JSON
// (nbformat, Excalidraw scene JSON) that would never pass as BlockNote content must still be
// accepted, since schema validation for those kinds is out of scope for this validator (#52/#53
// own it). Only "is this well-formed JSON at all" is checked.
auto TestJupyterNotebookKindAcceptsNonBlockNoteJson() -> void {
  const auto json = QByteArray(R"({
    "nbformat": 4,
    "nbformat_minor": 5,
    "cells": [
      { "cell_type": "markdown", "source": ["# Hello"], "metadata": {} }
    ],
    "metadata": {}
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(
      json, cppwiki::document::DocumentKind::kJupyterNotebook);
  Require(!result.error.has_value(), "TestJupyterNotebookKindAcceptsNonBlockNoteJson: no error");
  Require(!result.document.has_value(),
          "TestJupyterNotebookKindAcceptsNonBlockNoteJson: no Document produced (BlockNote-only "
          "type)");
  Require(result.raw_snapshot_json.find("nbformat") != std::string::npos,
          "TestJupyterNotebookKindAcceptsNonBlockNoteJson: raw JSON preserved");
}

auto TestExcalidrawCanvasKindAcceptsNonBlockNoteJson() -> void {
  const auto json = QByteArray(R"({
    "type": "excalidraw",
    "version": 2,
    "elements": [],
    "appState": {}
  })");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(
      json, cppwiki::document::DocumentKind::kExcalidrawCanvas);
  Require(!result.error.has_value(), "TestExcalidrawCanvasKindAcceptsNonBlockNoteJson: no error");
  Require(!result.document.has_value(),
          "TestExcalidrawCanvasKindAcceptsNonBlockNoteJson: no Document produced");
}

auto TestNonWikiPageKindStillRejectsInvalidJson() -> void {
  const auto json = QByteArray(R"(not json at all { )");

  const auto result = cppwiki::document::DocumentValidator::ParseAndValidateSnapshot(
      json, cppwiki::document::DocumentKind::kJupyterNotebook);
  RequireError(result, "not valid JSON", "TestNonWikiPageKindStillRejectsInvalidJson");
}

}  // namespace

auto main() -> int {
  TestValidDocumentObject();
  TestValidBlockArray();
  TestValidBlockNoteInlineContentArray();
  TestReflectCppSnapshotRoundTrip();
  TestValidQuoteBlock();
  TestValidDefaultBlockNoteBlockTypes();
  TestInvalidJson();
  TestInvalidRoot();
  TestMissingSchemaVersion();
  TestMissingBlockId();
  TestEmptyBlockId();
  TestDuplicateBlockId();
  TestUnsupportedBlockType();
  TestInvalidHeadingLevel();
  TestListNestingCycle();
  TestJupyterNotebookKindAcceptsNonBlockNoteJson();
  TestExcalidrawCanvasKindAcceptsNonBlockNoteJson();
  TestNonWikiPageKindStillRejectsInvalidJson();

  spdlog::info("cppwiki_document_validator_tests passed");
  return EXIT_SUCCESS;
}
