import "@blocknote/core/fonts/inter.css";
import "@blocknote/mantine/style.css";
import "@blocknote/xl-ai/style.css";
import "./styles.css";

import { en as coreEnDictionary } from "@blocknote/core/locales";
import { BlockNoteView } from "@blocknote/mantine";
import { FormattingToolbar, FormattingToolbarController, SuggestionMenuController, useCreateBlockNote } from "@blocknote/react";
import { AIMenuController, AIToolbarButton, createAIExtension, getAISlashMenuItems } from "@blocknote/xl-ai";
import { en as aiEnDictionary } from "@blocknote/xl-ai/locales";
import { useEffect, useMemo, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import { createEditorBridge } from "./bridge";
import { BridgeChatTransport } from "./bridge/aiChatTransport";
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
  const [isEditable, setIsEditable] = useState(true);
  const [, setIsLoadingDocument] = useState(false);
  const [hasLoadedDocumentOnce, setHasLoadedDocumentOnce] = useState(false);
  const [aiFeaturesEnabled, setAiFeaturesEnabled] = useState(false);
  const [aiAutocompleteEnabled, setAiAutocompleteEnabled] = useState(false);
  const editorTheme: "dark" = "dark";
  const snapshot_timer = useRef<number | null>(null);
  const replacing_document = useRef(false);
  const selected_page_id = useRef<string | null>(null);
  // The bridge is created asynchronously (see effect below); this ref lets
  // the AI transports (constructed once, up front) reach the live bridge
  // instance once it exists, without recreating the editor.
  const bridge_ref = useRef<EditorBridge | null>(null);

  // ADR-012: forwards every AI request (both the formatting-toolbar rewrite
  // and the slash-command autocomplete — MVP scope, ADR-010) through
  // EditorBridge, never fetching directly from this JS context. `mode` is a
  // best-effort label used only for the C++ side's system-prompt prefix; the
  // AIExtension itself decides which UI action triggered a given request.
  const aiTransport = useMemo(
    () => new BridgeChatTransport(() => bridge_ref.current, "rewrite"),
    [],
  );

  const editor = useCreateBlockNote(
    {
      // createAIExtension()'s UI (AIMenuController, AIToolbarButton, slash-menu items)
      // reads its strings from editor.dictionary.ai; useCreateBlockNote()'s default
      // dictionary doesn't include it, so every AI-menu render threw
      // "AI dictionary not found" — merge the xl-ai package's own locale in.
      dictionary: { ...coreEnDictionary, ai: aiEnDictionary },
      extensions: [createAIExtension({ transport: aiTransport })],
    },
    [],
  );
  const showOverlay = !selectedPageId;
  const shouldMountEditor = hasLoadedDocumentOnce;

  useEffect(() => {
    editor.isEditable = isEditable;
  }, [editor, isEditable]);

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
    if (!activeBridge || !selected_page_id.current || !isEditable) {
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
      bridge_ref.current = created_bridge;

      const bridge_info = await created_bridge.getBridgeInfo();
      if (
        !bridge_info.ok ||
        bridge_info.result.apiVersion !== bridgeApiVersion ||
        bridge_info.result.namespace !== "wiki.documents"
      ) {
        console.error("Unsupported editor bridge contract", bridge_info);
        return;
      }

      setAiFeaturesEnabled(bridge_info.result.aiFeaturesEnabled ?? false);
      setAiAutocompleteEnabled(bridge_info.result.aiAutocompleteEnabled ?? false);

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
        setIsEditable(document.editable);
        window.setTimeout(() => {
          replacing_document.current = false;
          setIsLoadingDocument(false);
        }, 0);
      });

      const unsubscribeAccessChanged = created_bridge.onDocumentAccessChanged(
        (editable) => {
          setIsEditable(editable);
        },
      );

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
          setIsEditable(true);
          setIsLoadingDocument(false);
        });
      unsubscribers.push(
        unsubscribeLoaded,
        unsubscribeAccessChanged,
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
    if (!bridge || !selectedPageId || !isEditable || replacing_document.current) {
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
              editable={isEditable}
              onChange={handleEditorChange}
              theme={editorTheme}
              formattingToolbar={!aiFeaturesEnabled}
            >
              {aiFeaturesEnabled ? (
                <>
                  <AIMenuController />
                  <FormattingToolbarController
                    formattingToolbar={() => (
                      <FormattingToolbar>
                        <AIToolbarButton />
                      </FormattingToolbar>
                    )}
                  />
                  {aiAutocompleteEnabled ? (
                    <SuggestionMenuController
                      triggerCharacter="/"
                      getItems={async (query) =>
                        getAISlashMenuItems(editor).filter((item) =>
                          item.title.toLowerCase().includes(query.toLowerCase()),
                        )
                      }
                    />
                  ) : null}
                </>
              ) : null}
            </BlockNoteView>
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
