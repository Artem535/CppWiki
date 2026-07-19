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
export type DocumentKind = "wikiPage" | "jupyterNotebook" | "excalidrawCanvas";

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
  // kinds (e.g. "jupyterNotebook", #52) it's whatever JSON-serializable value that kind's
  // component maintains as its document state (nbformat v4's notebook object, in the notebook
  // case) — the C++ side branches on the currently-open document's kind and, for non-wikiPage
  // kinds, persists `JSON.stringify(snapshot)` verbatim (only checking it's well-formed JSON)
  // instead of parsing it as a BlockNote block array. Widened from `DocumentSnapshot` alone so
  // callers outside the BlockNote path don't need an unsound cast.
  updateSnapshot(snapshot: DocumentSnapshot | unknown): Promise<BridgeResult<void>>;
  onDocumentOpenRequested(callback: (pageId: string) => void): () => void;
  onDocumentLoaded(callback: (document: LoadedDocument) => void): () => void;
  onDocumentAccessChanged(
    callback: (editable: boolean, lockOwner: string, accessMessage: string) => void,
  ): () => void;
  onDocumentLoadFailed(callback: (pageId: string, message: string) => void): () => void;
  onDocumentSelectionCleared(callback: () => void): () => void;

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
