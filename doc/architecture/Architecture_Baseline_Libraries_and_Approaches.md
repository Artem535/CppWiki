# Architecture Baseline: Libraries and Approaches

**Product:** Wiki Platform v9 — Block Document Edition  
**Status:** Draft / Baseline for implementation spikes  
**Date:** 2026-06-11  
**Source:** `doc/PRD_v9_Block_Document_Edition.md`  
**Related ADRs:** ADR-001, ADR-002, ADR-003, ADR-004, ADR-005, ADR-006, ADR-008

---

# 1. Purpose

This document fixes the baseline technology choices and engineering approaches for Wiki Platform v9.

The goal is to remove ambiguity before implementation starts:

- what libraries are selected;
- which choices are already accepted by ADR;
- which choices are implementation defaults;
- which areas require a spike before final lock-in;
- what architectural boundaries must not be crossed.

This document does not replace ADRs. If a choice becomes controversial or changes system architecture, create or update an ADR.

---

# 2. Baseline Principles

1. The product is desktop-first and offline-first.
2. The canonical document source of truth is structured block JSON, not Markdown, Djot, HTML or Confluence format.
3. The editor UX is web-based inside a native Qt shell.
4. The C++ core owns persistence, sync, rendering, plugin execution, auth integration and business rules.
5. JavaScript editor code must not become the authority for storage, permissions, sync or export.
6. Plugins are untrusted and must run inside a WASM sandbox.
7. MVP uses Single Writer / Multi Reader locking; realtime collaboration is deferred but must remain architecturally possible.
8. AI/LLM capabilities are out of MVP scope and must not be required for core wiki workflows.

---

# 3. Selected Libraries and Technologies

| Area | Selected Technology | Status | Reason |
| :--- | :--- | :--- | :--- |
| Desktop shell | Qt 6 Widgets | Accepted | Mature native desktop UI, cross-platform support, existing C++ ecosystem |
| Embedded editor host | QWebEngine | Accepted | Required to host modern JS block editor inside desktop app |
| JS/native bridge | QWebChannel | Baseline | Native Qt mechanism for typed communication between QWebEngine and C++ |
| Editor framework | BlockNote | Accepted | Notion-like block UX, structured block model, custom blocks |
| Editor engine | Tiptap | Accepted | BlockNote foundation and ProseMirror ecosystem access |
| Document editing core | ProseMirror | Accepted | Structured document model and future collaboration path |
| Future realtime collaboration | Yjs | Accepted for future phase | CRDT ecosystem compatible with Tiptap/ProseMirror |
| Local database | Couchbase Lite | Accepted | Offline-first local storage and replication model |
| Sync | Couchbase Sync Gateway | Accepted | Replication, channels and OIDC-compatible access control |
| Backend | Drogon | Accepted | C++ async HTTP/WebSocket backend |
| Plugin runtime | Wasmtime | Accepted | WASM sandbox with host-controlled capabilities |
| Plugin SDK language | C++20 | Accepted | Native developer experience for extension authors |
| Identity provider | Authentik | Accepted | Self-hosted OIDC/OAuth2/SAML/LDAP-capable identity platform |
| Auth protocol | OIDC Authorization Code Flow with PKCE | Accepted | Correct desktop login flow without embedding credentials |
| Confluence integration | Confluence REST API v2/v1 | Accepted | Required for Cloud and Data Center support |
| Serialization / JSON library | reflect-cpp | Selected default | Typed C++ reflection-based serialization for document payloads, API DTOs and config contracts |
| Logging | spdlog | Selected default | Structured, fast, widely used C++ logging |
| Build system | CMake | Accepted | Cross-platform C++ build and target organization |
| Compiler baseline | clang++ | Accepted default | Tooling, diagnostics, clang-tidy and compile_commands.json |

---

# 4. Architecture Approaches

## 4.1. Desktop and Editor Boundary

Use Qt 6 Widgets for the desktop shell and QWebEngine for the editor host.

The Qt shell owns:

- navigation tree;
- workspace switching;
- sync status;
- settings;
- account and auth flow;
- plugin manager;
- native dialogs;
- local service orchestration.

The web editor owns:

- interactive block editing;
- slash commands;
- block handles;
- drag and drop;
- inline editor UX;
- editor-only previews.

The web editor must communicate with C++ only through QWebChannel APIs under stable namespaces:

```text
wiki.auth.*
wiki.documents.*
wiki.render.*
wiki.plugins.*
wiki.sync.*
wiki.confluence.*
wiki.presence.*
wiki.locks.*
```

The editor must not receive raw filesystem, token, database or unrestricted network access.

## 4.2. Document Model

Use Block Document JSON as the canonical source of truth.

Markdown, Djot, HTML, ADF and Confluence Storage Format are import/export formats only.

Required document rules:

- every page has a stable page ID;
- every block has a stable block UUID;
- every document has `schema_version`;
- migrations are explicit and versioned;
- invalid document JSON must be rejected before persistence;
- lossy import/export behavior must be explicit and test-covered.

## 4.3. Rendering

Use a two-layer rendering approach.

Editor Render:

- runs inside BlockNote;
- optimizes for interactive editing;
- may differ from export rendering;
- is not authoritative for Confluence/PDF/search output.

Canonical Render:

- runs in C++ RenderService;
- converts Document JSON to an internal AST;
- invokes WASM plugin renderers when needed;
- produces HTML, ADF, Confluence Storage Format, Markdown/Djot and SearchText;
- sanitizes plugin and imported HTML before display or export.

## 4.4. Persistence and Sync

Use Couchbase Lite for local persistence and Couchbase Sync Gateway for replication.

MVP persistence must support:

- users cache;
- workspaces;
- pages;
- documents;
- attachments metadata and blobs;
- sync metadata;
- lock cache;
- plugin metadata;
- Confluence mapping.

Sync rules:

- local edits must not be lost when offline;
- replication must respect Sync Gateway channels;
- auth must use OIDC-compatible identity from Authentik;
- conflicts in MVP are user-visible and resolved by local/remote choice;
- losing versions must be preserved as snapshots.

## 4.5. Backend

Use Drogon for the backend API and WebSocket services.

Backend owns:

- authoritative document locks;
- presence and lock notifications;
- REST APIs for workspaces, pages, permissions, plugins, Confluence sync and audit logs;
- JWT validation middleware;
- Sync Gateway provisioning and diagnostics;
- future collaboration gateway integration.

The backend must not become the canonical editor renderer for MVP unless the C++ RenderService is explicitly reused server-side.

## 4.6. Authentication and Authorization

Use Authentik as the recommended identity provider.

Desktop login flow:

```text
Desktop App
    -> system browser
    -> Authentik OIDC login
    -> localhost callback or custom URI scheme
    -> authorization code exchange
    -> token storage in system keyring
```

Authorization approach:

- derive platform user ID from stable OIDC `sub`;
- use RBAC for global roles;
- use ABAC for workspace/page-sensitive rules;
- map Authentik groups/roles to platform permissions;
- map trusted claims to Sync Gateway roles/channels;
- audit admin actions, lock takeover, permission denies and Confluence sync actions.

Tokens must never be stored in plain text config files.

## 4.7. Plugin System

Use Wasmtime to execute plugins as WebAssembly modules.

Plugin authors write C++20 against the Wiki Plugin SDK. The SDK hides ABI, memory transfer, serialization and error handling.

Plugin contract:

- manifest;
- block schema;
- validation;
- rendering;
- SearchText extraction;
- declared permissions.

Default plugin policy:

- no filesystem access;
- no network access;
- no process memory access;
- no credential/keyring access;
- no access to other plugins;
- explicit capability approval for any host-provided operation.

Plugin failures must not crash the host. The host terminates failed execution, logs the failure, marks the plugin unhealthy and renders a fallback block.

## 4.8. Collaboration

Use Single Writer / Multi Reader in MVP.

MVP rules:

- only one user can edit a page at a time;
- lock owner is authoritative on the backend;
- editor sends heartbeat every 10 seconds;
- stale locks can be force-released by authorized users;
- force release is audited;
- other users see read-only mode and presence.

Future realtime collaboration must remain possible through:

```text
BlockNote -> Tiptap -> ProseMirror -> Yjs -> WebSocket Provider
```

Therefore, MVP must preserve stable page and block IDs and avoid storage choices that prevent CRDT adoption later.

## 4.9. Confluence Integration

Use the internal structured document model as the central conversion point.

Conversion baseline:

```text
Document JSON
    <-> Internal AST
    <-> ADF
    <-> Confluence Storage Format
    <-> HTML
    <-> Djot / Markdown
```

Rules:

- supported Confluence blocks must round-trip through fixtures;
- unsupported macros should become placeholder blocks;
- raw unsupported source should be preserved where possible;
- no silent lossy conversion;
- attachments must retain stable IDs and source mapping;
- Cloud and Data Center auth modes must be isolated behind one Confluence client interface.

---

# 5. Implementation Defaults

## 5.1. C++ Project Structure

Use modular CMake targets:

```text
core
gui
editor_bridge
storage
auth
plugins
render
confluence
server
```

Each target should expose a narrow public API and avoid circular dependencies.

## 5.2. Serialization, JSON and Schemas

Use `reflect-cpp` as the default serialization and JSON library for MVP.

Rules:

- define JSON Schemas for document, block, plugin manifest and API contracts;
- model transport payloads as typed C++ structs instead of ad hoc dynamic JSON objects;
- validate external JSON before conversion to internal domain types;
- keep internal domain models separate from transport DTOs when invariants differ;
- centralize migrations by `schema_version`.
- keep raw JSON only at system boundaries where preserving unknown fields is required, such as unsupported Confluence macro payloads.

## 5.3. Logging

Use `spdlog` as the default logging library.

Rules:

- logs must include component, operation, request/session ID where applicable;
- logs must not contain tokens, secrets or raw sensitive document content;
- sync/auth/plugin/confluence failures must be observable;
- plugin crashes and timeouts must include plugin ID and version.

## 5.4. Frontend Build

The editor bundle is a local web asset loaded by QWebEngine.

Rules:

- no runtime dependency on remote CDN assets;
- editor build is versioned with the desktop app;
- QWebChannel API compatibility is versioned;
- Content Security Policy is defined for editor assets.

## 5.5. Testing Approach

Minimum test surfaces:

- document schema validation and migrations;
- import/export fixtures for supported block types;
- QWebChannel contract tests;
- RenderService output snapshots;
- plugin sandbox failure tests;
- auth token lifecycle tests;
- Sync Gateway channel mapping tests;
- Confluence conversion fixtures;
- MVP lock/presence behavior.

---

# 6. Spike Requirements Before Full Implementation

These choices are accepted as baseline, but require implementation spikes before large-scale buildout:

| Spike | Purpose | Exit Criteria |
| :--- | :--- | :--- |
| BlockNote inside QWebEngine | Validate editor packaging, sizing, focus, keyboard shortcuts and bridge calls | Local editor opens, edits and saves Document JSON through QWebChannel |
| QWebChannel contract | Validate typed request/response/error pattern | Stable API convention and error envelope documented |
| Couchbase Lite + Sync Gateway + OIDC | Validate offline storage, replication and channel auth | Local edit replicates through authenticated Sync Gateway channel |
| Wasmtime C++ host | Validate plugin ABI, timeout, memory limit and error handling | Demo plugin renders and failed plugin does not crash host |
| Authentik desktop login | Validate PKCE, callback and token keyring storage | Login, refresh and logout work on target OS |
| Confluence conversion | Validate ADF/Storage import/export for MVP blocks | Fixture set round-trips without silent loss for supported blocks |

---

# 7. Deferred or Rejected Approaches

| Approach | Decision | Reason |
| :--- | :--- | :--- |
| QTextEdit as primary editor | Rejected | Does not support required block UX and creates high collaboration risk |
| Djot as source of truth | Rejected | Plain-text-first storage does not match block document requirements |
| Native dynamic plugins through `dlopen` / `QLibrary` | Rejected | Unsafe failure isolation and ABI risks |
| Full realtime collaboration in MVP | Deferred | Too much delivery risk; MVP uses locking |
| AI/LLM core workflows in MVP | Deferred | PRD explicitly treats AI automation as non-goal |
| Remote web editor assets | Rejected for MVP | Offline-first desktop app must load bundled local editor assets |

---

# 8. Change Control

Any change to the selected technologies in section 3 requires:

1. an ADR update or new ADR;
2. impact review against PRD goals and MVP slice;
3. migration notes if implementation has already started;
4. test plan update for affected contracts.

Implementation defaults in section 5 can be changed by technical lead approval if no external contract changes. If the change affects storage format, plugin ABI, QWebChannel API, sync behavior or auth model, it requires an ADR.
