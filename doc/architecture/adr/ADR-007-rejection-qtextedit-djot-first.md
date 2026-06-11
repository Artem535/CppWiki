# ADR-007 — Rejection of QTextEdit and Djot-First Architecture

## Status

Accepted

## Date

2026-06-11

---

# Context

The original architecture attempted to build a fully native Qt editor using QTextEdit and Djot.

The motivation was:

- native desktop feel;
- C++ ownership of editor;
- simple plain-text document format;
- lower dependency on web technologies.

After analysis, this architecture was found to be high risk.

---

# Problems

## Realtime collaboration

QTextEdit does not provide mature integrations with modern CRDT systems such as Yjs.

A custom bridge would need to handle:

- text operations;
- selections;
- cursors;
- undo/redo;
- paste;
- IME;
- block-level updates;
- remote operations.

## Ecosystem

QTextEdit lacks a modern ecosystem for:

- slash commands;
- Notion-like blocks;
- collaborative editing;
- rich embeds;
- structured block plugins.

## Document model mismatch

Djot is a text format.

The target product is a structured block-based knowledge platform.

---

# Decision

Reject QTextEdit/Djot-first as the primary architecture.

Use:

```text
Qt
+
QWebEngine
+
BlockNote
+
Tiptap
+
ProseMirror
```

Djot remains available as import/export format only.

---

# Consequences

## Positive

- Lower collaboration risk.
- Better modern editor UX.
- Easier custom blocks.
- Better Confluence alignment.

## Negative

- Embedded web runtime.
- Frontend build pipeline.
- Larger distribution size.

---

# Conclusion

The decision favors product feasibility and future collaboration support over a purely native editor implementation.
