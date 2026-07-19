import "@blocknote/core/fonts/inter.css";
import "@blocknote/mantine/style.css";
import "@blocknote/xl-ai/style.css";
import "./styles.css";

import { createBlockNoteExtension, filterSuggestionItems } from "@blocknote/core";
import { en as coreEnDictionary } from "@blocknote/core/locales";
import { BlockNoteView } from "@blocknote/mantine";
import {
  FormattingToolbar,
  FormattingToolbarController,
  getDefaultReactSlashMenuItems,
  getFormattingToolbarItems,
  SuggestionMenuController,
  useCreateBlockNote,
} from "@blocknote/react";
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
import { runInlineCompletion } from "./bridge/inlineCompletion";
import {
  emptyStateMessage,
  emptyStateTitle,
  inlineSuggestionContextChars,
  inlineSuggestionDebounceMs,
  snapshotDebounceMs,
} from "./constants";
import { createInlineSuggestionExtension } from "./extensions/inlineSuggestionExtension";

function EditorApp() {
  const [bridge, setBridge] = useState<EditorBridge | null>(null);
  const [selectedPageId, setSelectedPageId] = useState<string | null>(null);
  const [isEditable, setIsEditable] = useState(true);
  const [, setIsLoadingDocument] = useState(false);
  const [hasLoadedDocumentOnce, setHasLoadedDocumentOnce] = useState(false);
  const [aiFeaturesEnabled, setAiFeaturesEnabled] = useState(false);
  const [aiAutocompleteEnabled, setAiAutocompleteEnabled] = useState(false);
  // Separate opt-in for inline ghost-text suggestions (issue #59); the
  // extension instance below reads this via a ref (see
  // ai_inline_suggestions_enabled_ref) since it's created once up front but
  // this flag only becomes known asynchronously once the bridge responds to
  // getBridgeInfo().
  const [aiInlineSuggestionsEnabled, setAiInlineSuggestionsEnabled] = useState(false);
  const ai_inline_suggestions_enabled_ref = useRef(false);
  useEffect(() => {
    ai_inline_suggestions_enabled_ref.current = aiInlineSuggestionsEnabled;
  }, [aiInlineSuggestionsEnabled]);
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

  // Inline ghost-text suggestions (issue #59): a separate, new Tiptap
  // extension (not part of createAIExtension()'s toolbar/slash-menu scope),
  // gated by its own opt-in setting via the ref above. Created once, like
  // aiTransport, since useCreateBlockNote()'s extensions are fixed at editor
  // creation time; both the enabled flag and the bridge become known only
  // after this point, so both are read through refs at call time.
  const inlineSuggestionExtension = useMemo(() => {
    const tiptapExtension = createInlineSuggestionExtension({
      isEnabled: () => ai_inline_suggestions_enabled_ref.current,
      isReplacingDocument: () => replacing_document.current,
      fetchCompletion: (contextText, signal) => {
        const bridge = bridge_ref.current;
        if (!bridge) {
          return Promise.reject(new Error("Editor bridge is not ready yet."));
        }
        return runInlineCompletion(bridge, contextText, signal);
      },
      debounceMs: inlineSuggestionDebounceMs,
      contextChars: inlineSuggestionContextChars,
    });
    // useCreateBlockNote()'s `extensions` option expects BlockNoteExtension
    // instances, not raw Tiptap extensions directly (unlike createAIExtension(),
    // whose factory already returns one) — wrap ours via the documented
    // createBlockNoteExtension() escape hatch, which just forwards the
    // Tiptap extension's ProseMirror plugin/keyboard shortcuts through.
    return createBlockNoteExtension({
      key: "cppwikiInlineSuggestion",
      tiptapExtensions: [tiptapExtension],
    });
  }, []);

  const editor = useCreateBlockNote(
    {
      // createAIExtension()'s UI (AIMenuController, AIToolbarButton, slash-menu items)
      // reads its strings from editor.dictionary.ai; useCreateBlockNote()'s default
      // dictionary doesn't include it, so every AI-menu render threw
      // "AI dictionary not found" — merge the xl-ai package's own locale in.
      dictionary: { ...coreEnDictionary, ai: aiEnDictionary },
      extensions: [createAIExtension({ transport: aiTransport }), inlineSuggestionExtension],
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
      setAiInlineSuggestionsEnabled(bridge_info.result.aiInlineSuggestionsEnabled ?? false);

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
                  {/* AI is added alongside BlockNote's standard formatting toolbar items
                      (bold/italic/link/color/etc, from getFormattingToolbarItems()), not
                      instead of them — passing FormattingToolbar only <AIToolbarButton />
                      would drop every standard item. */}
                  <FormattingToolbarController
                    formattingToolbar={() => (
                      <FormattingToolbar>
                        {[...getFormattingToolbarItems(), <AIToolbarButton key="aiButton" />]}
                      </FormattingToolbar>
                    )}
                  />
                  {aiAutocompleteEnabled ? (
                    <SuggestionMenuController
                      triggerCharacter="/"
                      getItems={async (query) =>
                        filterSuggestionItems(
                          [...getDefaultReactSlashMenuItems(editor), ...getAISlashMenuItems(editor)],
                          query,
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
