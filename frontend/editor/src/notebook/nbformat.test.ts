import { describe, expect, it } from "vitest";
import {
  joinNbSource,
  parseNotebookJson,
  resolveKernelLanguage,
  resolveOutputData,
  splitToNbSource,
  type NbNotebook,
} from "./nbformat";

describe("joinNbSource", () => {
  it("returns a plain string unchanged", () => {
    expect(joinNbSource("print('hi')")).toBe("print('hi')");
  });

  it("joins an array-of-lines source without inserting separators", () => {
    expect(joinNbSource(["import os\n", "print(os.getcwd())"])).toBe(
      "import os\nprint(os.getcwd())",
    );
  });

  it("returns an empty string for undefined", () => {
    expect(joinNbSource(undefined)).toBe("");
  });
});

describe("splitToNbSource", () => {
  it("round-trips through joinNbSource", () => {
    const text = "line one\nline two\nline three";
    expect(joinNbSource(splitToNbSource(text))).toBe(text);
  });

  it("returns an empty array for empty text", () => {
    expect(splitToNbSource("")).toEqual([]);
  });

  it("appends trailing newlines to every line but the last", () => {
    expect(splitToNbSource("a\nb")).toEqual(["a\n", "b"]);
  });
});

describe("parseNotebookJson", () => {
  it("parses a well-formed nbformat v4 document", () => {
    const raw = JSON.stringify({
      cells: [{ cell_type: "markdown", source: ["# Title"] }],
      nbformat: 4,
      nbformat_minor: 5,
    });
    const notebook = parseNotebookJson(raw);
    expect(notebook).not.toBeNull();
    expect(notebook?.cells).toHaveLength(1);
  });

  it("returns an empty notebook skeleton for empty input", () => {
    const notebook = parseNotebookJson("");
    expect(notebook?.cells).toEqual([]);
  });

  it("returns null for invalid JSON", () => {
    expect(parseNotebookJson("{not json")).toBeNull();
  });

  it("returns null when the JSON is well-formed but has no cells array", () => {
    expect(parseNotebookJson(JSON.stringify({ foo: "bar" }))).toBeNull();
  });
});

describe("resolveOutputData", () => {
  it("prefers image/png over text/html and text/plain", () => {
    const resolved = resolveOutputData({
      "image/png": "base64data",
      "text/html": "<b>hi</b>",
      "text/plain": "hi",
    });
    expect(resolved).toEqual({ mime: "image/png", value: "base64data" });
  });

  it("falls back to text/html when no image is present", () => {
    const resolved = resolveOutputData({
      "text/html": ["<b>", "hi</b>"],
      "text/plain": "hi",
    });
    expect(resolved).toEqual({ mime: "text/html", value: "<b>hi</b>" });
  });

  it("falls back to text/plain when nothing richer is present", () => {
    const resolved = resolveOutputData({ "text/plain": "hello" });
    expect(resolved).toEqual({ mime: "text/plain", value: "hello" });
  });

  it("returns null when the bundle has no supported MIME type", () => {
    expect(resolveOutputData({ "application/json": { a: 1 } })).toBeNull();
  });

  it("returns null for an undefined bundle", () => {
    expect(resolveOutputData(undefined)).toBeNull();
  });
});

describe("resolveKernelLanguage", () => {
  const withMetadata = (metadata: Record<string, unknown>): NbNotebook => ({
    cells: [],
    metadata,
  });

  it("prefers metadata.language_info.name when present", () => {
    const notebook = withMetadata({
      kernelspec: { name: "python3", language: "python" },
      language_info: { name: "python" },
    });
    expect(resolveKernelLanguage(notebook)).toBe("python");
  });

  it("falls back to metadata.kernelspec.language when language_info is absent", () => {
    const notebook = withMetadata({ kernelspec: { name: "ir", language: "R" } });
    expect(resolveKernelLanguage(notebook)).toBe("r");
  });

  it("normalizes to lowercase", () => {
    const notebook = withMetadata({ language_info: { name: "JavaScript" } });
    expect(resolveKernelLanguage(notebook)).toBe("javascript");
  });

  it("defaults to python when metadata has neither field", () => {
    expect(resolveKernelLanguage(withMetadata({}))).toBe("python");
  });

  it("defaults to python when metadata is absent entirely", () => {
    expect(resolveKernelLanguage({ cells: [] })).toBe("python");
  });

  it("defaults to python for a null notebook", () => {
    expect(resolveKernelLanguage(null)).toBe("python");
  });
});
