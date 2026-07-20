// Jupyter notebook document kind renderer (issue #52, ADR-017): view/edit cell source and view
// already-saved cell output. No kernel connectivity, no code execution — permanently out of
// scope, not just a v1 cut, so this component must never grow a "Run cell" affordance.
import DOMPurify from "dompurify";
import { useEffect, useMemo, useRef, useState } from "react";

import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import {
  joinNbSource,
  parseNotebookJson,
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
  onSourceChange,
  onDeleteCell,
}: {
  cell: NbCell;
  index: number;
  editable: boolean;
  onSourceChange: (index: number, source: string) => void;
  onDeleteCell: (index: number) => void;
}) {
  const sourceText = joinNbSource(cell.source);
  const isCode = cell.cell_type === "code";

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
      <textarea
        className={`notebook-cell-source${isCode ? " notebook-cell-source--code" : ""}`}
        value={sourceText}
        readOnly={!editable}
        spellCheck={!isCode}
        onChange={(event) => onSourceChange(index, event.target.value)}
        rows={Math.max(2, sourceText.split("\n").length)}
      />
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

const kIpynbFileFilter = "Jupyter Notebook (*.ipynb)";

// Native .ipynb import/export (issue #82), via the same EditorBridge.exportTextToFile()/
// importTextFromFile() QFileDialog-backed pair the Excalidraw canvas kind uses — see
// ExcalidrawCanvasView.tsx's UIOptions comment for why this app builds its own native file
// dialogs rather than relying on any browser-native affordance. The notebook's stored snapshot
// is already raw nbformat JSON (see nbformat.ts), so export is a straight file-save of the
// current in-memory notebook object and import is parseNotebookJson() (the same
// well-formedness check used when loading a stored snapshot — a malformed file falls back to
// "could not read notebook" rather than crashing the view) followed by the normal
// updateSnapshot() save path.
function FileActionsToolbar({
  editable,
  status,
  onExport,
  onImport,
}: {
  editable: boolean;
  status: string | null;
  onExport: () => void;
  onImport: () => void;
}) {
  return (
    <div className="notebook-file-actions">
      {status ? <span className="notebook-file-actions-status">{status}</span> : null}
      {editable ? (
        <button type="button" className="notebook-file-actions-button" onClick={onImport}>
          Import .ipynb
        </button>
      ) : null}
      <button type="button" className="notebook-file-actions-button" onClick={onExport}>
        Export .ipynb
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
  const [fileActionStatus, setFileActionStatus] = useState<string | null>(null);
  const snapshot_timer = useRef<number | null>(null);
  const loaded_page_id = useRef<string | null>(null);
  const notebookRef = useRef(notebook);
  notebookRef.current = notebook;

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

  const handleExportNotebook = async () => {
    if (!bridge) {
      return;
    }
    const current = notebookRef.current ?? { cells: [], nbformat: 4, nbformat_minor: 5 };
    const response = await bridge.exportTextToFile(
      `${pageId}.ipynb`,
      kIpynbFileFilter,
      JSON.stringify(current, null, 1),
    );
    if (!response.ok) {
      if (response.error.code !== "cancelled") {
        setFileActionStatus(`Export failed: ${response.error.message}`);
      }
      return;
    }
    setFileActionStatus(`Exported to ${response.result.fileName}`);
  };

  const handleImportNotebook = async () => {
    if (!bridge || !editable) {
      return;
    }
    const response = await bridge.importTextFromFile(kIpynbFileFilter);
    if (!response.ok) {
      if (response.error.code !== "cancelled") {
        setFileActionStatus(`Import failed: ${response.error.message}`);
      }
      return;
    }
    const parsed = parseNotebookJson(response.result.content);
    if (!parsed) {
      setFileActionStatus(`Import failed: ${response.result.fileName} is not valid nbformat JSON.`);
      return;
    }
    setNotebook(parsed);
    setParseFailed(false);
    setFileActionStatus(`Imported ${response.result.fileName}`);
    scheduleSave(parsed);
  };

  useEffect(() => {
    if (fileActionStatus === null) {
      return;
    }
    const timer = window.setTimeout(() => setFileActionStatus(null), 4000);
    return () => window.clearTimeout(timer);
  }, [fileActionStatus]);

  const cells = useMemo(() => notebook?.cells ?? [], [notebook]);

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
      <FileActionsToolbar
        editable={editable}
        status={fileActionStatus}
        onExport={() => void handleExportNotebook()}
        onImport={() => void handleImportNotebook()}
      />
      {cells.length === 0 ? (
        <div className="empty-state">
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
            onSourceChange={handleSourceChange}
            onDeleteCell={handleDeleteCell}
          />
        ))
      )}
      {editable ? <AddCellToolbar onAddCell={handleAddCell} /> : null}
    </div>
  );
}
