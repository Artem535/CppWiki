import { describe, expect, it } from "vitest";

import type { EditorBridge } from "../bridge/editorBridge";
import { attachmentChunkSizeBytes, encodeAttachmentChunk, uploadAttachment } from "./attachmentUpload";

describe("attachment upload encoding", () => {
  it("encodes binary data without changing byte values", () => {
    expect(encodeAttachmentChunk(new Uint8Array([0, 137, 80, 78, 71, 255]))).toBe("AIlQTkf/");
  });

  it("uses chunks small enough for the native bridge contract", () => {
    expect(attachmentChunkSizeBytes).toBeLessThanOrEqual(256 * 1024);
  });

  it("uploads every chunk before completing", async () => {
    const appended: string[] = [];
    const bridge = {
      beginAttachmentUpload: async () => ({ apiVersion: 1, ok: true, result: { uploadId: "upload-1" } }),
      appendAttachmentChunk: async (_uploadId: string, chunk: string) => {
        appended.push(chunk);
        return { apiVersion: 1, ok: true, result: undefined };
      },
      completeAttachmentUpload: async () => ({
        apiVersion: 1,
        ok: true,
        result: { attachmentId: "attachment-1", uri: "cppwiki-attachment://attachment-1" },
      }),
      cancelAttachmentUpload: async () => ({ apiVersion: 1, ok: true, result: undefined }),
    } as unknown as EditorBridge;
    const bytes = new Uint8Array(attachmentChunkSizeBytes + 1);
    bytes[0] = 137;
    bytes[bytes.length - 1] = 255;
    const file = {
      name: "image.png",
      type: "image/png",
      size: bytes.length,
      arrayBuffer: async () => bytes.buffer,
    } as File;

    const result = await uploadAttachment(bridge, file);

    expect(result.ok).toBe(true);
    expect(appended).toHaveLength(2);
    expect(appended[0]).toBe(encodeAttachmentChunk(bytes.subarray(0, attachmentChunkSizeBytes)));
  });

  it("cancels a started upload after a rejected chunk", async () => {
    let cancelled = false;
    const bridge = {
      beginAttachmentUpload: async () => ({ apiVersion: 1, ok: true, result: { uploadId: "upload-1" } }),
      appendAttachmentChunk: async () => ({
        apiVersion: 1,
        ok: false,
        error: { code: "invalid_attachment", message: "Rejected" },
      }),
      cancelAttachmentUpload: async () => {
        cancelled = true;
        return { apiVersion: 1, ok: true, result: undefined };
      },
    } as unknown as EditorBridge;
    const file = {
      name: "image.png",
      type: "image/png",
      size: 1,
      arrayBuffer: async () => new Uint8Array([1]).buffer,
    } as File;

    const result = await uploadAttachment(bridge, file);

    expect(result.ok).toBe(false);
    expect(cancelled).toBe(true);
  });
});
