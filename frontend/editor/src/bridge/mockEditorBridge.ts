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
  },
];

const mockAiChunkListeners = new Set<(requestId: string, chunk: string) => void>();
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
        },
      };
    },

    async updateSnapshot(): Promise<BridgeResult<void>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: undefined };
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

    async startAiRequest(prompt, contextText) {
      const requestId = `mock-ai-${Math.random().toString(36).slice(2)}`;
      const mockReply = `[mock ${prompt}] ${contextText}`.trim();
      window.setTimeout(() => {
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
