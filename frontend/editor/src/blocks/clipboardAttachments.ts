/**
 * Returns files exposed by a browser clipboard event.
 *
 * Clipboard images are represented as DataTransferItems rather than as a normal file list
 * in Chromium/QWebEngine, so reading `clipboardData.files` alone misses them.
 */
export function getClipboardFiles(event: ClipboardEvent): File[] {
  const clipboardData = event.clipboardData;
  if (!clipboardData) {
    return [];
  }

  const filesFromItems = Array.from(clipboardData.items ?? [])
    .filter((item) => item.kind === "file")
    .map((item) => item.getAsFile())
    .filter((file): file is File => file !== null);
  const filesFromList = Array.from(clipboardData.files ?? []);
  const files = [...filesFromItems, ...filesFromList];

  // Some clipboard providers expose the same file through both items and files. Keep the
  // insertion deterministic and avoid creating duplicate attachment blocks.
  const uniqueFiles: File[] = [];
  const seen = new Set<File>();
  for (const file of files) {
    if (!seen.has(file)) {
      seen.add(file);
      uniqueFiles.push(file);
    }
  }

  return uniqueFiles.map((file, index) => ensureClipboardFileName(file, index));
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
