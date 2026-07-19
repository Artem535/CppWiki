// Pure helpers for turning Excalidraw's scene state into the JSON string persisted via
// EditorBridge.updateSnapshot() (issue #53) and back into the shape Excalidraw's `initialData`
// prop expects. Kept side-effect free so they're unit-testable without mounting the canvas.

// Minimal shape of what we persist: Excalidraw's `elements`/`appState`/`files` from onChange().
// We don't import Excalidraw's own element/appState types here to keep this module decoupled
// from the package's (large, versioned) type surface — callers pass through whatever Excalidraw
// gives them and we only look at the handful of appState fields worth restoring.
export type ExcalidrawSceneJson = {
  type: "excalidraw";
  version: 2;
  elements: readonly unknown[];
  appState: {
    viewBackgroundColor?: string;
    gridSize?: number | null;
  };
  files: Record<string, unknown>;
};

/**
 * Builds the plain-object scene envelope from Excalidraw's onChange() payload — the shape
 * persisted as the document's snapshot content. Only a small, stable subset of appState is kept
 * (the rest — selection, scroll position, active tool, etc. — is session UI state, not document
 * content). Returns a plain object (rather than a JSON string) so callers can pass it directly
 * to EditorBridge.updateSnapshot(), which serializes it itself.
 */
export function buildExcalidrawScene(
  elements: readonly unknown[],
  appState: { viewBackgroundColor?: string; gridSize?: number | null },
  files: Record<string, unknown>,
): ExcalidrawSceneJson {
  return {
    type: "excalidraw",
    version: 2,
    elements,
    appState: {
      viewBackgroundColor: appState.viewBackgroundColor,
      gridSize: appState.gridSize ?? null,
    },
    files,
  };
}

/** JSON-string form of {@link buildExcalidrawScene}, mainly useful for tests. */
export function serializeExcalidrawScene(
  elements: readonly unknown[],
  appState: { viewBackgroundColor?: string; gridSize?: number | null },
  files: Record<string, unknown>,
): string {
  return JSON.stringify(buildExcalidrawScene(elements, appState, files));
}

/**
 * Parses a stored snapshot JSON string back into the shape Excalidraw's `initialData` prop
 * expects. Returns `null` for an empty/blank snapshot (new document) and also tolerates
 * malformed/unexpected JSON by falling back to a blank scene rather than throwing, since a
 * corrupt snapshot shouldn't prevent the canvas from opening at all.
 */
export function parseExcalidrawSceneJson(snapshotJson: string | undefined | null): {
  elements: unknown[];
  appState: { viewBackgroundColor?: string; gridSize?: number | null };
  files: Record<string, unknown>;
} | null {
  if (!snapshotJson || snapshotJson.trim().length === 0) {
    return null;
  }

  let parsed: unknown;
  try {
    parsed = JSON.parse(snapshotJson);
  } catch {
    return null;
  }

  if (typeof parsed !== "object" || parsed === null) {
    return null;
  }

  const candidate = parsed as Partial<ExcalidrawSceneJson>;
  const elements = Array.isArray(candidate.elements) ? candidate.elements : [];
  const appState =
    typeof candidate.appState === "object" && candidate.appState !== null
      ? candidate.appState
      : {};
  const files =
    typeof candidate.files === "object" && candidate.files !== null
      ? (candidate.files as Record<string, unknown>)
      : {};

  return { elements, appState, files };
}
