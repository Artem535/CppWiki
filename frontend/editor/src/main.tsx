import "@blocknote/core/fonts/inter.css";
import "@blocknote/mantine/style.css";
import "./styles.css";

import { BlockNoteView } from "@blocknote/mantine";
import { useCreateBlockNote } from "@blocknote/react";
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

function EditorApp() {
  const editor = useCreateBlockNote({
    initialContent: [
      {
        type: "heading",
        props: {
          level: 1,
        },
        content: "CppWiki",
      },
      {
        type: "paragraph",
        content:
          "This is the first BlockNote document running inside Qt QWebEngine.",
      },
      {
        type: "bulletListItem",
        content: "Desktop shell: Qt 6 Widgets",
      },
      {
        type: "bulletListItem",
        content: "Editor host: QWebEngine",
      },
      {
        type: "bulletListItem",
        content: "Editor runtime: BlockNote / Tiptap / ProseMirror",
      },
    ],
  });

  return (
    <main className="app-shell">
      <aside className="sidebar" aria-label="Workspace navigation">
        <div className="workspace-title">CppWiki</div>
        <button className="nav-item is-active" type="button">
          Getting Started
        </button>
        <button className="nav-item" type="button">
          Architecture
        </button>
        <button className="nav-item" type="button">
          Sync Status
        </button>
      </aside>
      <section className="editor-pane" aria-label="Document editor">
        <header className="document-header">
          <div>
            <p className="eyebrow">Local draft</p>
            <h1>Getting Started</h1>
          </div>
          <span className="status-pill">QWebEngine</span>
        </header>
        <BlockNoteView editor={editor} theme="light" />
      </section>
    </main>
  );
}

const root = document.getElementById("root");
if (!root) {
  throw new Error("Root element was not found");
}

createRoot(root).render(
  <StrictMode>
    <EditorApp />
  </StrictMode>,
);
