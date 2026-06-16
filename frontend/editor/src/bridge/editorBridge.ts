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

export type DocumentSummary = {
  id: string;
  title: string;
  parentId?: string | null;
  sortOrder: number;
  createdAt: string;
  updatedAt: string;
};

export type LoadedDocument = {
  id: string;
  title: string;
  parentId?: string | null;
  sortOrder: number;
  createdAt: string;
  updatedAt: string;
  blocks: InitialDocumentSnapshot;
};

export type BridgeInfo = {
  apiVersion: typeof bridgeApiVersion;
  namespace: "wiki.documents";
  methods: string[];
};

export interface EditorBridge {
  getBridgeInfo(): Promise<BridgeResult<BridgeInfo>>;
  getInitialDocument(): Promise<BridgeResult<InitialDocumentSnapshot>>;
  listDocuments(): Promise<BridgeResult<DocumentSummary[]>>;
  loadDocument(pageId: string): Promise<BridgeResult<LoadedDocument>>;
  openDocument(pageId: string): Promise<BridgeResult<LoadedDocument>>;
  updateSnapshot(snapshot: DocumentSnapshot): Promise<BridgeResult<void>>;
  onDocumentOpenRequested(callback: (pageId: string) => void): () => void;
  onDocumentLoaded(callback: (document: LoadedDocument) => void): () => void;
  onDocumentLoadFailed(callback: (pageId: string, message: string) => void): () => void;
}
