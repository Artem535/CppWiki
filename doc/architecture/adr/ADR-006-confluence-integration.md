# ADR-006 — Confluence Integration

## Status

Accepted

## Date

2026-06-11

---

# Context

Confluence stores documents in formats such as ADF and Storage Format.

The previous Djot-first architecture created unnecessary conversion layers.

---

# Decision

Use internal structured document model as the central conversion point.

```text
Document JSON
    ↔ Internal AST
    ↔ ADF
    ↔ Confluence Storage Format
    ↔ HTML
    ↔ Djot / Markdown
```

---

# Consequences

## Positive

- Better alignment with Confluence structured documents.
- Custom blocks can map to Confluence macros or placeholders.
- Unsupported macros can be preserved.
- Reduced dependency on plain-text roundtrip quality.

## Negative

- Requires custom converters.
- Full Confluence fidelity is still difficult.
- Unsupported macros require fallback strategy.
