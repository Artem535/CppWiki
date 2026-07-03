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

export function createMockEditorBridge(): EditorBridge {
  return {
    async getBridgeInfo(): Promise<BridgeResult<BridgeInfo>> {
      return {
        apiVersion: bridgeApiVersion,
        ok: true,
        result: {
          apiVersion: bridgeApiVersion,
          namespace: "wiki.documents",
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
  };
}
