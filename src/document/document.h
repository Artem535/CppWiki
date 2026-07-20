#ifndef CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_
#define CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cppwiki::document {

enum class SchemaVersion : std::int32_t { kV1 = 1 };

// The content-schema/renderer discriminator for a Document (ADR-017). All kinds share the
// same sync/lock/conflict machinery (ADR-010/ADR-013) unchanged; only the content shape
// stored in the snapshot and how it's rendered differ per kind. kWikiPage is the default so
// existing documents (which predate this field) round-trip as wiki pages unchanged.
enum class DocumentKind : std::uint8_t {
  kWikiPage,
  kJupyterNotebook,
  kExcalidrawCanvas,
};

enum class BlockType : std::uint8_t {
  kParagraph,
  kHeading,
  kBulletListItem,
  kNumberedListItem,
  kCheckListItem,
  kQuote,
  kToggleListItem,
  kCodeBlock,
  kTable,
  kImage,
  kVideo,
  kAudio,
  kFile,
  kDivider,
  kPageBreak,
  // Mermaid diagram block (ADR-017, issue #50): a custom BlockNote block spec whose inline
  // content is the raw Mermaid diagram source text, rendered to SVG client-side in
  // frontend/editor — this validator only needs to recognize the block type name and extract
  // the source text like any other inline-content block, not understand Mermaid syntax itself.
  kMermaid,
};

struct HeadingProps {
  std::int8_t level{};
};

struct Block {
  std::string id;
  BlockType type{};
  std::string text_content;
  std::optional<HeadingProps> heading_props;
  std::optional<bool> checked;
  std::vector<Block> children;
};

struct PageMetadata {
  std::string id;
  SchemaVersion schema_version{SchemaVersion::kV1};
  DocumentKind kind{DocumentKind::kWikiPage};
  std::string title;
  std::string workspace_id;
  std::optional<std::string> parent_id;
  std::int32_t sort_order{};
  std::string created_at;
  std::string updated_at;
  std::string created_by;
  std::string updated_by;
  std::int64_t content_version{1};
};

struct Document {
  PageMetadata metadata;
  std::vector<Block> blocks;
};

// Persistence key for a given kind (stored as a string, not a raw integer, so on-disk records
// stay stable across enum reordering — same convention as AccentColor's ToAccentColorKey).
[[nodiscard]] inline auto ToDocumentKindKey(DocumentKind kind) -> std::string {
  switch (kind) {
    case DocumentKind::kJupyterNotebook:
      return "jupyterNotebook";
    case DocumentKind::kExcalidrawCanvas:
      return "excalidrawCanvas";
    case DocumentKind::kWikiPage:
      return "wikiPage";
  }
  return "wikiPage";
}

// Inverse of ToDocumentKindKey(); unknown, empty, or missing keys (e.g. records written before
// this field existed) fall back to DocumentKind::kWikiPage, so existing documents round-trip
// as wiki pages unchanged.
[[nodiscard]] inline auto DocumentKindFromKey(const std::string& key) -> DocumentKind {
  if (key == "jupyterNotebook") {
    return DocumentKind::kJupyterNotebook;
  }
  if (key == "excalidrawCanvas") {
    return DocumentKind::kExcalidrawCanvas;
  }
  return DocumentKind::kWikiPage;
}

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_DOCUMENT_H_
