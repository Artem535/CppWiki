export type BridgeResult<T> =
  | { ok: true; result: T }
  | { ok: false; error: { code: string; message: string } };

export type DocumentSnapshot = unknown;

export interface EditorBridge {
  getInitialDocument(): Promise<BridgeResult<DocumentSnapshot>>;
  updateSnapshot(snapshot: DocumentSnapshot): Promise<BridgeResult<void>>;
}
