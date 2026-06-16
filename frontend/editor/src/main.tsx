import "@blocknote/core/fonts/inter.css";
import "@blocknote/mantine/style.css";
import "./styles.css";

import { BlockNoteView } from "@blocknote/mantine";
import { useCreateBlockNote } from "@blocknote/react";
import { useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import { createEditorBridge } from "./bridge";
import {
  bridgeApiVersion,
  type EditorBridge,
  type InitialDocumentSnapshot,
} from "./bridge/editorBridge";
import {
  emptyStateMessage,
  emptyStateTitle,
  snapshotDebounceMs,
} from "./constants";

function EditorApp() {
  const [bridge, setBridge] = useState<EditorBridge | null>(null);
  const [selectedPageId, setSelectedPageId] = useState<string | null>(null);
  const [, setIsLoadingDocument] = useState(false);
  const [hasLoadedDocumentOnce, setHasLoadedDocumentOnce] = useState(false);
  const editorTheme: "dark" = "dark";
  const snapshot_timer = useRef<number | null>(null);
  const replacing_document = useRef(false);
  const selected_page_id = useRef<string | null>(null);

  const editor = useCreateBlockNote({}, []);
  const showOverlay = !selectedPageId;
  const shouldMountEditor = hasLoadedDocumentOnce;

  const applyLoadedBlocks = (
    blocks: InitialDocumentSnapshot,
  ) => {
    const currentBlockIds = editor.document
      .map((block) => block.id)
      .filter((id): id is string => typeof id === "string" && id.length > 0);

    if (blocks.length === 0) {
      if (currentBlockIds.length > 0) {
        editor.replaceBlocks(currentBlockIds, [
          {
            type: "paragraph",
            content: [],
          },
        ]);
      }
      return;
    }

    if (currentBlockIds.length > 0) {
      editor.replaceBlocks(currentBlockIds, blocks);
    }
  };

  const flushAutosave = async (activeBridge: EditorBridge | null) => {
    if (!activeBridge || !selected_page_id.current) {
      return;
    }

    if (snapshot_timer.current !== null) {
      window.clearTimeout(snapshot_timer.current);
      snapshot_timer.current = null;
    }

    await activeBridge.updateSnapshot(editor.document);
  };

  useEffect(() => {
    let cancelled = false;
    const unsubscribers: Array<() => void> = [];

    void createEditorBridge().then(async (created_bridge) => {
      if (cancelled) {
        return;
      }

      setBridge(created_bridge);

      const bridge_info = await created_bridge.getBridgeInfo();
      if (
        !bridge_info.ok ||
        bridge_info.result.apiVersion !== bridgeApiVersion ||
        bridge_info.result.namespace !== "wiki.documents"
      ) {
        console.error("Unsupported editor bridge contract", bridge_info);
        return;
      }

      unsubscribers.push(
        created_bridge.onDocumentOpenRequested((pageId) => {
          setIsLoadingDocument(true);
          void flushAutosave(created_bridge).then(async () => {
            const response = await created_bridge.openDocument(pageId);
            if (!response.ok) {
              console.error("Failed to open document", response.error);
              setIsLoadingDocument(false);
            }
          });
        }),
      );

      const unsubscribeLoaded = created_bridge.onDocumentLoaded((document) => {
        replacing_document.current = true;
        setHasLoadedDocumentOnce(true);
        applyLoadedBlocks(document.blocks);
        selected_page_id.current = document.id;
        setSelectedPageId(document.id);
        window.setTimeout(() => {
          replacing_document.current = false;
          setIsLoadingDocument(false);
        }, 0);
      });

      const unsubscribeLoadFailed = created_bridge.onDocumentLoadFailed(
        (_pageId, message) => {
          console.error("Failed to load document", message);
          setIsLoadingDocument(false);
        },
      );
      const unsubscribeSelectionCleared =
        created_bridge.onDocumentSelectionCleared(() => {
          selected_page_id.current = null;
          setSelectedPageId(null);
          setIsLoadingDocument(false);
        });
      unsubscribers.push(
        unsubscribeLoaded,
        unsubscribeLoadFailed,
        unsubscribeSelectionCleared,
      );

      if (cancelled) {
        unsubscribers.forEach((unsubscribe) => {
          unsubscribe();
        });
      }
    });

    return () => {
      cancelled = true;
      if (snapshot_timer.current !== null) {
        window.clearTimeout(snapshot_timer.current);
      }
      unsubscribers.forEach((unsubscribe) => {
        unsubscribe();
      });
    };
  }, [editor]);

  const handleEditorChange = () => {
    if (!bridge || !selectedPageId || replacing_document.current) {
      return;
    }

    if (snapshot_timer.current !== null) {
      window.clearTimeout(snapshot_timer.current);
    }

    snapshot_timer.current = window.setTimeout(() => {
      snapshot_timer.current = null;
      void bridge.updateSnapshot(editor.document);
    }, snapshotDebounceMs);
  };

  return (
    <main className="app-shell">
      <section className="editor-pane" aria-label="Document editor">
        {shouldMountEditor ? (
          <div
            className={`editor-surface${showOverlay ? " editor-surface--hidden" : ""}`}
          >
            <BlockNoteView
              editor={editor}
              onChange={handleEditorChange}
              theme={editorTheme}
            />
          </div>
        ) : null}
        {showOverlay ? (
          <div className="empty-state">
            <h1>{emptyStateTitle}</h1>
            <p>{emptyStateMessage}</p>
          </div>
        ) : null}
      </section>
    </main>
  );
}

const root = document.getElementById("root");
if (!root) {
  throw new Error("Root element was not found");
}

createRoot(root).render(<EditorApp />);
