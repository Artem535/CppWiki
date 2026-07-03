# Backlog

This file tracks intentionally deferred work that is useful, but not part of the current implementation step.

## Desktop UI

- Replace the current `StatusBadgeWidget + QLabel` status-line widgets with cleaner badge-only status chips.
- Remove the textual status copy from document/backend indicators once the visual badge states are expressive enough on their own.
- Revisit the visual design of the desktop status line so document state and backend state read as compact product UI, not debug labels.
- Rework lock/auth/backend wording and status presentation so collaboration state reads as product UX instead of transport/debug output.
- Add conflict surfacing UI that consumes explicit sync conflict metadata instead of implicit sync error strings.
- Add manual conflict resolution flow for preserved local vs remote snapshots.
