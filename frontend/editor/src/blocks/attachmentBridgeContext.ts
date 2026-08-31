import type { EditorBridge } from "../bridge/editorBridge";

// BlockNote creates block renderers from the module-level schema. Keep the currently connected
// native bridge in this narrow module instead of exposing a global Qt object to block renderers.
// The editor shell owns lifecycle and clears it when unmounted.
let activeBridge: EditorBridge | null = null;

export function setAttachmentBridge(bridge: EditorBridge | null): void {
  activeBridge = bridge;
}

export function getAttachmentBridge(): EditorBridge | null {
  return activeBridge;
}
