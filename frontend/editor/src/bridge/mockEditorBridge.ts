import type { BridgeResult, DocumentSnapshot, EditorBridge } from "./editorBridge";

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
    async getInitialDocument(): Promise<BridgeResult<DocumentSnapshot>> {
      return { ok: true, result: initialDocument };
    },

    async updateSnapshot(): Promise<BridgeResult<void>> {
      return { ok: true, result: undefined };
    },
  };
}
