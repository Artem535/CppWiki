// Markdown -> HTML conversion for notebook markdown cells (issue #89, "render markdown cells on
// Shift+Enter"). Reuses the remark/rehype pipeline that @blocknote/core already pulls in
// transitively for its own markdown import/export, rather than adding a new markdown dependency.
//
// This function does NOT sanitize its output. Callers MUST run the result through
// DOMPurify.sanitize() before using it in dangerouslySetInnerHTML — mirroring the existing
// text/html output handling in NotebookView.tsx's OutputView component.
import rehypeStringify from "rehype-stringify";
import remarkGfm from "remark-gfm";
import remarkParse from "remark-parse";
import remarkRehype from "remark-rehype";
import { unified } from "unified";

// allowDangerousHtml lets raw HTML embedded in markdown source (e.g. "<b>bold</b>") survive into
// the output HTML, matching classic Jupyter's own markdown-cell rendering. This is exactly the
// untrusted HTML the caller MUST run through DOMPurify.sanitize() before rendering — see the
// module doc comment above.
const markdownProcessor = unified()
  .use(remarkParse)
  .use(remarkGfm)
  .use(remarkRehype, { allowDangerousHtml: true })
  .use(rehypeStringify, { allowDangerousHtml: true });

export function renderMarkdownToHtml(source: string): string {
  return String(markdownProcessor.processSync(source));
}
