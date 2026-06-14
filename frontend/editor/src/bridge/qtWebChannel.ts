import type { BridgeResult, DocumentSnapshot, EditorBridge } from "./editorBridge";

declare global {
  interface Window {
    qt?: {
      webChannelTransport?: unknown;
    };
    QWebChannel?: new (
      transport: unknown,
      callback: (channel: { objects: Record<string, unknown> }) => void,
    ) => void;
  }
}

type QtEditorBridgeObject = {
  getInitialDocument(
    callback: (response: BridgeResult<DocumentSnapshot>) => void,
  ): void;
  updateSnapshot(
    snapshotJson: string,
    callback: (response: BridgeResult<void>) => void,
  ): void;
};

export async function createQtEditorBridge(): Promise<EditorBridge | null> {
  if (!window.qt?.webChannelTransport || !window.QWebChannel) {
    return null;
  }

  const qtObject = await new Promise<QtEditorBridgeObject>((resolve) => {
    new window.QWebChannel!(window.qt!.webChannelTransport!, (channel) => {
      resolve(channel.objects.editorBridge as QtEditorBridgeObject);
    });
  });

  return {
    getInitialDocument() {
      return new Promise((resolve) => {
        qtObject.getInitialDocument(resolve);
      });
    },

    updateSnapshot(snapshot) {
      return new Promise((resolve) => {
        qtObject.updateSnapshot(JSON.stringify(snapshot), resolve);
      });
    },
  };
}
