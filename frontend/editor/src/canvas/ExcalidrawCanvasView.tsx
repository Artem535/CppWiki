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
  snapshotJson: string | undefined;
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
  snapshotJson,
  isEditable,
  bridge,
}: ExcalidrawCanvasViewProps) {
  const saveTimer = useRef<number | null>(null);
  const bridgeRef = useRef(bridge);
  bridgeRef.current = bridge;

  // Excalidraw's `initialData` is only read once at mount, so this only needs to be recomputed
  // when the open document changes (main.tsx remounts this component via `key={documentId}` on
  // document switch, but recompute defensively if snapshotJson changes without a remount too).
  const initialData = useMemo(() => {
    const scene = parseExcalidrawSceneJson(snapshotJson);
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
  }, [snapshotJson]);

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
      void activeBridge.updateSnapshot(buildExcalidrawScene(elements, appState, files));
    }, snapshotDebounceMs);
  };

  return (
    <div className="excalidraw-canvas" data-testid="excalidraw-canvas-view" style={{ height: "100%", width: "100%" }}>
      <Excalidraw
        initialData={initialData}
        onChange={handleChange}
        viewModeEnabled={!isEditable}
      />
    </div>
  );
}
