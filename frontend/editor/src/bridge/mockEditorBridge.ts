import {
  bridgeApiVersion,
  type BridgeInfo,
  type BridgeResult,
  type EditorBridge,
  type InitialDocumentSnapshot,
} from "./editorBridge";

const initialDocument: InitialDocumentSnapshot = [
  {
    id: "mock-heading",
    type: "heading",
    props: { level: 1 },
    content: [{ type: "text", text: "CppWiki", styles: {} }],
    children: [],
  },
  {
    id: "mock-body",
    type: "paragraph",
    content: [{ type: "text", text: "Running without Qt bridge.", styles: {} }],
    children: [],
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
          methods: ["getBridgeInfo", "getInitialDocument", "updateSnapshot"],
        },
      };
    },

    async getInitialDocument(): Promise<BridgeResult<InitialDocumentSnapshot>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: initialDocument };
    },

    async updateSnapshot(): Promise<BridgeResult<void>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: undefined };
    },
  };
}
