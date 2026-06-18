# Realistic Delivery Pipeline

**Product:** CppWiki / Wiki Platform v9 - Block Document Edition  
**Status:** Draft delivery plan  
**Date:** 2026-06-16
**Related docs:** `doc/PRD_v9_Block_Document_Edition.md`, `doc/roadmap/Current_Roadmap.md`, `doc/architecture/Architecture_Baseline_Libraries_and_Approaches.md`

---

# 1. Purpose

This pipeline turns the current architecture roadmap into a realistic build order.

The project currently has a working Qt shell, `QWebEngineView`, local Vite/React bundle loading, BlockNote rendering, QWebChannel bridge, typed document validation, local repository boundary and a minimal page list path. The current frontend may contain temporary navigation only as scaffolding. Product navigation, workspace UI and application chrome belong to Qt.

The remaining work should be delivered in risk-reducing slices, not by implementing the full PRD surface at once.

The main rule: close one vertical path before widening the product. The first vertical path is:

```text
BlockNote edit
  -> QWebChannel event
  -> typed C++ document DTO
  -> validation
  -> local save
  -> page list/load
  -> restart
  -> local load
```

Sync, auth, plugins, Confluence and realtime collaboration should not be started as full implementations until this loop is stable.

---

# 2. Delivery Principles

1. Keep the MVP offline-first and deterministic.
2. Treat Block Document JSON as the only source of truth.
3. Treat Qt as the application foundation and owner of the product shell.
4. Build the C++/TypeScript boundary before building backend features.
5. Add external systems only after local save/load is proven.
6. Every phase must leave the app runnable.
7. Every risky integration gets a spike and an exit gate.
8. Do not start realtime collaboration until stable IDs, document validation and persistence exist.

## 2.1. UI Ownership Boundary

Qt owns the desktop application surface:

- main window;
- workspace selector;
- page tree and list/tree widgets;
- document tabs;
- document title area;
- sync, save, lock and offline status;
- account menu;
- settings dialogs;
- plugin manager;
- Confluence sync status;
- native dialogs and notifications.

JavaScript owns only editor-local widgets that must live inside the web editor runtime:

- BlockNote editor surface;
- block handles;
- slash menu;
- inline formatting controls;
- editor-only block previews;
- custom block widgets that cannot be implemented as native Qt widgets because they depend on BlockNote, Tiptap or ProseMirror internals.

Any UI that can be a Qt widget should be a Qt widget. Use JavaScript when the widget is part of the BlockNote editing experience or when implementing it natively would break editor integration.

The web editor must not own application navigation, workspace state, persistence, auth, sync, permissions, plugin lifecycle or Confluence operations. It sends document/editor events through QWebChannel and receives explicit commands/state from C++.

---

# 3. Phase 0 - Baseline Stabilization

## Goal

Make the existing editor host reproducible for local development and CI.

## Scope

- Verify `frontend/editor` builds from a clean checkout.
- Verify Qt app loads `frontend/editor/dist/index.html`.
- Keep fallback HTML when the bundle is missing.
- Mark any frontend navigation/sidebar as temporary demo scaffolding.
- Fix duplicate or noisy CMake entries if found during normal work.
- Document local build commands.
- Add a minimal smoke check for frontend build.

## Exit Gate

- `npm run build` succeeds in `frontend/editor`.
- CMake configure succeeds.
- Desktop app starts and loads the editor bundle from disk.
- Missing bundle still shows fallback HTML.

## Do Not Include

- QWebChannel contract.
- Persistence.
- Product shell redesign.
- Moving product navigation into JavaScript.

---

# 4. Phase 1 - QWebChannel Contract

## Goal

Create the first reliable bridge between BlockNote and the C++ core.

## Scope

- Add `editor_bridge` C++ module or target.
- Expose one QObject through QWebChannel.
- Define a versioned request/response/error envelope.
- Add `wiki.documents.getInitialDocument`.
- Add `wiki.documents.updateSnapshot`.
- Log editor document changes in C++ with `spdlog`.
- Add a small TypeScript bridge wrapper.
- Keep frontend usable outside Qt with a mock bridge.
- Keep the TypeScript wrapper scoped to editor events and editor commands.

## Exit Gate

- Frontend can call C++ through QWebChannel.
- C++ can provide initial document JSON.
- BlockNote change events are observable in C++ logs.
- Error responses have a stable shape.
- Bridge API version is visible on both sides.

## Do Not Include

- Real persistence.
- Sync.
- Auth.
- Multiple documents.
- Application shell state in JavaScript.

---

# 4.5. Phase 1.5 - Runtime Foundation and CBLite Proof

## Goal

Put shared runtime configuration and dependency proof points in place before introducing typed document DTOs.

## Scope

- Add a central `ProgramSettings` class for application runtime paths and metadata.
- Move shared application and bridge constants into one `constants` header.
- Keep editor bundle path, app data path and database path behind settings rather than scattered literals.
- Add a CTest-covered bridge contract test.
- Add a CTest-covered settings test.
- Add a Couchbase Lite C++ API smoke test behind `CPPWIKI_ENABLE_CBLITE_STORAGE`.
- Keep Couchbase Lite optional and outside GUI/public app headers.

## Exit Gate

- Default configure/build/test works without Couchbase Lite installed.
- When `CPPWIKI_ENABLE_CBLITE_STORAGE=ON` and `CPPWIKI_CBLITE_ROOT` points to a valid package, CTest opens, closes and deletes a temporary Couchbase Lite database through the C++ wrapper.
- Runtime paths are available from `ProgramSettings`.
- Shared constants are not duplicated across C++ bridge and Qt host code.

## Do Not Include

- Real document persistence.
- Repository implementation.
- Couchbase document schema.
- Sync Gateway or replication.

---

# 5. Phase 2 - Document Model Skeleton

## Goal

Make document data typed, versioned and reject invalid payloads before storage exists.

## Scope

- Add `core` or `document` module for document DTOs.
- Define page metadata DTO.
- Define reflect-cpp DTOs for BlockNote snapshots.
- Preserve the validated raw BlockNote snapshot JSON for lossless persistence.
- Support the current BlockNote block set:
  - paragraph;
  - heading;
  - quote;
  - bullet list item;
  - numbered list item;
  - checklist item;
  - toggle list item;
  - code block;
  - table;
  - image;
  - video;
  - audio;
  - file;
  - divider;
  - page break.
- Add `schema_version`.
- Add stable page ID and block ID rules.
- Add basic validation:
  - required IDs;
  - supported block types;
  - valid heading levels;
  - duplicate block IDs across nested blocks.
- Add fixture tests for valid and invalid BlockNote documents.
- Do not duplicate BlockNote-owned payload limits in C++ validation.

## Exit Gate

- Sample BlockNote document serializes and deserializes through C++ DTOs.
- Validated snapshots keep raw JSON for the persistence phase.
- Invalid payloads are rejected with structured errors.
- Fixture tests run in CTest.
- Bridge only accepts validated document snapshots.

## Do Not Include

- Couchbase Lite.
- Import/export conversion.
- Custom application-specific block types beyond the BlockNote schema.

---

# 6. Phase 3 - Local Persistence Vertical Slice

## Goal

Close the first real user path: edit, save, restart, load.

## Scope

- Choose the practical local persistence implementation.
- If Couchbase Lite C++ packaging is blocked, use a temporary repository interface with a file-backed JSON implementation and keep the Couchbase decision explicit.
- Add `LocalDocumentRepository` interface.
- Persist one workspace and multiple page records.
- Add repository methods for saving, loading and listing documents.
- Save validated document snapshots.
- Add UUID generation for document IDs.
- Bootstrap a default `Welcome to CppWiki` page when the repository is empty.
- Add a temporary left-side list view so users can select a page before editing.
- Show an empty/loading hint widget before a document is selected.
- Autosave the currently selected page.
- Preserve the previous valid version on save failure.
- Load selected document snapshots through the bridge.
- Surface persistence errors through bridge responses and logs.
- Add tests for repository save/load/list behavior and bridge list/load/update behavior.

## Exit Gate

- Empty repository creates a default welcome page.
- Page list is loaded from the repository.
- User can select a page, edit it and autosave locally.
- Restart can load the saved valid document by ID.
- Invalid documents are not persisted.
- Failed save does not destroy the previous valid document.
- Save/load/list failures are visible through bridge errors and logs.

## Do Not Include

- Sync Gateway.
- Conflict resolution.
- Authenticated access.
- Attachment blobs.
- Final tree navigation.
- Document hash based dirty checking.

---

# 6.5. Phase 3.5 - Navigation and Dirty-State Hardening

## Goal

Stabilize the Phase 3 document lifecycle and harden the native navigation shell before moving into the full product shell.

## Scope

- Add explicit selected-document state to the C++ application layer.
- Extend page metadata for hierarchy-ready navigation:
  - `parent_id`;
  - `sort_order`;
  - `workspace_id` if the repository needs it before real workspaces;
  - `created_at`;
  - `updated_at`.
- Keep the first UI pass as list view, but read it from the hierarchy-ready model.
- Move page navigation ownership from React to a Qt widget.
- Move from the temporary list widget to a Qt tree view after hierarchy metadata and selected-document state are stable.
- Finish row-level navigation actions in the tree:
  - add-child button;
  - right-click context menu;
  - delete;
  - move up/down;
  - drag-and-drop reparenting and reordering.
- Add application theme state owned by C++/Qt.
- Add light/dark theme switching through Qlementine.
- Propagate the active theme to the WebView editor so BlockNote uses matching colors.
- Decide whether to store a document hash for quick dirty checks.
- Add skip-save behavior only if it measurably simplifies autosave or sync preparation.
- Add manual QA checklist for empty repository, first page, selection, autosave, restart and corrupted document cases.

## Exit Gate

- Page metadata can represent both flat lists and trees.
- The app can still run with a flat list while tree metadata is present.
- Navigation selection is owned by C++/Qt, not by React application state.
- Tree view can be introduced without changing the stored document snapshot payload.
- Row actions work consistently through click targets, context menu and drag-and-drop.
- Light/dark switching has a single source of truth in the Qt application layer.
- The WebView editor background and text colors match the active Qlementine theme.
- Dirty-state behavior is explicit: either hash-based or snapshot-comparison based.
- Native tree navigation is stable enough to replace React-owned chrome.
- Page navigation data model can evolve from list view to tree view without changing stored document payloads.
- Autosave does not write when no document is selected.
- Corrupted stored snapshots fail safely.

## Do Not Include

- Sync Gateway.
- Full permissions model.
- Confluence import/export.
- Plugin runtime.

---

# 7. Phase 4 - Minimal Product Shell

## Goal

Replace the proof-of-connection UI with a minimal Qt-owned wiki workspace shell.

## Scope

- Add page tree placeholder as a Qt widget.
- Start from list view if hierarchy metadata is still incomplete.
- Add document title area in Qt.
- Add editor save status in Qt.
- Add offline/online status placeholder in Qt.
- Add sync status placeholder in Qt.
- Add settings dialog in Qt using Qlementine widgets for font size and database folder access.
- Add remaining settings/action placeholders in Qt.
- Remove or hide frontend demo navigation/sidebar from the product view.
- Keep the JavaScript bundle focused on the BlockNote editor surface.
- Keep editor as the primary surface.
- Keep layout stable during resize.
- Keep Qlementine as the application style baseline unless a stronger Qt theme decision replaces it.
- Expose theme switching from the native shell, not from the editor bundle.

## Exit Gate

- User can distinguish workspace navigation from document editing.
- Save/load status is visible.
- Editor remains usable at target desktop sizes.
- The shell does not imply unfinished features are working.
- Page tree, title and status are native Qt UI, not React application chrome.
- The web view contains the editor widget, not the application shell.
- Switching between light and dark themes updates both Qt widgets and the WebView editor.
- Native settings dialog uses Qlementine widgets instead of plain Qt form controls and only exposes the current user-facing appearance/runtime actions.

## Do Not Include

- Real multi-page hierarchy.
- Real sync.
- Real account menu.
- Plugin manager.
- React-owned workspace navigation.

---

# 8. Phase 5 - Backend Skeleton and Lock API

## Goal

Start the server phase only after local document editing works and the native shell is stable enough to consume backend configuration.

## Scope

- Replace the legacy server dependency plan with `oat++`.
- Add `oat++` server target.
- Add health endpoint.
- Add structured logging.
- Add OpenTelemetry-ready observability boundaries: request IDs, trace/span hooks and exporter configuration points.
- Add placeholder auth middleware and explicit public/protected route separation.
- Add lock API stubs:
  - acquire lock;
  - heartbeat;
  - release lock;
  - force release placeholder.
- Add presence WebSocket stub.
- Add DTO/controller layout for future workspace, page and lock APIs.
- Add integration tests for health and lock stubs if feasible.

## Exit Gate

- Backend starts independently.
- Health endpoint works.
- Observability configuration can be wired without touching editor code.
- Lock endpoints return stable JSON envelopes.
- Desktop app can be configured with backend URL without requiring it for local editing.
- Server module layout is compatible with expanding into authenticated page APIs in the next phase.
- Public routes remain accessible without JWT while protected routes have a clear boundary.

## Do Not Include

- Full RBAC/ABAC.
- Real Sync Gateway provisioning.
- Realtime collaboration.

---

# 9. Phase 6 - Auth Spike

## Goal

Validate the Authentik desktop login path without entangling it with sync.

## Scope

- Implement OIDC Authorization Code Flow with PKCE.
- Use system browser login.
- Support localhost callback or custom URI scheme.
- Store tokens in system keyring.
- Refresh token flow.
- Logout flow.
- Backend JWT validation middleware.
- Never expose tokens to the web editor.

## Exit Gate

- Login works on target development OS.
- Refresh works.
- Logout clears local token state.
- Protected backend endpoint rejects invalid tokens.
- Editor has no token access.

## Do Not Include

- Sync Gateway channel mapping.
- Full permissions UI.
- Confluence auth.

---

# 10. Phase 7 - MVP Collaboration Locks

## Goal

Close the Single Writer / Multi Reader path before sync.

## Scope

- Make backend lock owner authoritative.
- Desktop app acquires lock before editing.
- Send heartbeat every 10 seconds while editing.
- Open editor in read-only mode when another user owns the lock.
- Show lock owner and lock status.
- Add force release for authorized users as a backend-audited action.
- Add presence display placeholder.

## Exit Gate

- Only one session can edit a page.
- Lock timeout makes stale locks recoverable.
- Read-only mode is enforced through the editor bridge.
- Lock takeover is logged.

## Do Not Include

- Yjs.
- Simultaneous editing.
- Offline collaborative merge.

---

# 11. Phase 8 - Sync Spike

## Goal

Validate offline replication and conflict handling with the real sync stack.

## Scope

- Stand up Couchbase Server and Sync Gateway dev environment.
- Define channel mapping for workspace/page access.
- Connect desktop local store to authenticated replication.
- Preserve local edits while offline.
- Detect conflicts.
- Preserve losing versions as snapshots.
- Show sync status and conflict status in UI.

## Exit Gate

- Local edit replicates after reconnect.
- Authenticated channels restrict document access.
- Conflict creates an explicit user-visible state.
- Losing version is preserved.
- Auth/sync failures are logged without leaking secrets.

## Do Not Include

- Full conflict merge UI beyond local/remote choice.
- Confluence sync.
- Realtime collaboration.

---

# 12. Phase 9 - Rendering and Export Foundation

## Goal

Create the canonical rendering path before Confluence conversion.

## Scope

- Add C++ RenderService.
- Convert Document JSON to internal AST.
- Render supported starter blocks to sanitized HTML.
- Extract SearchText.
- Add snapshot tests.
- Keep editor rendering separate from canonical rendering.

## Exit Gate

- Starter block documents render through C++.
- Render output is deterministic.
- Unsupported blocks render explicit placeholders.
- HTML output is sanitized.

## Do Not Include

- Plugin rendering.
- Full Confluence conversion.
- PDF export.

---

# 13. Phase 10 - Confluence Import/Export Spike

## Goal

Validate no-silent-loss import/export for a small supported set.

## Scope

- Add Confluence client skeleton.
- Support Cloud/Data Center configuration shape.
- Add conversion fixtures for supported starter blocks.
- Preserve unsupported macro source where possible.
- Render unsupported macro placeholder blocks.
- Add attachment metadata mapping placeholder.

## Exit Gate

- Supported fixture set round-trips without silent loss.
- Unsupported macros become explicit placeholders.
- Source reference is preserved.
- Conversion tests run in CI.

## Do Not Include

- Full Confluence parity.
- Space-scale migration tooling.
- Background sync scheduler.

---

# 14. Phase 11 - Plugin Runtime Spike

## Goal

Validate the WASM plugin safety model with one non-critical extension point.

## Scope

- Decide Wasmtime provisioning path.
- Add plugin manifest schema.
- Define minimal C++ Plugin SDK boundary.
- Run one demo WASM renderer.
- Enforce timeout.
- Enforce memory limit if supported.
- Deny filesystem, network and credential access by default.
- Render fallback on plugin failure.

## Exit Gate

- Demo plugin renders a block.
- Crashing or timing-out plugin does not crash the host.
- Failed plugin is logged and marked unhealthy.
- Fallback rendering works.

## Do Not Include

- Plugin marketplace.
- Broad host capability API.
- User-installed plugin lifecycle.

---

# 15. Phase 12 - MVP Hardening and Release Gates

## Goal

Turn the validated vertical paths into a pilot-ready MVP.

## Scope

- Run acceptance tests for:
  - offline edit;
  - restart load;
  - sync recovery;
  - auth renewal;
  - lock expiration;
  - plugin failure;
  - Confluence unsupported macro handling.
- Add runbook for sync/auth/backend failures.
- Add rollback notes.
- Add security note for editor bridge, tokens and plugin sandbox.
- Add observability checklist.
- Review all open decisions and either close or defer explicitly.

## Exit Gate

- Pilot gate scenarios pass.
- Release gate docs exist.
- Known limitations are documented.
- No critical path relies on LLM behavior.

---

# 16. Parallel Workstreams

Some work can run in parallel after the first vertical slice is stable.

| Workstream | Can Start After | Notes |
| :--- | :--- | :--- |
| UI shell polish | Phase 1 | Qt-owned shell only; must not fake unavailable sync/auth states as working |
| Backend skeleton | Phase 5 | Keep desktop local editing independent |
| Auth spike | Phase 6 | Keep tokens out of editor runtime |
| RenderService | Phase 2 | Can start once DTO shape is stable |
| Confluence fixtures | Phase 9 | Needs canonical AST/rendering conventions |
| Plugin runtime | Phase 9 | Safer after rendering boundary exists |
| Sync spike | Phase 8 | Needs auth path and local persistence |

---

# 17. Recommended Immediate Backlog

1. Define minimal document DTO with `schema_version`, page ID and block IDs.
2. Add fixture tests for valid and invalid starter documents.
3. Route `wiki.documents.updateSnapshot` through document validation.
4. Add local repository interface before implementing real persistence.
5. Keep Couchbase Lite behind the storage boundary and build option.
6. Move product navigation/status out of the frontend demo when the Qt shell phase starts.

---

# 18. Current Open Decisions

| Decision | Recommended Handling |
| :--- | :--- |
| Wasmtime dependency provisioning | Defer until Phase 11; do not block editor/document/persistence work |
| Couchbase Lite C++ packaging | C++ wrapper smoke test exists behind `CPPWIKI_ENABLE_CBLITE_STORAGE`; keep packaging optional until Phase 3 persistence work |
| Editor bundle packaging | Keep filesystem `dist` during spikes; decide Qt resources vs installed app data before release hardening |
| QWebChannel schema | Phase 1 baseline closed; generated DTOs continue in Phase 2 |
| Frontend package manager | Keep npm unless workspace tooling becomes a real pain point |
| JS widget boundary | Default to Qt; use JS only for BlockNote-native widgets and editor internals |

---

# 19. Non-Goals Until After MVP

- Full realtime collaborative editing.
- Offline CRDT merge.
- Full Confluence macro parity.
- Mobile clients.
- Server-side AI automation.
- Plugin marketplace.
- Full Google Docs-like UX.
