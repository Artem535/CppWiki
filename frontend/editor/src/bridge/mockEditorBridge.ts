import {
  bridgeApiVersion,
  type DocumentSummary,
  type BridgeInfo,
  type BridgeResult,
  type EditorBridge,
  type InitialDocumentSnapshot,
} from "./editorBridge";
import {
  mockBodyBlockId,
  mockBodyText,
  mockHeadingBlockId,
  mockHeadingText,
  mockPageCreatedAt,
  mockPageId,
  mockPageTitle,
  mockPageUpdatedAt,
} from "../constants";

const initialDocument: InitialDocumentSnapshot = [
  {
    id: mockHeadingBlockId,
    type: "heading",
    props: { level: 1 },
    content: [{ type: "text", text: mockHeadingText, styles: {} }],
    children: [],
  },
  {
    id: mockBodyBlockId,
    type: "paragraph",
    content: [{ type: "text", text: mockBodyText, styles: {} }],
    children: [],
  },
];

const documents: DocumentSummary[] = [
  {
    id: mockPageId,
    title: mockPageTitle,
    parentId: null,
    sortOrder: 0,
    createdAt: mockPageCreatedAt,
    updatedAt: mockPageUpdatedAt,
    kind: "wikiPage",
  },
];

const mockAiChunkListeners = new Set<(requestId: string, chunk: string) => void>();
const mockAiToolCallListeners = new Set<
  (requestId: string, toolName: string, argumentsJson: string) => void
>();
const mockAiCompletedListeners = new Set<(requestId: string) => void>();
const mockAiFailedListeners = new Set<(requestId: string, error: string) => void>();

export function createMockEditorBridge(): EditorBridge {
  return {
    async getBridgeInfo(): Promise<BridgeResult<BridgeInfo>> {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: {
          apiVersion: bridgeApiVersion,
          namespace: "wiki.documents",
          // Mock bridge (no Qt/QWebChannel): enable inline suggestions by
          // default so the feature is exercisable in `npm run dev` without a
          // full desktop build.
          aiInlineSuggestionsEnabled: true,
          methods: [
            "getBridgeInfo",
            "getInitialDocument",
            "listDocuments",
            "loadDocument",
            "openDocument",
            "updateSnapshot",
            "exportTextToFile",
            "importTextFromFile",
          ],
        },
      };
    },

    async getInitialDocument(): Promise<BridgeResult<InitialDocumentSnapshot>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: initialDocument };
    },

    async listDocuments(): Promise<BridgeResult<DocumentSummary[]>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: documents };
    },

    async loadDocument() {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: {
          id: mockPageId,
          title: mockPageTitle,
          parentId: null,
          sortOrder: 0,
          createdAt: mockPageCreatedAt,
          updatedAt: mockPageUpdatedAt,
          blocks: initialDocument,
          editable: true,
          lockOwner: null,
          accessMessage: "Document: local-only editing",
          kind: "wikiPage",
        },
      };
    },

    async openDocument(pageId) {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: {
          id: pageId,
          title: mockPageTitle,
          parentId: null,
          sortOrder: 0,
          createdAt: mockPageCreatedAt,
          updatedAt: mockPageUpdatedAt,
          blocks: initialDocument,
          editable: true,
          lockOwner: null,
          accessMessage: "Document: local-only editing",
          kind: "wikiPage",
        },
      };
    },

    async updateSnapshot(_pageId, _snapshot): Promise<BridgeResult<void>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: undefined };
    },

    // No native file dialog outside the Qt embedding (`npm run dev`) — mimic "user cancelled"
    // rather than pretending a file was chosen, since there's nowhere for content to go.
    async exportTextToFile(suggestedFileName, _nameFilter, _content) {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: {
          code: "cancelled",
          message: `Export to disk is only available in the desktop app (would have saved "${suggestedFileName}").`,
        },
      };
    },

    async importTextFromFile(_nameFilter) {
      return {
        apiVersion: bridgeApiVersion,
        ok: false,
        error: { code: "cancelled", message: "Import from disk is only available in the desktop app." },
      };
    },

    onDocumentOpenRequested() {
      return () => undefined;
    },

    onDocumentLoaded() {
      return () => undefined;
    },

    onDocumentAccessChanged() {
      return () => undefined;
    },

    onDocumentLoadFailed() {
      return () => undefined;
    },

    onDocumentSelectionCleared() {
      return () => undefined;
    },

    async startAiRequest(prompt, contextText, _mode, toolName, toolSchemaJson) {
      const requestId = `mock-ai-${Math.random().toString(36).slice(2)}`;
      window.setTimeout(() => {
        if (toolName && toolSchemaJson) {
          // Mock tool-call reply: an empty operations array is a valid,
          // schema-conforming structured response for most xl-ai tool
          // schemas, and is enough to exercise the tool-call relay path.
          mockAiToolCallListeners.forEach((listener) =>
            listener(requestId, toolName, JSON.stringify({ operations: [] })),
          );
          mockAiCompletedListeners.forEach((listener) => listener(requestId));
          return;
        }

        const mockReply = `[mock ${prompt}] ${contextText}`.trim();
        for (const char of mockReply) {
          mockAiChunkListeners.forEach((listener) => listener(requestId, char));
        }
        mockAiCompletedListeners.forEach((listener) => listener(requestId));
      }, 10);
      return requestId;
    },

    onAiChunkReceived(callback) {
      mockAiChunkListeners.add(callback);
      return () => {
        mockAiChunkListeners.delete(callback);
      };
    },

    onAiToolCallReceived(callback) {
      mockAiToolCallListeners.add(callback);
      return () => {
        mockAiToolCallListeners.delete(callback);
      };
    },

    onAiRequestCompleted(callback) {
      mockAiCompletedListeners.add(callback);
      return () => {
        mockAiCompletedListeners.delete(callback);
      };
    },

    onAiRequestFailed(callback) {
      mockAiFailedListeners.add(callback);
      return () => {
        mockAiFailedListeners.delete(callback);
      };
    },
  };
}
