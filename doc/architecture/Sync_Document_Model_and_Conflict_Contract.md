# Sync Document Model and Conflict Contract

**Product:** CppWiki / Wiki Platform v9 - Block Document Edition  
**Status:** Implementation contract  
**Date:** 2026-06-30  
**Related docs:** `doc/architecture/Desktop_Backend_Sync_Interaction.md`, `doc/architecture/adr/ADR-003-collaboration-strategy.md`, `doc/architecture/adr/ADR-008-authentik-auth-model.md`

---

# 1. Purpose

This document fixes the application-level sync contract for desktop, backend and Couchbase Sync.

It defines:

- which document fields belong to the product model;
- how document versioning works;
- how workspace access maps to Sync Gateway channels;
- how conflict metadata must be represented;
- which sync and collaboration states are allowed to affect editing behavior.

The goal is to make the next sync implementation phase decision-complete and prevent product logic from depending directly on Couchbase internal revision semantics.

---

# 2. Synced Document Contract

Every replicated document record must expose the following application fields:

| Field | Type | Meaning |
| :--- | :--- | :--- |
| `id` | string | Stable document id |
| `workspace_id` | string | Owning workspace id |
| `title` | string | Current derived document title |
| `raw_snapshot` | string | Validated BlockNote snapshot JSON |
| `created_at` | string | UTC ISO timestamp of initial creation |
| `updated_at` | string | UTC ISO timestamp of last local mutation |
| `created_by` | string | Principal that created the document |
| `updated_by` | string | Principal that produced the last persisted mutation |
| `content_version` | int64 | Monotonic product-level document version |

Rules:

- `workspace_id` is mandatory for all synced documents.
- `raw_snapshot` is the source payload replicated through sync.
- `created_by`, `updated_by` and `content_version` are product fields, not UI-only hints.
- Couchbase metadata may exist in storage, but product code must treat it as infrastructure metadata rather than business API.

---

# 3. Versioning Rules

CppWiki uses `content_version` as the application-level document version.

Rules:

- a newly created document starts with `content_version = 1`;
- each successful persisted mutation increments `content_version` by exactly one;
- `updated_at` is audit metadata and must not be used as the only ordering or conflict signal;
- `created_by` never changes after creation;
- `updated_by` must be updated on every persisted mutation;
- desktop save, rename and tree placement updates are all versioned mutations.

This keeps product logic stable even if Couchbase revision shape changes later.

---

# 4. Workspace and Channel Contract

Each document belongs to exactly one workspace.

Workspace replication is mapped to Sync Gateway channels with the following naming rule:

- `workspace:<workspace_id>`

Rules:

- backend is the only authority that decides which workspace channels the current principal may use;
- desktop must consume workspace channels only from backend bootstrap;
- desktop must not derive new channels or access rights locally;
- sync access to a workspace is granted only if bootstrap includes the corresponding `workspace:<id>` channel.

The backend bootstrap contract must include at minimum:

- `available`
- `enabled`
- `gateway_url`
- `database_name`
- `auth_mode`
- `token_passthrough`
- `principal_subject`
- `principal_username`
- `principal_email`
- `principal_roles`
- `principal_groups`
- `channels`
- `status_text`

Derived rule:

- desktop may derive the visible workspace list from the assigned `workspace:<id>` channels for UX and routing purposes, but not for authorization decisions beyond that bootstrap.

---

# 5. Conflict Contract

Sync conflicts must be preserved explicitly.

CppWiki must not silently discard either side of a conflict.

The conflict metadata model for the next phase is:

| Field | Type | Meaning |
| :--- | :--- | :--- |
| `document_id` | string | Conflicted document id |
| `workspace_id` | string | Owning workspace |
| `base_version` | int64 | Last known shared product version if available |
| `local_snapshot` | string | Local document snapshot kept by desktop |
| `remote_snapshot` | string | Remote document snapshot received through sync |
| `local_updated_by` | string | Local author for the preserved local snapshot |
| `remote_updated_by` | string | Remote author for the preserved remote snapshot |
| `detected_at` | string | UTC ISO timestamp when conflict was detected |
| `resolution_state` | string enum | `pending`, `resolved`, `dismissed` |

Rules:

- the primary working copy must not be overwritten silently when a conflict is detected;
- a conflict creates explicit conflict metadata;
- the losing version is preserved until the user or a later resolution step handles it;
- the first implementation phase does not introduce automatic merge.

Default resolution strategy for v1:

- preserve local;
- preserve remote;
- mark document as conflicted;
- require later manual resolution.

---

# 6. Sync and Collaboration Invariants

Sync state and collaboration state are related but separate.

The following product rules are mandatory:

| Situation | Expected behavior |
| :--- | :--- |
| Sync is offline, repository is healthy | local editing remains available |
| Sync reports error, lock is still valid | edit mode remains active |
| Lock is lost, sync is healthy | edit mode exits and editor becomes read-only |
| Auth expires | collaboration degrades immediately; sync may degrade independently |
| Sync succeeds | this does not grant lock ownership |
| Lock is granted | this does not imply replication is healthy |
| Conflict is detected | local data is preserved and the document is marked conflicted |

This separation must remain true in desktop UI, sync service code and backend coordination code.

---

# 7. Implementation Mapping

The current implementation contract maps as follows:

- `document::PageMetadata` owns `workspace_id`, `created_by`, `updated_by`, `content_version`;
- repository save/load/list paths must preserve those fields;
- desktop bridge responses must expose those fields to the shell/editor host;
- sync snapshot reporting must expose workspace scope and conflict presence separately from backend reachability.

The next sync implementation phase may extend storage with explicit conflict records, but it must not remove or weaken this document contract.

---

# 8. Implementation Plan

This section fixes the delivery order for the next sync phase.

The goal is to make desktop behavior deterministic for:

- first install;
- backend connect;
- initial document download;
- offline work from the local database;
- reconnect and later replication;
- explicit conflict handling.

## 8.1. Target product flow

The intended product flow is:

1. user installs the desktop app;
2. desktop starts in local-only mode;
3. user signs in and connects to the backend;
4. backend returns sync bootstrap for the current principal;
5. desktop starts replication using that bootstrap;
6. documents for assigned workspaces are materialized into the local database;
7. once materialized, those documents remain available offline;
8. when network returns, local and remote changes are synchronized;
9. if versions diverge, a conflict record is created and resolved explicitly.

This flow means:

- local database is the runtime working copy;
- backend bootstrap is the authority for workspace visibility and sync scope;
- offline access is valid only after initial materialization for the relevant workspace;
- desktop must not invent remote workspace contents locally.

## 8.2. Delivery phases

The work should be delivered in the following order.

### Phase 1. Separate local-only and synced behavior

Rules:

- local-only mode may create welcome content and local starter documents;
- synced mode must not create synthetic remote workspace contents automatically;
- workspace visibility in synced mode must come from backend bootstrap channels only;
- the desktop must stop using weak heuristics such as "sync is enabled in settings" as a substitute for real remote readiness.

Implementation tasks:

- introduce a stronger sync readiness predicate than a plain settings flag;
- keep local-only document bootstrapping only for repositories that are not under active remote sync scope;
- remove any desktop behavior that appends remote workspaces optimistically before backend/bootstrap confirms them.

### Phase 2. Introduce initial workspace hydration state

Each workspace visible to the desktop must have an explicit hydration state.

Required states:

- `not_started`
- `in_progress`
- `materialized`
- `failed`

Rules:

- a workspace is not offline-capable until it reaches `materialized`;
- an empty synced workspace and a not-yet-downloaded workspace are different states and must not be rendered the same way;
- the desktop must be able to tell whether the local database is empty because there are no documents, or because initial pull has not completed yet.

Implementation tasks:

- extend sync snapshot with `WorkspaceHydrationState` (`not_started`, `in_progress`, `materialized`, `failed`) for every workspace;
- persist and restore per-workspace hydration in `sync-context.json` (version 2);
- expose hydration state to desktop UI through the sync snapshot;
- add explicit desktop copy such as "Syncing documents...", "Documents not downloaded yet" and "Initial pull failed";
- ensure workspace tree refresh reacts to hydration completion and failure transitions;
- keep `ShouldExpectRemoteDocuments()` driven by hydration state so unmaterialized/failed workspaces do not look offline-ready.

Status: **Implemented** in `DocumentSyncService`, `DocumentSyncSnapshot` and `Page` workspace tree UX.

### Phase 3. Define initial pull completion gate
- [x] Update architecture docs
- [ ] Build/test verification


### Phase 3. Define initial pull completion gate

The desktop needs a real gate for "documents are now available offline".

Rules:

- successful bootstrap is not enough;
- started replicator is not enough;
- `materialized` must mean the workspace has passed at least one successful initial pull cycle;
- after that point, offline reads must come from the local database without requiring backend reachability.

Implementation tasks:

- define what CppWiki treats as initial pull completion for a workspace;
- persist or derive enough state so a restart can still distinguish hydrated and non-hydrated workspaces;
- keep the local database authoritative for reads after hydration, even when backend later becomes unavailable.

### Phase 4. Tighten reconnect and replication behavior

After hydration, reconnect behavior must remain predictable.

Rules:

- local edits performed offline remain in the local database first;
- reconnect triggers replication, not a destructive overwrite;
- sync health changes must not silently change lock ownership or editability;
- replication recovery must not fabricate missing welcome documents in synced workspaces.

Implementation tasks:

- verify that offline edits survive reconnect and are sent back through replication;
- keep collaboration state separate from replication state;
- keep workspace tree and document list refresh driven by actual repository state changes.

### Phase 5. Finish conflict surfacing and resolution

The current contract already defines conflict metadata.

The next step is to make conflict handling a complete product flow.

Rules:

- conflict detection creates `DocumentConflictRecord` at the real sync conflict point;
- desktop must surface pending conflicts distinctly from generic sync errors;
- the primary working copy must remain usable after conflict detection;
- resolve and dismiss actions must operate on explicit stored conflict records.

Implementation tasks:

- keep the current conflict badge/count UI;
- add a dedicated conflict list and later a compare/merge surface;
- support explicit `resolve` and `dismiss` over stored conflict metadata;
- keep automatic merge out of scope for v1.

## 8.3. Concrete coding order

The recommended implementation order in code is:

1. strengthen sync readiness semantics in `SyncService` / `DocumentSyncService`;
2. remove synthetic remote workspace bootstrapping from desktop bridge/page flow;
3. introduce workspace hydration tracking in the sync snapshot/model;
4. wire hydration state into `Page` and workspace tree UX;
5. validate cold start after local DB deletion;
6. validate offline edit then reconnect;
7. finish conflict UX on top of the already stored conflict records.

## 8.4. Acceptance criteria

The sync phase is considered correct only if all of the following are true:

- first install with no backend still works as local-only desktop mode;
- after backend connect, assigned workspace documents are downloaded into the local database;
- after the first successful download, those documents are available offline after app restart;
- deleting the local database and reconnecting causes documents to be re-materialized from sync;
- offline edits survive reconnect and are replicated later;
- conflicts do not silently overwrite local data and are represented explicitly in UI and storage.
