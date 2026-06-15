import {
  bridgeApiVersion,
  type BridgeInfo,
  type BridgeResult,
  type DocumentSnapshot,
  type EditorBridge,
} from "./editorBridge";

const initialDocument = [
  {
    type: "heading",
    props: { level: 1 },
    content: "CppWiki",
  },
  {
    type: "paragraph",
    content: "Running without Qt bridge.",
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

    async getInitialDocument(): Promise<BridgeResult<DocumentSnapshot>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: initialDocument };
    },

    async updateSnapshot(): Promise<BridgeResult<void>> {
      return { apiVersion: bridgeApiVersion, ok: true, result: undefined };
    },
  };
}
