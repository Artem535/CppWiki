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

// Small chevron, rotated via CSS depending on collapsed state (see .mermaid-block-toggle-icon
// below) rather than swapping between two separate icon markups.
function ChevronIcon() {
  return (
    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <path
        d="M6 9l6 6 6-6"
        stroke="currentColor"
        strokeWidth="2"
        strokeLinecap="round"
        strokeLinejoin="round"
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
  // Collapse/expand affordance for the raw source (issue #95): once a diagram has rendered
  // successfully, the raw text stays permanently visible above it by default otherwise, wasting
  // space for a "finished" diagram. Manual toggle only (no auto-collapse-on-success) — collapsing
  // out from under a user who's still actively editing the source would be surprising, and this
  // re-runs on every keystroke via the debounced render effect below.
  //
  // Deliberately CSS-only visual collapsing (max-height/opacity, see .mermaid-block-source
  // --collapsed in styles.css): props.contentRef below is BlockNote's actual ProseMirror
  // contentDOM node for this block. Unmounting it (conditional render) or `display: none`-ing it
  // would detach it from the DOM the ProseMirror view expects to keep measuring, breaking
  // editability/undo. Keeping it mounted with zero clipped height is the same trick BlockNote's
  // own toggleListItem-style "collapsed content" patterns and most collapsible-code-block widgets
  // use for exactly this reason.
  const [isSourceCollapsed, setIsSourceCollapsed] = useState(false);

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

  // Only offer the collapse toggle once there's a rendered diagram to collapse the source in
  // favor of — while empty/pending/erroring, the user needs the source visible to write or fix it.
  const canCollapseSource = renderState.status === "ok";
  const sourceIsCollapsed = canCollapseSource && isSourceCollapsed;

  return (
    <div className="mermaid-block" data-testid="mermaid-block">
      <div className="mermaid-block-toolbar" contentEditable={false}>
        <span className="mermaid-block-toolbar-label">Mermaid source</span>
        {canCollapseSource ? (
          <button
            type="button"
            className="mermaid-block-toggle"
            aria-expanded={!sourceIsCollapsed}
            data-testid="mermaid-block-toggle-source"
            onClick={() => setIsSourceCollapsed((collapsed) => !collapsed)}
          >
            <span
              className={`mermaid-block-toggle-icon${
                sourceIsCollapsed ? " mermaid-block-toggle-icon--collapsed" : ""
              }`}
            >
              <ChevronIcon />
            </span>
            {sourceIsCollapsed ? "Show source" : "Hide source"}
          </button>
        ) : null}
      </div>
      {/* The editable raw diagram source. This div (via contentRef) becomes BlockNote's
          ProseMirror contentDOM for the block — the same role @blocknote/core's codeBlock gives
          its <code> element (see Code/block.ts's render()); everything else rendered here is a
          plain sibling, not part of the editable content model. Stays mounted at all times (see
          isSourceCollapsed above) — only its CSS class toggles, via
          mermaid-block-source--collapsed in styles.css. */}
      <div
        className={`mermaid-block-source${
          sourceIsCollapsed ? " mermaid-block-source--collapsed" : ""
        }`}
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
    // Mermaid source is inherently multi-line. BlockNote's ordinary inline blocks split into a
    // new paragraph on Enter, so make Enter insert a literal newline in this block just as it
    // does in BlockNote's built-in codeBlock. The newline is then part of the regular text item
    // and therefore reaches updateSnapshot() and the file repository unchanged.
    meta: { hardBreakShortcut: "enter" },
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
