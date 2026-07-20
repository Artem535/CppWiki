import { describe, expect, it } from "vitest";

import { extractMermaidSource } from "./mermaidSource";

describe("extractMermaidSource", () => {
  it("returns an empty string for undefined content", () => {
    expect(extractMermaidSource(undefined)).toBe("");
  });

  it("returns an empty string for an empty content array", () => {
    expect(extractMermaidSource([])).toBe("");
  });

  it("concatenates a single text run", () => {
    expect(extractMermaidSource([{ type: "text", text: "graph TD; A-->B;" }])).toBe(
      "graph TD; A-->B;",
    );
  });

  it("concatenates multiple text runs with different styles, in order", () => {
    const content = [
      { type: "text", text: "graph TD;\n  A" },
      { type: "text", text: "-->" },
      { type: "text", text: "B;" },
    ];
    expect(extractMermaidSource(content)).toBe("graph TD;\n  A-->B;");
  });

  it("recurses into link items' nested content", () => {
    const content = [
      { type: "text", text: "graph TD; " },
      {
        type: "link",
        href: "https://example.com",
        content: [{ type: "text", text: "A-->B" }],
      },
      { type: "text", text: ";" },
    ];
    expect(extractMermaidSource(content)).toBe("graph TD; A-->B;");
  });

  it("ignores items with no text and no nested content", () => {
    const content = [{ type: "text", text: "graph TD;" }, { type: "unknown" }];
    expect(extractMermaidSource(content)).toBe("graph TD;");
  });
});
