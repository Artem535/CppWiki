// CodeMirror 6-backed editor for Jupyter notebook code cells (issue #88, follow-up to #52).
// Display/editing only — no execution affordance, ever (see NotebookView.tsx's top-of-file note).
// Wraps CodeMirror directly (rather than pulling in a React wrapper package) so the only new
// dependencies are the official @codemirror/* packages, and so this component can precisely mirror
// the plain-textarea contract it replaces for code cells: an uncontrolled-ish value prop synced in
// on external change (cell switch, notebook reload), readOnly gating, and a plain onChange(text)
// callback — no CodeMirror types leak into NotebookView.tsx's onSourceChange wiring.
import { useEffect, useRef } from "react";

import { Annotation, EditorState } from "@codemirror/state";
import { EditorView, keymap, lineNumbers } from "@codemirror/view";
import { defaultKeymap, history, historyKeymap } from "@codemirror/commands";
import { bracketMatching, indentOnInput, syntaxHighlighting } from "@codemirror/language";
import { oneDarkTheme, oneDarkHighlightStyle } from "@codemirror/theme-one-dark";

import { languageExtensionFor } from "./codeCellLanguage";

// Marks a dispatched transaction as originating from this component's own external-value sync
// (props.value changed underneath it, e.g. switching cells) rather than user typing, so the
// updateListener below doesn't loop it straight back into onChange.
const externalSyncAnnotation = Annotation.define<boolean>();

export function CodeCellEditor({
  value,
  editable,
  language,
  onChange,
}: {
  value: string;
  editable: boolean;
  language: string;
  onChange: (value: string) => void;
}) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const viewRef = useRef<EditorView | null>(null);
  const onChangeRef = useRef(onChange);
  onChangeRef.current = onChange;

  // Mount/unmount CodeMirror exactly once per component instance (NotebookView keys CellView by
  // cell index, so a cell being deleted/inserted elsewhere naturally remounts the ones that shift).
  useEffect(() => {
    if (!containerRef.current) {
      return undefined;
    }
    const langExtension = languageExtensionFor(language);
    const view = new EditorView({
      parent: containerRef.current,
      state: EditorState.create({
        doc: value,
        extensions: [
          lineNumbers(),
          history(),
          bracketMatching(),
          indentOnInput(),
          keymap.of([...defaultKeymap, ...historyKeymap]),
          oneDarkTheme,
          syntaxHighlighting(oneDarkHighlightStyle, { fallback: true }),
          ...(langExtension ? [langExtension] : []),
          EditorState.readOnly.of(!editable),
          EditorView.editable.of(editable),
          EditorView.lineWrapping,
          EditorView.updateListener.of((update) => {
            if (!update.docChanged) {
              return;
            }
            if (update.transactions.some((tr) => tr.annotation(externalSyncAnnotation))) {
              return;
            }
            onChangeRef.current(update.state.doc.toString());
          }),
        ],
      }),
    });
    viewRef.current = view;
    return () => {
      view.destroy();
      viewRef.current = null;
    };
    // language/editable are fixed for the lifetime of a given cell instance in practice (notebook
    // language is notebook-wide and cell identity is index-keyed by the parent); value is synced
    // separately below without tearing down/recreating the view (which would lose cursor/undo
    // history on every keystroke, since onChange -> parent state update -> new value prop).
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Push external value changes (cell switched underneath us, notebook reloaded) into CodeMirror
  // without clobbering it on every keystroke: onChange -> parent setState -> this same value comes
  // back down as a prop, and re-inserting it here would reset cursor position/undo history mid-edit.
  useEffect(() => {
    const view = viewRef.current;
    if (!view) {
      return;
    }
    const current = view.state.doc.toString();
    if (current !== value) {
      view.dispatch({
        changes: { from: 0, to: current.length, insert: value },
        annotations: externalSyncAnnotation.of(true),
      });
    }
  }, [value]);

  return <div className="notebook-code-editor" ref={containerRef} />;
}
