import { describe, expect, it } from "vitest";
import { parseExcalidrawSceneJson, serializeExcalidrawScene } from "./excalidrawScene";

describe("serializeExcalidrawScene", () => {
  it("serializes elements/appState/files into a stable excalidraw scene envelope", () => {
    const json = serializeExcalidrawScene(
      [{ id: "el1", type: "rectangle" }],
      { viewBackgroundColor: "#ffffff", gridSize: 20 },
      { file1: { mimeType: "image/png" } },
    );

    const parsed = JSON.parse(json);
    expect(parsed).toEqual({
      type: "excalidraw",
      version: 2,
      elements: [{ id: "el1", type: "rectangle" }],
      appState: { viewBackgroundColor: "#ffffff", gridSize: 20 },
      files: { file1: { mimeType: "image/png" } },
    });
  });

  it("defaults gridSize to null when absent", () => {
    const json = serializeExcalidrawScene([], {}, {});
    expect(JSON.parse(json).appState.gridSize).toBeNull();
  });
});

describe("parseExcalidrawSceneJson", () => {
  it("returns null for an empty/blank snapshot (new document)", () => {
    expect(parseExcalidrawSceneJson(undefined)).toBeNull();
    expect(parseExcalidrawSceneJson(null)).toBeNull();
    expect(parseExcalidrawSceneJson("")).toBeNull();
    expect(parseExcalidrawSceneJson("   ")).toBeNull();
  });

  it("round-trips a scene produced by serializeExcalidrawScene", () => {
    const json = serializeExcalidrawScene(
      [{ id: "el1", type: "ellipse" }],
      { viewBackgroundColor: "#111111", gridSize: null },
      {},
    );

    expect(parseExcalidrawSceneJson(json)).toEqual({
      elements: [{ id: "el1", type: "ellipse" }],
      appState: { viewBackgroundColor: "#111111", gridSize: null },
      files: {},
    });
  });

  it("falls back to a blank scene for malformed JSON instead of throwing", () => {
    expect(parseExcalidrawSceneJson("{not valid json")).toBeNull();
  });

  it("falls back to a blank scene for JSON that isn't an object", () => {
    expect(parseExcalidrawSceneJson("42")).toBeNull();
    expect(parseExcalidrawSceneJson('"a string"')).toBeNull();
  });

  it("tolerates a document's default empty-blocks snapshot (non-excalidraw shape)", () => {
    // New documents are created via MakeNewDocumentRecord() with a generic
    // {id, schema_version, title, blocks: []} envelope regardless of kind (see
    // src/bridge/editor_bridge.cc) until the canvas is first edited.
    const result = parseExcalidrawSceneJson(
      JSON.stringify({ id: "doc1", schema_version: 1, title: "Untitled", blocks: [] }),
    );
    expect(result).toEqual({ elements: [], appState: {}, files: {} });
  });
});
