import { Extension } from "@tiptap/core";
import { Plugin, PluginKey, TextSelection } from "prosemirror-state";
import { Decoration, DecorationSet } from "prosemirror-view";
import type { EditorState, Transaction } from "prosemirror-state";
import { InlineSuggestionScheduler } from "./inlineSuggestionScheduler";

/**
 * Ghost-text inline suggestions (issue #59): a new, separate Tiptap
 * extension (not part of `@blocknote/xl-ai`'s `createAIExtension`), since
 * this is a continuous/automatic interaction pattern rather than the
 * existing manually-triggered rewrite/slash-menu features.
 *
 * - Fires a debounced completion request (via `InlineSuggestionScheduler`)
 *   after the user pauses typing at a collapsed cursor.
 * - Renders the result as a ProseMirror widget decoration positioned right
 *   at the cursor.
 * - Tab accepts the suggestion (inserts it, clears the decoration) when one
 *   is showing; otherwise Tab is left alone so BlockNote's own Tab bindings
 *   (list indent, etc.) still apply. `priority` is set high so this
 *   extension's keymap is consulted before BlockNote's defaults, but still
 *   falls through (returns false) when there is nothing to accept.
 */

export interface InlineSuggestionExtensionOptions {
  // A getter rather than a plain boolean: the setting can flip at runtime
  // (Settings dialog) and the extension instance is created once up front,
  // matching the pattern used for the bridge/AI-transport getters elsewhere
  // in this app (see main.tsx's `bridge_ref`).
  isEnabled: () => boolean;
  fetchCompletion: (contextText: string, signal: AbortSignal) => Promise<string>;
  debounceMs: number;
  contextChars: number;
}

interface InlineSuggestionPluginState {
  decorations: DecorationSet;
  suggestion: string | null;
  from: number | null;
}

type SetSuggestionMeta = { type: "set"; pos: number; text: string };
type ClearSuggestionMeta = { type: "clear" };
type InlineSuggestionMeta = SetSuggestionMeta | ClearSuggestionMeta;

export const inlineSuggestionPluginKey = new PluginKey<InlineSuggestionPluginState>(
  "cppwikiInlineSuggestion",
);

function ghostTextWidget(pos: number, text: string): Decoration {
  return Decoration.widget(
    pos,
    () => {
      const span = document.createElement("span");
      span.className = "cppwiki-ghost-text";
      span.textContent = text;
      span.setAttribute("aria-hidden", "true");
      return span;
    },
    { side: 1, ignoreSelection: true },
  );
}

/** Extracts up to `contextChars` of plain text immediately preceding `pos`. */
function extractPrecedingContext(state: EditorState, pos: number, contextChars: number): string {
  const start = Math.max(0, pos - contextChars);
  return state.doc.textBetween(start, pos, "\n", "\n");
}

export function createInlineSuggestionExtension(options: InlineSuggestionExtensionOptions) {
  const scheduler = new InlineSuggestionScheduler(
    {
      fetchCompletion: options.fetchCompletion,
      onSuggestion: (suggestion, forContextText) => {
        const view = latestView;
        if (!view) {
          return;
        }
        const { state } = view;
        const { selection } = state;
        if (!(selection instanceof TextSelection) || !selection.empty) {
          return;
        }
        // Guard against the cursor having moved on (without a doc change,
        // e.g. arrow keys) since this request was scheduled: only show the
        // suggestion if the preceding context still matches what we asked
        // the provider to continue.
        const currentContext = extractPrecedingContext(
          state,
          selection.from,
          options.contextChars,
        );
        if (currentContext !== forContextText) {
          return;
        }
        const tr = state.tr.setMeta(inlineSuggestionPluginKey, {
          type: "set",
          pos: selection.from,
          text: suggestion,
        } satisfies SetSuggestionMeta);
        view.dispatch(tr);
      },
    },
    options.debounceMs,
  );

  // The ProseMirror plugin only gets `apply(tr, state)`, not the view, so we
  // stash the latest view here (set in `view()`) for the scheduler's
  // callback and for reading state when scheduling a new request.
  let latestView: import("prosemirror-view").EditorView | null = null;

  const plugin: Plugin<InlineSuggestionPluginState> = new Plugin<InlineSuggestionPluginState>({
    key: inlineSuggestionPluginKey,
    state: {
      init: () => ({ decorations: DecorationSet.empty, suggestion: null, from: null }),
      apply(tr: Transaction, previous) {
        const meta = tr.getMeta(inlineSuggestionPluginKey) as InlineSuggestionMeta | undefined;
        if (meta?.type === "set") {
          return {
            decorations: DecorationSet.create(tr.doc, [ghostTextWidget(meta.pos, meta.text)]),
            suggestion: meta.text,
            from: meta.pos,
          };
        }
        if (meta?.type === "clear") {
          return { decorations: DecorationSet.empty, suggestion: null, from: null };
        }
        if (tr.docChanged && previous.suggestion !== null) {
          // Any other doc change (typing, accept-via-other-path, etc.)
          // dismisses a currently-showing suggestion (issue #59: "continuing
          // to type dismisses it").
          return { decorations: DecorationSet.empty, suggestion: null, from: null };
        }
        if (tr.docChanged) {
          return { decorations: previous.decorations.map(tr.mapping, tr.doc), suggestion: previous.suggestion, from: previous.from };
        }
        return previous;
      },
    },
    props: {
      decorations(state): DecorationSet | null {
        return plugin.getState(state)?.decorations ?? null;
      },
    },
    view(editorView) {
      latestView = editorView;
      return {
        destroy() {
          if (latestView === editorView) {
            latestView = null;
          }
          scheduler.dispose();
        },
      };
    },
  });

  const extension = Extension.create<InlineSuggestionExtensionOptions>({
    name: "cppwikiInlineSuggestion",
    // Higher than Tiptap/BlockNote's implicit default (100), so our Tab
    // binding gets first refusal and only falls through to list-indent/etc.
    // when there is no suggestion to accept.
    priority: 1000,

    addOptions() {
      return options;
    },

    addProseMirrorPlugins() {
      return [plugin];
    },

    onTransaction({ transaction, editor }) {
      if (!options.isEnabled()) {
        scheduler.cancel();
        return;
      }
      // Ignore transactions we dispatched ourselves (setting/clearing the
      // decoration) to avoid re-triggering scheduling in a loop.
      if (transaction.getMeta(inlineSuggestionPluginKey)) {
        return;
      }
      if (!transaction.docChanged && !transaction.selectionSet) {
        return;
      }

      const { state } = editor;
      const { selection } = state;
      if (!(selection instanceof TextSelection) || !selection.empty) {
        scheduler.cancel();
        return;
      }

      const contextText = extractPrecedingContext(state, selection.from, options.contextChars);
      scheduler.schedule(contextText);
    },

    addKeyboardShortcuts() {
      return {
        Tab: () => {
          const view = this.editor.view;
          const pluginState = inlineSuggestionPluginKey.getState(view.state);
          if (!pluginState || pluginState.suggestion === null || pluginState.from === null) {
            return false;
          }

          const { suggestion, from } = pluginState;
          const tr = view.state.tr
            .insertText(suggestion, from)
            .setMeta(inlineSuggestionPluginKey, { type: "clear" } satisfies ClearSuggestionMeta);
          view.dispatch(tr);
          scheduler.cancel();
          return true;
        },
      };
    },
  });

  return extension;
}
