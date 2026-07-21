// Jupyter notebook document kind renderer (issue #52, ADR-017): view/edit cell source and view
// already-saved cell output. No kernel connectivity, no code execution — permanently out of
// scope, not just a v1 cut, so this component must never grow a "Run cell" affordance.
import DOMPurify from "dompurify";
import { useEffect, useMemo, useRef, useState } from "react";
import type { KeyboardEvent } from "react";

import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import { CodeCellEditor } from "./CodeCellEditor";
import { renderMarkdownToHtml } from "./markdown";
import {
  joinNbSource,
  parseNotebookJson,
  resolveKernelLanguage,
  resolveOutputData,
  splitToNbSource,
  type NbCell,
  type NbDataBundle,
  type NbErrorOutput,
  type NbNotebook,
  type NbOutput,
  type NbSource,
} from "./nbformat";

function OutputView({ output }: { output: NbOutput }) {
  if (output.output_type === "stream") {
    const streamText = (output as { text?: NbSource }).text;
    return <pre className="notebook-output notebook-output--stream">{joinNbSource(streamText)}</pre>;
  }

  if (output.output_type === "execute_result" || output.output_type === "display_data") {
    const data = (output as { data?: NbDataBundle }).data;
    const resolved = resolveOutputData(data);
    if (!resolved) {
      return null;
    }
    if (resolved.mime === "image/png") {
      return (
        <img
          className="notebook-output notebook-output--image"
          src={`data:image/png;base64,${resolved.value}`}
          alt="Cell output"
        />
      );
    }
    if (resolved.mime === "text/html") {
      const sanitized = DOMPurify.sanitize(resolved.value);
      return (
        // eslint-disable-next-line react/no-danger -- sanitized via DOMPurify above.
        <div
          className="notebook-output notebook-output--html"
          dangerouslySetInnerHTML={{ __html: sanitized }}
        />
      );
    }
    return <pre className="notebook-output notebook-output--text">{resolved.value}</pre>;
  }

  if (output.output_type === "error") {
    const errorOutput = output as NbErrorOutput;
    const traceback = (errorOutput.traceback ?? []).join("\n");
    return (
      <pre className="notebook-output notebook-output--error">
        {traceback || `${errorOutput.ename ?? "Error"}: ${errorOutput.evalue ?? ""}`}
      </pre>
    );
  }

  return null;
}

function CellView({
  cell,
  index,
  editable,
  notebookLanguage,
  onSourceChange,
  onDeleteCell,
}: {
  cell: NbCell;
  index: number;
  editable: boolean;
  notebookLanguage: string;
  onSourceChange: (index: number, source: string) => void;
  onDeleteCell: (index: number) => void;
}) {
  const sourceText = joinNbSource(cell.source);
  const isCode = cell.cell_type === "code";
  const isMarkdown = cell.cell_type === "markdown";

  // Per-cell rendered/raw-source toggle for markdown cells (issue #89): local, not lifted into
  // notebook state, since different cells can independently be rendered or being edited. Code
  // cells never set this — they have no rendered mode at all, only raw source.
  //
  // Defaults to rendered for markdown cells (issue #108): a freshly opened notebook should show
  // markdown cells the way real Jupyter shows an already-executed notebook — rendered output
  // only, never the raw source box underneath. Clicking the rendered view (or pressing Enter/Space
  // on it, below) still flips back to the raw textarea for editing; this only changes the
  // *initial* state on open, not the toggle itself.
  const [isRendered, setIsRendered] = useState(isMarkdown);

  const handleSourceKeyDown = (event: KeyboardEvent<HTMLTextAreaElement>) => {
    if (isMarkdown && event.key === "Enter" && event.shiftKey) {
      // Mirrors classic Jupyter's Shift+Enter: switch this markdown cell to its rendered display,
      // without inserting a newline into the source.
      event.preventDefault();
      setIsRendered(true);
    }
  };

  const showRendered = isMarkdown && isRendered;

  return (
    <div className="notebook-cell" data-cell-type={cell.cell_type}>
      <div className="notebook-cell-toolbar">
        <div className="notebook-cell-kind">{isCode ? "Code" : cell.cell_type}</div>
        {editable ? (
          <button
            type="button"
            className="notebook-cell-delete"
            onClick={() => onDeleteCell(index)}
            aria-label="Delete cell"
          >
            Delete cell
          </button>
        ) : null}
      </div>
      {isCode ? (
        // Syntax-highlighted, still-editable source for code cells (issue #88) — language comes
        // from the notebook-level kernelspec/language_info (nbformat has no per-cell language),
        // resolved once by the parent via resolveKernelLanguage(). Markdown cells get their own
        // render/edit toggle below (issue #89); other cell types keep the plain textarea.
        <CodeCellEditor
          value={sourceText}
          editable={editable}
          language={notebookLanguage}
          onChange={(nextText) => onSourceChange(index, nextText)}
        />
      ) : showRendered ? (
        <div
          className="notebook-cell-markdown-rendered"
          data-testid={`notebook-cell-markdown-rendered-${index}`}
          role="button"
          tabIndex={0}
          onClick={() => setIsRendered(false)}
          onKeyDown={(event) => {
            if (event.key === "Enter" || event.key === " ") {
              event.preventDefault();
              setIsRendered(false);
            }
          }}
          // eslint-disable-next-line react/no-danger -- sanitized via DOMPurify below.
          dangerouslySetInnerHTML={{ __html: DOMPurify.sanitize(renderMarkdownToHtml(sourceText)) }}
        />
      ) : (
        <textarea
          className="notebook-cell-source"
          value={sourceText}
          readOnly={!editable}
          spellCheck
          onChange={(event) => onSourceChange(index, event.target.value)}
          onKeyDown={handleSourceKeyDown}
          rows={Math.max(2, sourceText.split("\n").length)}
        />
      )}
      {isCode && cell.outputs && cell.outputs.length > 0 ? (
        <div className="notebook-cell-outputs">
          {cell.outputs.map((output, outputIndex) => (
            // eslint-disable-next-line react/no-array-index-key -- outputs have no stable id.
            <OutputView key={outputIndex} output={output} />
          ))}
        </div>
      ) : null}
    </div>
  );
}

function AddCellToolbar({
  onAddCell,
}: {
  onAddCell: (cellType: "markdown" | "code") => void;
}) {
  return (
    <div className="notebook-add-cell-toolbar">
      <button type="button" onClick={() => onAddCell("markdown")}>
        + Markdown cell
      </button>
      <button type="button" onClick={() => onAddCell("code")}>
        + Code cell
      </button>
    </div>
  );
}

export function NotebookView({
  bridge,
  pageId,
  editable,
  rawContent,
}: {
  bridge: EditorBridge | null;
  pageId: string;
  editable: boolean;
  rawContent: string | undefined;
}) {
  const [notebook, setNotebook] = useState<NbNotebook | null>(() => parseNotebookJson(rawContent ?? ""));
  const [parseFailed, setParseFailed] = useState(false);
  const snapshot_timer = useRef<number | null>(null);
  const loaded_page_id = useRef<string | null>(null);

  // Re-parse whenever a different document (or the same one reloaded) is handed to us — mirrors
  // main.tsx's applyLoadedBlocks()/onDocumentLoaded reset for the BlockNote path.
  useEffect(() => {
    if (loaded_page_id.current === pageId) {
      return;
    }
    loaded_page_id.current = pageId;
    // NotebookView doesn't remount on document switch (only on a documentKind change does the
    // parent unmount it), so a debounced save scheduled for the previous notebook must be
    // cancelled here — otherwise it fires against the newly switched-to document's id once the
    // debounce elapses, saving stale content from the wrong notebook onto it.
    if (snapshot_timer.current !== null) {
      window.clearTimeout(snapshot_timer.current);
      snapshot_timer.current = null;
    }
    const parsed = parseNotebookJson(rawContent ?? "");
    setNotebook(parsed);
    setParseFailed(parsed === null);
  }, [pageId, rawContent]);

  useEffect(() => {
    return () => {
      if (snapshot_timer.current !== null) {
        window.clearTimeout(snapshot_timer.current);
      }
    };
  }, []);

  useEffect(() => {
    const flushForExport = () => {
      if (bridge && editable && notebook) {
        if (snapshot_timer.current !== null) {
          window.clearTimeout(snapshot_timer.current);
          snapshot_timer.current = null;
        }
        void bridge.updateSnapshot(pageId, notebook);
      }
    };
    window.addEventListener("cppwiki-export-current-document", flushForExport);
    return () => window.removeEventListener("cppwiki-export-current-document", flushForExport);
  }, [bridge, editable, notebook, pageId]);

  const scheduleSave = (next: NbNotebook) => {
    if (!bridge || !editable) {
      return;
    }
    if (snapshot_timer.current !== null) {
      window.clearTimeout(snapshot_timer.current);
    }
    snapshot_timer.current = window.setTimeout(() => {
      snapshot_timer.current = null;
      // Serializes the edited nbformat JSON as the document's snapshot, reusing the same
      // updateSnapshot()/DocumentValidator/sync/lock/conflict pipeline the BlockNote path uses
      // (see main.tsx's handleEditorChange) — no separate save pathway for notebooks.
      void bridge.updateSnapshot(pageId, next);
    }, snapshotDebounceMs);
  };

  const handleSourceChange = (index: number, sourceText: string) => {
    if (!notebook) {
      return;
    }
    const nextCells = notebook.cells.slice();
    nextCells[index] = { ...nextCells[index], source: splitToNbSource(sourceText) };
    const next = { ...notebook, cells: nextCells };
    setNotebook(next);
    scheduleSave(next);
  };

  const handleAddCell = (cellType: "markdown" | "code") => {
    // notebook may be null only when parsing failed (handled by the parseFailed early return
    // below) or, per parseNotebookJson(), for a genuinely blank rawContent — which still yields
    // {cells: [], nbformat: 4, nbformat_minor: 5}, not null. Fall back to that shape defensively.
    const base = notebook ?? { cells: [], nbformat: 4, nbformat_minor: 5 };
    const newCell: NbCell = { cell_type: cellType, source: [], metadata: {} };
    const next = { ...base, cells: [...base.cells, newCell] };
    setNotebook(next);
    scheduleSave(next);
  };

  const handleDeleteCell = (index: number) => {
    if (!notebook) {
      return;
    }
    const next = { ...notebook, cells: notebook.cells.filter((_, i) => i !== index) };
    setNotebook(next);
    scheduleSave(next);
  };

  const cells = useMemo(() => notebook?.cells ?? [], [notebook]);
  const notebookLanguage = useMemo(() => resolveKernelLanguage(notebook), [notebook]);

  if (parseFailed) {
    return (
      <div className="empty-state" data-testid="notebook-parse-error">
        <h1>Could not read notebook</h1>
        <p>The stored document is not valid nbformat JSON.</p>
      </div>
    );
  }

  return (
    <div className="notebook-view" data-testid="notebook-view">
      {cells.length === 0 ? (
        // NOT .empty-state: that class is position: absolute; inset: 0 (meant for a full-page
        // placeholder with nothing else on screen), which took this message out of .notebook-view's
        // flex flow and painted it over the AddCellToolbar rendered below — the buttons were still
        // in the DOM and clickable in theory, just visually hidden underneath this message's opaque
        // background. This needs to stay a normal in-flow block since the toolbar follows it.
        <div className="notebook-empty-cells">
          <p>This notebook has no cells yet.</p>
        </div>
      ) : (
        cells.map((cell, index) => (
          // eslint-disable-next-line react/no-array-index-key -- nbformat cells have no stable id.
          <CellView
            key={index}
            cell={cell}
            index={index}
            editable={editable}
            notebookLanguage={notebookLanguage}
            onSourceChange={handleSourceChange}
            onDeleteCell={handleDeleteCell}
          />
        ))
      )}
      {editable ? <AddCellToolbar onAddCell={handleAddCell} /> : null}
    </div>
  );
}
