import { describe, expect, it } from "vitest";
import { renderMarkdownToHtml } from "./markdown";

describe("renderMarkdownToHtml", () => {
  it("renders headings", () => {
    expect(renderMarkdownToHtml("# Title")).toBe("<h1>Title</h1>");
  });

  it("renders emphasis and strong text", () => {
    expect(renderMarkdownToHtml("*em* and **strong**")).toBe(
      "<p><em>em</em> and <strong>strong</strong></p>",
    );
  });

  it("renders GFM tables (remark-gfm)", () => {
    const source = ["| a | b |", "| - | - |", "| 1 | 2 |"].join("\n");
    const html = renderMarkdownToHtml(source);
    expect(html).toContain("<table>");
    expect(html).toContain("<td>1</td>");
  });

  it("renders GFM task lists (remark-gfm)", () => {
    const html = renderMarkdownToHtml("- [x] done\n- [ ] todo");
    expect(html).toContain('type="checkbox"');
    expect(html).toContain("checked");
  });

  it("renders fenced code blocks without executing anything", () => {
    const html = renderMarkdownToHtml("```js\nconsole.log('hi')\n```");
    expect(html).toContain("<pre><code");
    expect(html).toContain("console.log");
  });

  it("passes raw HTML through unsanitized — callers are responsible for DOMPurify", () => {
    // renderMarkdownToHtml is explicitly documented as not sanitizing; this locks in that
    // contract so a future change doesn't silently start sanitizing (or double-sanitizing).
    const html = renderMarkdownToHtml("<script>alert(1)</script>");
    expect(html).toContain("<script>alert(1)</script>");
  });

  it("returns an empty string for empty source", () => {
    expect(renderMarkdownToHtml("")).toBe("");
  });
});
