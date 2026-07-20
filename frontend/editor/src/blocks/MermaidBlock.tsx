// Mermaid diagram block (ADR-017, issue #50): a custom BlockNote block spec whose content is
// the raw Mermaid diagram source text (`content: "inline"`, same content model as codeBlock —
// see @blocknote/core's Code/block.ts), so the C++ validator's existing inline-content
// extraction (document_validator.cc's ExtractInlineText) and this repo's sync/lock/conflict
// pipeline handle it exactly like any other text-bearing block, with no format-specific
// awareness needed anywhere except this renderer.
//
// Chosen over a codeBlock-with-language=="mermaid" special case: a dedicated block type gets its
// own slash-menu entry (see getMermaidSlashMenuItem below) instead of requiring a user to know
// to type ```mermaid, which is both more discoverable and keeps "does this block render as a
// diagram" a first-class, inspectable property of the block itself rather than a prop value a
// future change to codeBlock's language list could silently break.
import type { BlockNoteEditor, PropSchema } from "@blocknote/core";
import { insertOrUpdateBlock } from "@blocknote/core";
import { useEffect, useRef, useState } from "react";

import type { DefaultReactSuggestionItem } from "@blocknote/react";

import { createReactBlockSpec, type ReactCustomBlockRenderProps } from "@blocknote/react";

import { mermaidRenderDebounceMs } from "../constants";
import { renderMermaidToSafeSvg } from "./mermaidRenderer";
import {
  defaultMermaidSource,
  extractMermaidSource,
  mermaidBlockType,
  type MermaidInlineContentItem,
} from "./mermaidSource";

const mermaidPropSchema = {} satisfies PropSchema;

type MermaidRenderState =
  | { status: "pending" }
  | { status: "rendering" }
  | { status: "ok"; svg: string }
  | { status: "error"; message: string }
  | { status: "empty" };

function MermaidIcon() {
  // Small inline SVG rather than pulling in react-icons (a transitive-only dependency here,
  // same reasoning as pinning `mermaid` itself as a direct dependency in package.json — don't
  // rely on another package's undeclared transitive dependency staying resolvable).
  return (
    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <circle cx="5" cy="6" r="2.5" stroke="currentColor" strokeWidth="1.5" />
      <circle cx="19" cy="6" r="2.5" stroke="currentColor" strokeWidth="1.5" />
      <circle cx="12" cy="18" r="2.5" stroke="currentColor" strokeWidth="1.5" />
      <path
        d="M7.2 7.2L10 16M16.8 7.2L14 16"
        stroke="currentColor"
        strokeWidth="1.5"
        strokeLinecap="round"
      />
    </svg>
  );
}

function MermaidBlockContent(
  props: ReactCustomBlockRenderProps<
    typeof mermaidBlockType,
    typeof mermaidPropSchema,
    "inline"
  >,
) {
  // props.block.content's precise type depends on BlockNote's full block-schema generics
  // (union across every registered block type's content shape); duck-typed against
  // MermaidInlineContentItem instead of fighting that union, since this block only ever
  // registers "inline" content (plain StyledText/Link runs, see mermaidBlockType's config).
  const source = extractMermaidSource(
    props.block.content as MermaidInlineContentItem[] | undefined,
  );
  const [renderState, setRenderState] = useState<MermaidRenderState>({ status: "pending" });
  const debounceTimer = useRef<number | null>(null);

  // Re-renders the diagram a short debounce after the source text last changed (same
  // setTimeout/clearTimeout debounce pattern used for autosave in main.tsx and
  // NotebookView.tsx's scheduleSave) — mermaid.render() does real layout work, so re-running it
  // on every keystroke would be wasteful and janky while typing a diagram definition.
  useEffect(() => {
    if (debounceTimer.current !== null) {
      window.clearTimeout(debounceTimer.current);
      debounceTimer.current = null;
    }

    if (!source.trim()) {
      setRenderState({ status: "empty" });
      return;
    }

    setRenderState({ status: "rendering" });
    debounceTimer.current = window.setTimeout(() => {
      debounceTimer.current = null;
      void renderMermaidToSafeSvg(props.block.id, source).then((result) => {
        setRenderState(
          result.ok
            ? { status: "ok", svg: result.svg }
            : { status: "error", message: result.error },
        );
      });
    }, mermaidRenderDebounceMs);

    return () => {
      if (debounceTimer.current !== null) {
        window.clearTimeout(debounceTimer.current);
        debounceTimer.current = null;
      }
    };
  }, [source, props.block.id]);

  return (
    <div className="mermaid-block" data-testid="mermaid-block">
      {/* The editable raw diagram source. This div (via contentRef) becomes BlockNote's
          ProseMirror contentDOM for the block — the same role @blocknote/core's codeBlock gives
          its <code> element (see Code/block.ts's render()); everything else rendered here is a
          plain sibling, not part of the editable content model. */}
      <div
        className="mermaid-block-source"
        ref={props.contentRef}
        data-placeholder={source ? undefined : "Type a Mermaid diagram, e.g. graph TD; A-->B;"}
      />
      <div className="mermaid-block-preview" contentEditable={false}>
        {renderState.status === "ok" ? (
          // Sanitized via DOMPurify inside renderMermaidToSafeSvg (mermaidRenderer.ts), the
          // same pattern notebook/NotebookView.tsx's OutputView uses for text/html cell output.
          // eslint-disable-next-line react/no-danger -- sanitized via DOMPurify, see mermaidRenderer.ts.
          <div
            className="mermaid-block-diagram"
            dangerouslySetInnerHTML={{ __html: renderState.svg }}
          />
        ) : renderState.status === "error" ? (
          <p className="mermaid-block-error">Could not render diagram: {renderState.message}</p>
        ) : renderState.status === "empty" ? (
          <p className="mermaid-block-placeholder">Diagram preview will appear here.</p>
        ) : (
          <p className="mermaid-block-placeholder">Rendering diagram…</p>
        )}
      </div>
    </div>
  );
}

export const MermaidBlock = createReactBlockSpec(
  {
    type: mermaidBlockType,
    propSchema: mermaidPropSchema,
    content: "inline",
  } as const,
  {
    render: MermaidBlockContent,
  },
);

// BlockNoteEditor's block-schema generics make a precisely-typed signature here unwieldy (see
// @blocknote/react's own ImageBlock.tsx/ResizableFileBlockWrapper.tsx, which cast through `any`
// for the same reason); `insertOrUpdateBlock`'s own parameter types still catch a typo'd block
// shape at the call site below.
export function getMermaidSlashMenuItem(
  // eslint-disable-next-line @typescript-eslint/no-explicit-any -- see comment above.
  editor: BlockNoteEditor<any, any, any>,
): DefaultReactSuggestionItem {
  return {
    title: "Mermaid Diagram",
    subtext: "An editable diagram rendered from Mermaid syntax",
    aliases: ["mermaid", "diagram", "flowchart", "sequence", "gantt", "chart"],
    group: "Media",
    icon: <MermaidIcon />,
    onItemClick: () => {
      insertOrUpdateBlock(editor, {
        type: mermaidBlockType,
        content: defaultMermaidSource,
      });
    },
  };
}
