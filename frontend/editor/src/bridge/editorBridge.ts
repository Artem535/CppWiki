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
  // Raw stored snapshot JSON, verbatim (added alongside `blocks` for non-wikiPage kinds, #53).
  // For kind === "wikiPage" this is BlockNote's own {id, schema_version, title, blocks} shape
  // and `blocks` above should be used instead; for "excalidrawCanvas"/"jupyterNotebook" this is
  // the only place the document's content is available, since it isn't a BlockNote block array.
  snapshotJson?: string;
  editable: boolean;
  lockOwner?: string | null;
  accessMessage?: string | null;
  kind?: DocumentKind;
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
  // `DocumentSnapshot` (BlockNote's `Block[]`) is the shape for kind === "wikiPage", the common
  // case; non-wiki-page kinds (#52/#53) pass their own JSON-serializable scene/notebook shape
  // instead — the C++ side (QEditorBridge::updateSnapshot) only requires well-formed JSON for
  // those kinds (see DocumentValidator::ParseAndValidateSnapshot) and persists it verbatim.
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
