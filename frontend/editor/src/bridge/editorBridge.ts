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

export type BridgeInfo = {
  apiVersion: typeof bridgeApiVersion;
  namespace: "wiki.documents";
  methods: string[];
};

export interface EditorBridge {
  getBridgeInfo(): Promise<BridgeResult<BridgeInfo>>;
  getInitialDocument(): Promise<BridgeResult<InitialDocumentSnapshot>>;
  updateSnapshot(snapshot: DocumentSnapshot): Promise<BridgeResult<void>>;
}
