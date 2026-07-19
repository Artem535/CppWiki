// Pure helpers for reading/writing nbformat v4 notebook JSON (no execution, no kernel — issue
// #52). Deliberately narrow: only the fields NotebookView.tsx needs to view/edit cell source and
// render already-saved outputs, not a full nbformat v4 schema implementation.

export type NbSource = string | string[];

export type NbStreamOutput = {
  output_type: "stream";
  name?: string;
  text?: NbSource;
};

export type NbDataBundle = Record<string, unknown>;

export type NbExecuteResultOutput = {
  output_type: "execute_result";
  data?: NbDataBundle;
};

export type NbDisplayDataOutput = {
  output_type: "display_data";
  data?: NbDataBundle;
};

export type NbErrorOutput = {
  output_type: "error";
  ename?: string;
  evalue?: string;
  traceback?: string[];
};

export type NbOutput =
  | NbStreamOutput
  | NbExecuteResultOutput
  | NbDisplayDataOutput
  | NbErrorOutput
  | { output_type: string; [key: string]: unknown };

export type NbCell = {
  cell_type: "markdown" | "code" | "raw" | string;
  source: NbSource;
  outputs?: NbOutput[];
  metadata?: Record<string, unknown>;
  execution_count?: number | null;
  [key: string]: unknown;
};

export type NbNotebook = {
  cells: NbCell[];
  metadata?: Record<string, unknown>;
  nbformat?: number;
  nbformat_minor?: number;
  [key: string]: unknown;
};

// nbformat's `source`/output `text` fields are either a single string or an array of strings
// meant to be concatenated (each element commonly, but not always, ending in "\n").
export function joinNbSource(source: NbSource | undefined): string {
  if (source === undefined) {
    return "";
  }
  return Array.isArray(source) ? source.join("") : source;
}

// Splits editable plain text back into nbformat's array-of-lines convention (each line but the
// last keeps a trailing "\n"), which is the more common on-disk form and round-trips predictably.
export function splitToNbSource(text: string): string[] {
  if (text === "") {
    return [];
  }
  const lines = text.split("\n");
  return lines.map((line, index) => (index < lines.length - 1 ? `${line}\n` : line));
}

export function parseNotebookJson(raw: string): NbNotebook | null {
  if (!raw || raw.trim() === "") {
    return { cells: [], nbformat: 4, nbformat_minor: 5 };
  }
  try {
    const parsed: unknown = JSON.parse(raw);
    if (
      typeof parsed === "object" &&
      parsed !== null &&
      Array.isArray((parsed as { cells?: unknown }).cells)
    ) {
      return parsed as NbNotebook;
    }
    return null;
  } catch {
    return null;
  }
}

// The MIME types this viewer knows how to render, in nbformat's own preference order (richest
// representation first) — mirrors Jupyter's own output-resolution convention.
const kSupportedMimeOrder = ["image/png", "text/html", "text/plain"] as const;
export type SupportedOutputMime = (typeof kSupportedMimeOrder)[number];

export type ResolvedOutputData = {
  mime: SupportedOutputMime;
  value: string;
};

// Picks the best renderable MIME representation out of an execute_result/display_data output's
// `data` bundle, per kSupportedMimeOrder. Returns null if none of the bundle's keys are
// supported (the output is simply not rendered, rather than showing an error).
export function resolveOutputData(data: NbDataBundle | undefined): ResolvedOutputData | null {
  if (!data) {
    return null;
  }
  for (const mime of kSupportedMimeOrder) {
    const value = data[mime];
    if (value === undefined) {
      continue;
    }
    if (mime === "image/png") {
      // image/png data is nbformat's base64 string, without a data: URI prefix.
      const base64 = Array.isArray(value) ? value.join("") : String(value);
      return { mime, value: base64 };
    }
    const text = Array.isArray(value) ? value.join("") : String(value);
    return { mime, value: text };
  }
  return null;
}
