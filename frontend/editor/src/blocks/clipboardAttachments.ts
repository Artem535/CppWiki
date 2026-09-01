/**
 * Returns files exposed by a browser clipboard event.
 *
 * Clipboard images are represented as DataTransferItems rather than as a normal file list
 * in Chromium/QWebEngine, so reading `clipboardData.files` alone misses them.
 */
export function getClipboardFiles(event: ClipboardEvent): File[] {
  const items = event.clipboardData?.items;
  if (!items) {
    return [];
  }

  return Array.from(items)
    .filter((item) => item.kind === "file")
    .map((item) => item.getAsFile())
    .filter((file): file is File => file !== null)
    .map((file, index) => ensureClipboardFileName(file, index));
}

function ensureClipboardFileName(file: File, index: number): File {
  if (file.name) {
    return file;
  }

  const extension = file.type.includes("/") ? file.type.split("/")[1] : "bin";
  return new File([file], `clipboard-${index + 1}.${extension}`, {
    lastModified: file.lastModified,
    type: file.type || "application/octet-stream",
  });
}
