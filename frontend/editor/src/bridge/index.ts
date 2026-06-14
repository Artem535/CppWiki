import type { EditorBridge } from "./editorBridge";
import { createMockEditorBridge } from "./mockEditorBridge";
import { createQtEditorBridge } from "./qtWebChannel";

export async function createEditorBridge(): Promise<EditorBridge> {
  return (await createQtEditorBridge()) ?? createMockEditorBridge();
}
