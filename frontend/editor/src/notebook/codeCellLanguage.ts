// Maps nbformat's notebook-level language identifier (see resolveKernelLanguage() in nbformat.ts)
// to a CodeMirror 6 language extension for code-cell syntax highlighting (issue #88). Deliberately
// small: only the languages actually installed as dependencies. An unrecognized-but-present
// language (e.g. "r", "julia") intentionally falls back to *no* highlighting extension rather than
// being mislabeled as Python — the "default to Python" behavior in nbformat.ts only applies when
// the notebook has no language info at all.
import type { Extension } from "@codemirror/state";
import { cpp } from "@codemirror/lang-cpp";
import { javascript } from "@codemirror/lang-javascript";
import { python } from "@codemirror/lang-python";

export function languageExtensionFor(language: string): Extension | null {
  switch (language.trim().toLowerCase()) {
    case "python":
    case "python3":
    case "ipython":
      return python();
    case "javascript":
    case "node":
    case "nodejs":
      return javascript();
    case "typescript":
      return javascript({ typescript: true });
    case "jsx":
      return javascript({ jsx: true });
    case "tsx":
      return javascript({ jsx: true, typescript: true });
    case "c":
    case "c++":
    case "cpp":
      return cpp();
    default:
      return null;
  }
}
