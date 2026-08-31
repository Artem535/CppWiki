import { bridgeApiVersion, type BridgeResult, type EditorBridge, type StoredAttachment } from "../bridge/editorBridge";

// The C++ bridge applies the authoritative 25 MiB file limit. Keeping browser chunks below the
// 256 KiB decoded-byte limit also keeps individual JSON/QWebChannel messages bounded despite
// Base64's expansion.
export const attachmentChunkSizeBytes = 192 * 1024;

export function encodeAttachmentChunk(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary);
}

export async function uploadAttachment(
  bridge: EditorBridge,
  file: File,
): Promise<BridgeResult<StoredAttachment>> {
  const begun = await bridge.beginAttachmentUpload({
    filename: file.name,
    mimeType: file.type || "application/octet-stream",
    sizeBytes: file.size,
  });
  if (!begun.ok) {
    return begun;
  }

  const bytes = new Uint8Array(await file.arrayBuffer());
  for (let offset = 0; offset < bytes.length; offset += attachmentChunkSizeBytes) {
    const chunk = bytes.subarray(offset, Math.min(offset + attachmentChunkSizeBytes, bytes.length));
    const appended = await bridge.appendAttachmentChunk(begun.result.uploadId, encodeAttachmentChunk(chunk));
    if (!appended.ok) {
      await bridge.cancelAttachmentUpload(begun.result.uploadId);
      return appended;
    }
  }

  const completed = await bridge.completeAttachmentUpload(begun.result.uploadId);
  if (!completed.ok) {
    return completed;
  }
  return completed;
}
