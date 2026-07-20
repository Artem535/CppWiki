// Browser-only Mermaid rendering (ADR-017, issue #50). Not unit tested — same convention as
// canvas/ExcalidrawCanvasView.tsx, whose actual rendering also depends on a real DOM/canvas
// and only has its pure data-shaping logic (excalidrawScene.ts) covered by Vitest. mermaid.js
// itself creates/measures real DOM nodes internally, so it cannot run under Vitest's default
// node environment without adding a jsdom dependency this repo doesn't otherwise need.
import DOMPurify from "dompurify";
import mermaid from "mermaid";

let initialized = false;

// mermaid.initialize() is a one-time, module-global call (per mermaid's own docs) — calling it
// again per-render is unnecessary and would reset any diagram-scoped caching mermaid does
// internally. securityLevel: "strict" is mermaid's own sanitizing mode (escapes/limits what
// diagram source can inject, e.g. disables `click` interactions and raw HTML in labels); we
// still re-sanitize the resulting SVG with DOMPurify below rather than relying on it alone,
// matching this app's existing pattern for rendering foreign markup (see
// notebook/NotebookView.tsx's OutputView, which does the same for text/html cell output).
//
// htmlLabels: false (issue #95) turns off mermaid's default HTML-in-SVG node/edge label
// rendering (`<foreignObject><div>...</div></foreignObject>`) in favor of plain SVG
// `<text>/<tspan>` for every diagram type. This was chosen over trying to make sanitizeSvg()
// below preserve foreignObject content correctly: verified empirically (standalone
// mermaid+DOMPurify+jsdom repro, not committed here) that DOMPurify strips a foreignObject's
// nested HTML regardless of ADD_ATTR (e.g. `xmlns`) tweaks — the html/svg profile mismatch
// isn't fixable by attribute allowlisting alone — while htmlLabels: false removes the
// foreignObject/HTML content entirely, so there's nothing sanitization-sensitive left to strip.
// Must be set at the top level, not nested under `flowchart:` — mermaid 11.x's flowchart
// renderer reads `config.htmlLabels` (top-level) in some code paths and the deprecated
// `config.flowchart.htmlLabels` in others, so only the nested key is unreliable.
function ensureInitialized(): void {
  if (initialized) {
    return;
  }
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: "strict",
    theme: "dark",
    fontFamily: "inherit",
    htmlLabels: false,
  });
  initialized = true;
}

// mermaid.render() requires an id that's unique per call and valid as an SVG/HTML element id
// (must start with a letter). A block id alone is not sufficient: reopening a document renders
// the same block again, and Mermaid retains per-id state while it builds its temporary DOM. That
// made a persisted diagram appear blank even though its source had been loaded correctly.
let renderSequence = 0;

function toRenderId(blockId: string): string {
  const sanitized = blockId.replace(/[^a-zA-Z0-9_-]/g, "-");
  renderSequence += 1;
  return `cppwiki-mermaid-${sanitized || "block"}-${renderSequence}`;
}

// DOMPurify's SVG profile alone strips <foreignObject> (used by mermaid's default HTML-based
// node labels), which would silently blank out most flowchart/sequence-diagram text. Allow it
// back in explicitly, together with the html profile for its contents, while DOMPurify's
// built-in forbidden list (script tags, on*= handlers, javascript: URIs, etc. — applied
// regardless of USE_PROFILES) still strips anything actually dangerous.
function sanitizeSvg(svg: string): string {
  return DOMPurify.sanitize(svg, {
    USE_PROFILES: { svg: true, svgFilters: true, html: true },
    ADD_TAGS: ["foreignObject"],
  });
}

export type MermaidRenderResult =
  | { ok: true; svg: string }
  | { ok: false; error: string };

// Renders Mermaid diagram source to a sanitized SVG string. Never throws — mermaid.render()
// rejects on invalid diagram syntax, which is a normal, expected state while a user is mid-edit
// (see MermaidBlock.tsx), not a bug to propagate as an unhandled rejection.
export async function renderMermaidToSafeSvg(
  blockId: string,
  source: string,
): Promise<MermaidRenderResult> {
  const trimmed = source.trim();
  if (!trimmed) {
    return { ok: false, error: "Diagram source is empty." };
  }

  ensureInitialized();
  const renderId = toRenderId(blockId);

  try {
    const { svg } = await mermaid.render(renderId, trimmed);
    return { ok: true, svg: sanitizeSvg(svg) };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    return { ok: false, error: message };
  } finally {
    // mermaid.render() appends a temporary `d${id}`-id'd div to document.body for its internal
    // layout/measurement step and removes it itself on success and on parse failure — but not
    // when its diagram-drawing step throws (mermaid.core.mjs's render(), the `diag.renderer.draw`
    // catch branch). Clean it up defensively so a draw failure doesn't leave it behind across
    // repeated re-renders while a diagram is being edited.
    document.getElementById(`d${renderId}`)?.remove();
  }
}
