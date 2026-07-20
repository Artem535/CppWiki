import "@excalidraw/excalidraw/index.css";

import { Excalidraw } from "@excalidraw/excalidraw";
import { useEffect, useMemo, useRef } from "react";
import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import { buildExcalidrawScene, parseExcalidrawSceneJson } from "./excalidrawScene";

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
//   - loadScene ("Open" — import a .excalidraw file) — replaced by CppWiki's native "Import
//     .excalidraw" control (issue #96: now lives in MainWindow's top-level UI, not this
//     component — see Page::ImportCurrentDocumentFromFile() in page.cc), which calls
//     EditorBridge.importTextFromFile() (QFileDialog-backed) directly from C++.
//   - export.saveFileToDisk (the "Export image" dialog's own save-to-disk button) — image
//     export (PNG/SVG) and copy-to-clipboard still work as normal (those don't depend on the
//     File System Access API); only the broken save-to-disk sub-affordance is turned off.
//     Scene (not image) export is likewise covered by the native "Export .excalidraw" control.
const kExcalidrawUiOptions = {
  canvasActions: {
    saveToActiveFile: false,
    loadScene: false,
    export: { saveFileToDisk: false },
  },
};

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
  const latestSceneRef = useRef(rawContent);

  // Excalidraw's `initialData` is only read once at mount, so this only needs to be recomputed
  // when the open document changes (main.tsx remounts this component via `key` on document
  // switch AND on a same-document reload — see main.tsx's documentLoadNonce, issue #96 — but
  // recompute defensively if rawContent changes without a remount too).
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
      const scene = buildExcalidrawScene(elements, appState, files);
      latestSceneRef.current = JSON.stringify(scene);
      void activeBridge.updateSnapshot(documentId, scene);
    }, snapshotDebounceMs);
  };

  useEffect(() => {
    const flushForExport = () => {
      const activeBridge = bridgeRef.current;
      if (!activeBridge || !isEditable || !latestSceneRef.current) return;
      if (saveTimer.current !== null) {
        window.clearTimeout(saveTimer.current);
        saveTimer.current = null;
      }
      void activeBridge.updateSnapshot(documentId, JSON.parse(latestSceneRef.current));
    };
    window.addEventListener("cppwiki-export-current-document", flushForExport);
    return () => window.removeEventListener("cppwiki-export-current-document", flushForExport);
  }, [documentId, isEditable]);

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
      />
    </div>
  );
}
