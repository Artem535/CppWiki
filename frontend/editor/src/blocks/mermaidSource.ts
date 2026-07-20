// Pure helpers for the Mermaid diagram block (ADR-017, issue #50). Kept dependency-free (no
// BlockNote/mermaid.js/DOM imports) so it can run under Vitest's default node environment,
// mirroring canvas/excalidrawScene.ts's split between pure logic (tested) and the
// browser-only rendering component (not unit tested, see MermaidBlock.tsx).

export const mermaidBlockType = "mermaid" as const;

// Seeded into a freshly inserted block (see getMermaidSlashMenuItem in MermaidBlock.tsx) so the
// diagram renders something meaningful immediately instead of an empty/error state.
export const defaultMermaidSource = "graph TD\n    A[Start] --> B{Decision}\n    B -->|Yes| C[Do the thing]\n    B -->|No| D[Skip it]";

// Loosely-typed mirror of BlockNote's inline content shape (StyledText | Link, see
// @blocknote/core's schema/inlineContent/types.ts) — duck-typed rather than imported so this
// module stays independent of BlockNote's generics. Matches the same shape the C++ side's
// ExtractInlineText (document_validator.cc) walks over the wire.
export type MermaidInlineContentItem = {
  type?: string;
  text?: string;
  content?: readonly MermaidInlineContentItem[];
};

// Concatenates every text run's characters, including text nested inside link items — same
// "just the characters, no formatting" extraction the C++ validator does for every other
// inline-content block type, so a Mermaid block's saved text_content stays meaningful (e.g. for
// future search/indexing) without this module needing to understand Mermaid syntax at all.
export function extractMermaidSource(
  content: readonly MermaidInlineContentItem[] | undefined,
): string {
  if (!content) {
    return "";
  }

  let text = "";
  for (const item of content) {
    if (typeof item.text === "string") {
      text += item.text;
    }
    if (item.content) {
      text += extractMermaidSource(item.content);
    }
  }
  return text;
}
