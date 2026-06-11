# ADR-003 — Collaboration Strategy

## Status

Accepted

## Date

2026-06-11

---

# Context

Realtime collaboration is a mandatory product requirement.

However, implementing full realtime editing in the MVP would significantly increase delivery risk.

The chosen editor stack already provides a future path toward Yjs-based collaboration.

---

# Decision

Use a staged collaboration strategy.

## MVP

Use:

```text
Single Writer / Multi Reader
```

Only one user can edit a document at the same time.

Other users can view the document and see presence.

## Future version

Implement realtime collaboration using:

```text
BlockNote
    ↓
Tiptap
    ↓
ProseMirror
    ↓
Yjs
    ↓
WebSocket Provider
```

---

# Consequences

## Positive

- MVP is much simpler.
- No text-level merge conflicts in MVP.
- Architecture still prepares for realtime collaboration.
- Locking model is acceptable for many corporate wiki workflows.

## Negative

- MVP does not provide simultaneous editing.
- Phase 2 requires additional collaboration infrastructure.

---

# Required architectural constraint

All document and block IDs must be stable from MVP onward.

This avoids migration pain when introducing Yjs/realtime collaboration later.
