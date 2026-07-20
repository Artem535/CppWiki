import "@excalidraw/excalidraw/index.css";

import { Excalidraw } from "@excalidraw/excalidraw";
import type { ExcalidrawImperativeAPI } from "@excalidraw/excalidraw/types";
import { useEffect, useMemo, useRef, useState } from "react";
import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import { buildExcalidrawScene, parseExcalidrawSceneJson, serializeExcalidrawScene } from "./excalidrawScene";

// Issue #82 audit: @excalidraw/excalidraw's built-in canvasActions, and which are kept vs.
// disabled here, and why.
//
// Kept enabled (self-contained, don't touch CppWiki's document model or this embedding's
// broken native-file-picker plumbing — see the crash note below):
//   - changeViewBackgroundColor, clearCanvas, toggleTheme: pure canvas/appState edits, already
//     covered by the normal onChange()/updateSnapshot() persistence path.
//   - The shape/element "library" (saving/loading reusable shape sets, left at its default —
//     no `library`/`UIOptions` restriction below): Excalidraw persists it to its own local
//     IndexedDB storage, entirely separate from CppWiki's document/sync model, so there's
//     nothing to wire up and no conflict with ADR-010/ADR-013 to worry about. Known caveat,
//     not fixed here: "Browse more libraries" opens libraries.excalidraw.com, which needs
//     network access this app doesn't otherwise require and may not behave well inside
//     QWebEngineView's single-window embedding (no real popup/tab support) — worth revisiting
//     if it turns out to matter in practice.
//
// Disabled here (all three ultimately call the File System Access API —
// window.showSaveFilePicker()/showOpenFilePicker() — which is present in this QtWebEngine
// build but has no working native picker/portal backend behind it: reproduced (issue #82)
// that calling it terminates the whole application, not just the page; see page.cc's
// fileSystemAccessRequested handling, which now always rejects the request so it fails
// gracefully instead of crashing):
//   - saveToActiveFile ("Save" / Ctrl+S to a previously-picked file handle) — CppWiki has no
//     concept of an externally-picked "active file" for a document it owns and syncs; this
//     would always be a no-op even if it worked.
//   - loadScene ("Open" — import a .excalidraw file) — replaced below by a CppWiki-native
//     "Import .excalidraw" button (EditorBridge.importTextFromFile(), QFileDialog-backed).
//   - export.saveFileToDisk (the "Export image" dialog's own save-to-disk button) — image
//     export (PNG/SVG) and copy-to-clipboard still work as normal (those don't depend on the
//     File System Access API); only the broken save-to-disk sub-affordance is turned off.
//     Scene (not image) export is covered by the "Export .excalidraw" button below.
const kExcalidrawUiOptions = {
  canvasActions: {
    saveToActiveFile: false,
    loadScene: false,
    export: { saveFileToDisk: false },
  },
};

const kExcalidrawFileFilter = "Excalidraw scene (*.excalidraw)";

type ExcalidrawCanvasViewProps = {
  // Identifies the open document, purely so the debounce/save wiring below resets cleanly when
  // the user switches to a different canvas document (see the `key={documentId}` remount at the
  // call site in main.tsx, which also achieves this — kept here too as a defensive guard).
  documentId: string;
  rawContent: string | undefined;
  isEditable: boolean;
  bridge: EditorBridge | null;
};

/**
 * Renders/edits a `kExcalidrawCanvas` document via the official @excalidraw/excalidraw
 * component (issue #53). Unlike the Jupyter notebook kind (#52), this is full editing from v1:
 * drawing is the entire point of the content type. Scene changes are debounced the same way
 * main.tsx debounces BlockNote's snapshot saves, then persisted through the existing
 * EditorBridge.updateSnapshot()/DocumentValidator/sync pathway unchanged (ADR-010/ADR-013) —
 * Excalidraw's scene JSON is just a different snapshot shape, not a separate save path.
 */
export function ExcalidrawCanvasView({
  documentId,
  rawContent,
  isEditable,
  bridge,
}: ExcalidrawCanvasViewProps) {
  const saveTimer = useRef<number | null>(null);
  const bridgeRef = useRef(bridge);
  bridgeRef.current = bridge;
  const containerRef = useRef<HTMLDivElement | null>(null);
  const excalidrawApiRef = useRef<ExcalidrawImperativeAPI | null>(null);
  const [importExportStatus, setImportExportStatus] = useState<string | null>(null);

  // Excalidraw's `initialData` is only read once at mount, so this only needs to be recomputed
  // when the open document changes (main.tsx remounts this component via `key={documentId}` on
  // document switch, but recompute defensively if rawContent changes without a remount too).
  const initialData = useMemo(() => {
    const scene = parseExcalidrawSceneJson(rawContent);
    if (!scene) {
      return null;
    }
    return {
      elements: scene.elements as never,
      appState: {
        viewBackgroundColor: scene.appState.viewBackgroundColor,
        gridSize: scene.appState.gridSize ?? undefined,
      },
      files: scene.files as never,
    };
  }, [rawContent]);

  useEffect(() => {
    return () => {
      if (saveTimer.current !== null) {
        window.clearTimeout(saveTimer.current);
        saveTimer.current = null;
      }
    };
  }, [documentId]);

  const handleChange: NonNullable<
    React.ComponentProps<typeof Excalidraw>["onChange"]
  > = (elements, appState, files) => {
    if (!isEditable) {
      return;
    }

    if (saveTimer.current !== null) {
      window.clearTimeout(saveTimer.current);
    }

    saveTimer.current = window.setTimeout(() => {
      saveTimer.current = null;
      const activeBridge = bridgeRef.current;
      if (!activeBridge) {
        return;
      }
      void activeBridge.updateSnapshot(documentId, buildExcalidrawScene(elements, appState, files));
    }, snapshotDebounceMs);
  };

  // Native "Export .excalidraw" (issue #82): the stored snapshot shape (see excalidrawScene.ts)
  // is already Excalidraw's own `{type, version, elements, appState, files}` file format, so
  // export is a straight file-save of the current in-memory scene through
  // EditorBridge.exportTextToFile() — see the UIOptions comment above for why this replaces
  // Excalidraw's own (broken in this embedding) save-to-disk affordance instead of just
  // re-enabling it.
  const handleExport = async () => {
    const api = excalidrawApiRef.current;
    if (!api || !bridgeRef.current) {
      return;
    }
    const json = serializeExcalidrawScene(api.getSceneElements(), api.getAppState(), api.getFiles());
    const response = await bridgeRef.current.exportTextToFile(
      `${documentId}.excalidraw`,
      kExcalidrawFileFilter,
      json,
    );
    if (!response.ok) {
      if (response.error.code !== "cancelled") {
        setImportExportStatus(`Export failed: ${response.error.message}`);
      }
      return;
    }
    setImportExportStatus(`Exported to ${response.result.fileName}`);
  };

  // Native "Import .excalidraw" (issue #82): loads a scene file into the *currently open*
  // canvas document via Excalidraw's imperative updateScene() API, then saves it through the
  // normal updateSnapshot() pathway like any other edit — imported content isn't a separate
  // kind of write. parseExcalidrawSceneJson() tolerates malformed/non-Excalidraw JSON by
  // falling back to a blank scene rather than throwing (same validation used for loading a
  // stored snapshot), so a bad file doesn't crash the view.
  const handleImport = async () => {
    const api = excalidrawApiRef.current;
    if (!api || !bridgeRef.current || !isEditable) {
      return;
    }
    const response = await bridgeRef.current.importTextFromFile(kExcalidrawFileFilter);
    if (!response.ok) {
      if (response.error.code !== "cancelled") {
        setImportExportStatus(`Import failed: ${response.error.message}`);
      }
      return;
    }
    const scene = parseExcalidrawSceneJson(response.result.content);
    if (!scene) {
      setImportExportStatus(`Import failed: ${response.result.fileName} is not a valid Excalidraw scene.`);
      return;
    }
    api.updateScene({ elements: scene.elements as never, appState: scene.appState as never });
    api.addFiles(Object.values(scene.files) as never);
    setImportExportStatus(`Imported ${response.result.fileName}`);
    void bridgeRef.current.updateSnapshot(
      documentId,
      buildExcalidrawScene(api.getSceneElements(), api.getAppState(), scene.files),
    );
  };

  useEffect(() => {
    if (importExportStatus === null) {
      return;
    }
    const timer = window.setTimeout(() => setImportExportStatus(null), 4000);
    return () => window.clearTimeout(timer);
  }, [importExportStatus]);

  return (
    // Sizing comes entirely from the `.excalidraw-canvas` CSS class (position: absolute; inset:
    // 0, anchored to .editor-pane) — see styles.css for why: Excalidraw measures its container
    // via ResizeObserver, and `100vh` doesn't reliably resolve inside this app's embedded
    // QWebEngineView (confirmed via devtools: computed width was correct but height stuck at 0
    // regardless of the CSS height value used here).
    <div ref={containerRef} className="excalidraw-canvas" data-testid="excalidraw-canvas-view">
      <Excalidraw
        initialData={initialData}
        onChange={handleChange}
        viewModeEnabled={!isEditable}
        UIOptions={kExcalidrawUiOptions}
        excalidrawAPI={(api) => {
          excalidrawApiRef.current = api;
        }}
        renderTopRightUI={() => (
          <div className="excalidraw-file-actions">
            {importExportStatus ? (
              <span className="excalidraw-file-actions-status">{importExportStatus}</span>
            ) : null}
            {isEditable ? (
              <button
                type="button"
                className="excalidraw-file-actions-button"
                onClick={() => void handleImport()}
              >
                Import .excalidraw
              </button>
            ) : null}
            <button
              type="button"
              className="excalidraw-file-actions-button"
              onClick={() => void handleExport()}
            >
              Export .excalidraw
            </button>
          </div>
        )}
      />
    </div>
  );
}
