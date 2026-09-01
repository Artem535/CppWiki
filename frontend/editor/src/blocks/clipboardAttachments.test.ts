import { describe, expect, it } from "vitest";

import { getClipboardFiles } from "./clipboardAttachments";

function clipboardEventWithItems(items: DataTransferItem[]): ClipboardEvent {
  return { clipboardData: { items } } as unknown as ClipboardEvent;
}

function clipboardEventWithFiles(files: File[]): ClipboardEvent {
  return { clipboardData: { files } } as unknown as ClipboardEvent;
}

describe("clipboard attachment extraction", () => {
  it("extracts clipboard files and gives unnamed images a stable filename", () => {
    const image = new File([new Uint8Array([1, 2, 3])], "", { type: "image/png" });
    const item = {
      kind: "file",
      getAsFile: () => image,
    } as unknown as DataTransferItem;

    const files = getClipboardFiles(clipboardEventWithItems([item]));

    expect(files).toHaveLength(1);
    expect(files[0].name).toBe("clipboard-1.png");
    expect(files[0].type).toBe("image/png");
  });

  it("ignores text clipboard items", () => {
    const item = {
      kind: "string",
      getAsFile: () => null,
    } as unknown as DataTransferItem;

    expect(getClipboardFiles(clipboardEventWithItems([item]))).toEqual([]);
  });

  it("ignores file items that cannot provide a File", () => {
    const item = {
      kind: "file",
      getAsFile: () => null,
    } as unknown as DataTransferItem;

    expect(getClipboardFiles(clipboardEventWithItems([item]))).toEqual([]);
  });

  it("also extracts files exposed through the clipboard file list", () => {
    const file = new File([new Uint8Array([1])], "notes.txt", { type: "text/plain" });

    expect(getClipboardFiles(clipboardEventWithFiles([file]))).toEqual([file]);
  });
});
