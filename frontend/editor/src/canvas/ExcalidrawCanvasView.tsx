import "@excalidraw/excalidraw/index.css";

import { Excalidraw } from "@excalidraw/excalidraw";
import { useEffect, useMemo, useRef } from "react";
import type { EditorBridge } from "../bridge/editorBridge";
import { snapshotDebounceMs } from "../constants";
import { buildExcalidrawScene, parseExcalidrawSceneJson } from "./excalidrawScene";

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
      />
    </div>
  );
}
