import type {
  BridgeInfo,
  BridgeResult,
  DocumentSummary,
  EditorBridge,
  InitialDocumentSnapshot,
  LoadedDocument,
} from "./editorBridge";

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
  documentOpenRequested: {
    connect(callback: (pageId: string) => void): void;
    disconnect(callback: (pageId: string) => void): void;
  };
  documentLoaded: {
    connect(callback: (document: LoadedDocument) => void): void;
    disconnect(callback: (document: LoadedDocument) => void): void;
  };
  documentAccessChanged: {
    connect(callback: (editable: boolean, lockOwner: string, accessMessage: string) => void): void;
    disconnect(
      callback: (editable: boolean, lockOwner: string, accessMessage: string) => void,
    ): void;
  };
  documentLoadFailed: {
    connect(callback: (pageId: string, message: string) => void): void;
    disconnect(callback: (pageId: string, message: string) => void): void;
  };
  documentSelectionCleared: {
    connect(callback: () => void): void;
    disconnect(callback: () => void): void;
  };
  aiChunkReceived: {
    connect(callback: (requestId: string, chunk: string) => void): void;
    disconnect(callback: (requestId: string, chunk: string) => void): void;
  };
  aiToolCallReceived: {
    connect(callback: (requestId: string, toolName: string, argumentsJson: string) => void): void;
    disconnect(
      callback: (requestId: string, toolName: string, argumentsJson: string) => void,
    ): void;
  };
  aiRequestCompleted: {
    connect(callback: (requestId: string) => void): void;
    disconnect(callback: (requestId: string) => void): void;
  };
  aiRequestFailed: {
    connect(callback: (requestId: string, error: string) => void): void;
    disconnect(callback: (requestId: string, error: string) => void): void;
  };
  startAiRequest(
    prompt: string,
    contextText: string,
    mode: string,
    toolName: string,
    toolSchemaJson: string,
    callback: (requestId: string) => void,
  ): void;
  getBridgeInfo(callback: (response: BridgeResult<BridgeInfo>) => void): void;
  getInitialDocument(
    callback: (response: BridgeResult<InitialDocumentSnapshot>) => void,
  ): void;
  listDocuments(callback: (response: BridgeResult<DocumentSummary[]>) => void): void;
  loadDocument(
    pageId: string,
    callback: (response: BridgeResult<LoadedDocument>) => void,
  ): void;
  openDocument(
    pageId: string,
    callback: (response: BridgeResult<LoadedDocument>) => void,
  ): void;
  updateSnapshot(
    pageId: string,
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
      resolve(channel.objects.wikiDocuments as QtEditorBridgeObject);
    });
  });

  return {
    getBridgeInfo() {
      return new Promise((resolve) => {
        qtObject.getBridgeInfo(resolve);
      });
    },

    getInitialDocument() {
      return new Promise((resolve) => {
        qtObject.getInitialDocument(resolve);
      });
    },

    listDocuments() {
      return new Promise((resolve) => {
        qtObject.listDocuments(resolve);
      });
    },

    loadDocument(pageId) {
      return new Promise((resolve) => {
        qtObject.loadDocument(pageId, resolve);
      });
    },

    openDocument(pageId) {
      return new Promise((resolve) => {
        qtObject.openDocument(pageId, resolve);
      });
    },

    updateSnapshot(pageId, snapshot) {
      return new Promise((resolve) => {
        qtObject.updateSnapshot(pageId, JSON.stringify(snapshot), resolve);
      });
    },

    onDocumentOpenRequested(callback) {
      qtObject.documentOpenRequested.connect(callback);
      return () => {
        qtObject.documentOpenRequested.disconnect(callback);
      };
    },

    onDocumentLoaded(callback) {
      qtObject.documentLoaded.connect(callback);
      return () => {
        qtObject.documentLoaded.disconnect(callback);
      };
    },

    onDocumentAccessChanged(callback) {
      qtObject.documentAccessChanged.connect(callback);
      return () => {
        qtObject.documentAccessChanged.disconnect(callback);
      };
    },

    onDocumentLoadFailed(callback) {
      qtObject.documentLoadFailed.connect(callback);
      return () => {
        qtObject.documentLoadFailed.disconnect(callback);
      };
    },

    onDocumentSelectionCleared(callback) {
      qtObject.documentSelectionCleared.connect(callback);
      return () => {
        qtObject.documentSelectionCleared.disconnect(callback);
      };
    },

    startAiRequest(prompt, contextText, mode, toolName, toolSchemaJson) {
      return new Promise((resolve) => {
        qtObject.startAiRequest(
          prompt,
          contextText,
          mode,
          toolName ?? "",
          toolSchemaJson ?? "",
          resolve,
        );
      });
    },

    onAiChunkReceived(callback) {
      qtObject.aiChunkReceived.connect(callback);
      return () => {
        qtObject.aiChunkReceived.disconnect(callback);
      };
    },

    onAiToolCallReceived(callback) {
      qtObject.aiToolCallReceived.connect(callback);
      return () => {
        qtObject.aiToolCallReceived.disconnect(callback);
      };
    },

    onAiRequestCompleted(callback) {
      qtObject.aiRequestCompleted.connect(callback);
      return () => {
        qtObject.aiRequestCompleted.disconnect(callback);
      };
    },

    onAiRequestFailed(callback) {
      qtObject.aiRequestFailed.connect(callback);
      return () => {
        qtObject.aiRequestFailed.disconnect(callback);
      };
    },
  };
}
