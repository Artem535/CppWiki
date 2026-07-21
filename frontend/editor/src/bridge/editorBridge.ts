import type { Block, PartialBlock } from "@blocknote/core";

export const bridgeApiVersion = 1;

export type BridgeResult<T> =
  | { apiVersion: typeof bridgeApiVersion; ok: true; result: T }
  | {
      apiVersion: typeof bridgeApiVersion;
      ok: false;
      error: { code: string; message: string };
    };

export type DocumentSnapshot = Block[];
export type InitialDocumentSnapshot = PartialBlock[];

// Content-schema/renderer discriminator for a document (ADR-017), mirroring
// cppwiki::document::DocumentKind on the C++ side. Persisted/transmitted as
// the string key produced by ToDocumentKindKey(), not a raw enum value.
export type DocumentKind = "wikiPage" | "jupyterNotebook" | "excalidrawCanvas" | "openApiSpec";

export type DocumentSummary = {
  id: string;
  title: string;
  parentId?: string | null;
  sortOrder: number;
  createdAt: string;
  updatedAt: string;
  kind?: DocumentKind;
};

export type LoadedDocument = {
  id: string;
  title: string;
  parentId?: string | null;
  sortOrder: number;
  createdAt: string;
  updatedAt: string;
  blocks: InitialDocumentSnapshot;
  editable: boolean;
  lockOwner?: string | null;
  accessMessage?: string | null;
  kind?: DocumentKind;
  // Raw stored snapshot JSON (as a string) for kinds whose content doesn't fit the BlockNote
  // block-array shape (`blocks` above) — currently "jupyterNotebook" (nbformat v4 JSON) and,
  // eventually, "excalidrawCanvas" (#53). Unset/empty for "wikiPage", where `blocks` is
  // authoritative. See EditorBridge.updateSnapshot()'s doc comment for the write-side mirror.
  rawContent?: string;
  // Present, one-shot, when this document was just created by Page::ImportDocumentAsNewFile()
  // from an imported Markdown file (issue #102 follow-up): raw Markdown text to convert via
  // BlockNoteEditor.tryParseMarkdownToBlocks() and apply in place of `blocks` (which is empty
  // for a freshly created document). See QEditorBridge::StashPendingMarkdownImport().
  pendingMarkdownImport?: string;
};

export type BridgeInfo = {
  apiVersion: typeof bridgeApiVersion;
  namespace: "wiki.documents";
  methods: string[];
  aiFeaturesEnabled?: boolean;
  aiAutocompleteEnabled?: boolean;
  // Separate opt-in for inline ghost-text suggestions (issue #59); independent
  // of aiFeaturesEnabled/aiAutocompleteEnabled.
  aiInlineSuggestionsEnabled?: boolean;
};

export interface EditorBridge {
  getBridgeInfo(): Promise<BridgeResult<BridgeInfo>>;
  getInitialDocument(): Promise<BridgeResult<InitialDocumentSnapshot>>;
  listDocuments(): Promise<BridgeResult<DocumentSummary[]>>;
  loadDocument(pageId: string): Promise<BridgeResult<LoadedDocument>>;
  openDocument(pageId: string): Promise<BridgeResult<LoadedDocument>>;
  // `snapshot` is `DocumentSnapshot` (BlockNote's `Block[]`) for "wikiPage" documents. For other
  // kinds (e.g. "jupyterNotebook"/"excalidrawCanvas", #52/#53) it's whatever JSON-serializable
  // value that kind's component maintains as its document state (nbformat v4's notebook object,
  // or the Excalidraw scene object) — the C++ side branches on the currently-open document's kind
  // and, for non-wikiPage kinds, persists `JSON.stringify(snapshot)` verbatim (only checking it's
  // well-formed JSON, see DocumentValidator::ParseAndValidateSnapshot) instead of parsing it as a
  // BlockNote block array. Widened from `DocumentSnapshot` alone so callers outside the BlockNote
  // path don't need an unsound cast.
  //
  // `pageId` is the document this snapshot is meant for, captured by the caller at the moment
  // it decided to save (not read from "whatever's current" at call time) — the C++ side rejects
  // the write with a "stale_document" error if the currently open document has since changed,
  // instead of silently applying it to the wrong document. This matters because saves are
  // debounced/async: the open document can change before a scheduled save actually fires.
  updateSnapshot(pageId: string, snapshot: DocumentSnapshot | unknown): Promise<BridgeResult<void>>;

  // Native file export/import (issue #82). Kind-agnostic: `content` is written/read verbatim
  // (no interpretation on the C++ side) via a native QFileDialog, specifically so CppWiki never
  // needs to expose Excalidraw's/Chromium's own File System Access API-backed save/open-to-disk
  // affordances in this embedding (calling those directly crashes the app — see
  // ExcalidrawCanvasView.tsx's UIOptions comment and page.cc's fileSystemAccessRequested
  // handling). Resolves with an `{ok: false, error: {code: "cancelled"}}` envelope (not a
  // rejected promise) if the user dismisses the native dialog without choosing a file, so
  // callers can treat that as a normal, silent no-op rather than an error to surface.
  exportTextToFile(
    suggestedFileName: string,
    nameFilter: string,
    content: string,
  ): Promise<BridgeResult<{ path: string; fileName: string }>>;
  importTextFromFile(
    nameFilter: string,
  ): Promise<BridgeResult<{ path: string; fileName: string; content: string }>>;

  onDocumentOpenRequested(callback: (pageId: string) => void): () => void;
  onDocumentLoaded(callback: (document: LoadedDocument) => void): () => void;
  onDocumentAccessChanged(
    callback: (editable: boolean, lockOwner: string, accessMessage: string) => void,
  ): () => void;
  onDocumentLoadFailed(callback: (pageId: string, message: string) => void): () => void;
  onDocumentSelectionCleared(callback: () => void): () => void;
  onExportCurrentDocumentRequested(callback: () => void): () => void;

  // AI transport (ADR-012): every AI request is forwarded through the bridge
  // to C++, never fetched directly from this JS context. `mode` matches the
  // MVP scope (ADR-010): "rewrite" or "autocomplete", plus "inline" for
  // continuous ghost-text completions (issue #59). `toolName`/`toolSchemaJson`
  // are optional and are set when the caller (xl-ai's AIExtension) wants a
  // structured tool-call response matching a JSON Schema rather than plain
  // text (see ADR-012 issue #65 addendum: xl-ai only applies document changes
  // from tool-call message parts, never from plain text).
  startAiRequest(
    prompt: string,
    contextText: string,
    mode: "rewrite" | "autocomplete" | "inline",
    toolName?: string,
    toolSchemaJson?: string,
  ): Promise<string>;
  onAiChunkReceived(callback: (requestId: string, chunk: string) => void): () => void;
  // Fired instead of onAiChunkReceived when the request was made with a tool
  // schema and the provider replied with a structured tool call.
  // `argumentsJson` is the tool call's arguments, JSON-stringified.
  onAiToolCallReceived(
    callback: (requestId: string, toolName: string, argumentsJson: string) => void,
  ): () => void;
  onAiRequestCompleted(callback: (requestId: string) => void): () => void;
  onAiRequestFailed(callback: (requestId: string, error: string) => void): () => void;
}
