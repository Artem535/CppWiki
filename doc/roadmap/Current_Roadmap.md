# Current Roadmap

**Product:** CppWiki / Wiki Platform v9 — Block Document Edition  
**Status:** Draft  
**Date:** 2026-06-16
**Related docs:** `doc/PRD_v9_Block_Document_Edition.md`, `doc/architecture/Architecture_Baseline_Libraries_and_Approaches.md`, `doc/architecture/Project_Structure_and_Style.md`

---

# 1. Current Baseline

The project currently has:

- Qt/C++ project skeleton with CMake presets.
- vcpkg manifest for reflect-cpp, spdlog and Drogon.
- Qt is resolved from the system or external Qt installation, not from vcpkg.
- `cppwiki::Application` and `cppwiki::MainWindow`.
- Qlementine wired as the desktop Qt style.
- `QWebEngineView` wired as the editor host.
- Vite/React editor bundle under `frontend/editor`.
- BlockNote installed and rendered in the startup editor page.
- QWebChannel bridge with versioned `{ apiVersion, ok, result/error }` envelope.
- Reflect-cpp BlockNote snapshot DTOs and validation before persistence.
- Local document repository boundary with file-backed storage and optional Couchbase Lite adapter.
- Minimal page list navigation in the editor bundle while the native shell is still being developed.
- Default `Welcome to CppWiki` page bootstrap when the local repository is empty.
- UUID helper for generated document IDs.
- Architecture and project-structure documentation.

The current UI is a minimal working shell, not the final native product shell. The page list is intentionally a list view for the current phase; tree view belongs to a later step.

---

# 2. Near-Term Milestones

## Milestone 1: Editor Host Spike

Goal: prove that Qt WebEngine can reliably load the local BlockNote bundle.

Scope:

- load `frontend/editor/dist/index.html` from `QWebEngineView`;
- keep fallback HTML when the bundle is not built;
- verify keyboard input, focus, scrolling and window resize;
- define how the frontend bundle is built in local and CI workflows.

Exit criteria:

- `npm run build` succeeds in `frontend/editor`;
- Qt application loads the built BlockNote page from disk;
- startup page remains usable when opened from a file URL.

## Milestone 2: QWebChannel Contract

Goal: define a typed bridge between the editor and C++ core.

Scope:

- introduce an `editor_bridge` C++ module;
- expose `wiki.documents.*` through QWebChannel;
- define request/response/error envelope;
- send editor document changes to C++;
- load initial document JSON from C++ into BlockNote.

Exit criteria:

- frontend can call a C++ method through QWebChannel;
- C++ can provide initial content;
- editor change events are observable in C++ logs.

## Milestone 3: Document Model Skeleton

Goal: establish the C++ representation of the block document.

Scope:

- define typed DTOs with reflect-cpp;
- define document schema version;
- implement basic validation;
- map BlockNote content to internal document payload;
- preserve validated raw BlockNote snapshot JSON for storage;
- add fixture tests for the supported BlockNote block set.

Exit criteria:

- sample BlockNote document can be serialized/deserialized;
- invalid payloads are rejected before persistence;
- fixture tests cover the BlockNote block set used by the editor.

## Milestone 4: Local Persistence Vertical Slice

Goal: prove offline save/load loop with a minimal multi-page navigation path.

Scope:

- choose Couchbase Lite integration path for C++;
- persist page metadata and validated document JSON;
- expose repository operations for listing and loading documents;
- create a default `Welcome to CppWiki` page when the repository is empty;
- support selection from a page list view;
- autosave the selected document;
- keep save failures visible in logs and bridge errors.

Exit criteria:

- page list loads from the repository;
- selecting a page loads its stored BlockNote snapshot;
- edit -> autosave -> restart -> load works locally;
- failed save does not destroy previous valid document;
- persistence errors are surfaced through the bridge and logs.

## Milestone 5: Product Shell

Goal: move the temporary web-owned workspace chrome toward a minimal Qt-owned wiki shell.

Scope:

- page list/tree widget in Qt;
- document title area in Qt;
- sync/status area in Qt;
- editor host;
- settings/action placeholders;
- stable layout for desktop screen sizes.

Exit criteria:

- user can distinguish workspace navigation from document editing;
- editor is the primary screen, not a landing page;
- UI layout remains stable during resize.

---

# 3. Medium-Term Milestones

## Milestone 6: Auth Spike

- Authentik OIDC Authorization Code Flow with PKCE.
- System browser login.
- token storage in system keyring.
- backend JWT validation path.

## Milestone 7: Backend Skeleton

- Drogon application target.
- health endpoint.
- auth middleware placeholder.
- lock and presence API stubs.
- structured logging with spdlog.

## Milestone 8: Sync Spike

- Couchbase Sync Gateway environment.
- channel mapping proof.
- authenticated replication.
- basic conflict snapshot behavior.

## Milestone 9: Plugin Runtime Spike

- decide Wasmtime provisioning path.
- define C++ Plugin SDK boundary.
- run one demo WASM renderer.
- enforce timeout and fallback rendering.

## Milestone 10: Confluence Import/Export Spike

- Confluence API client skeleton.
- supported block fixture set.
- unsupported macro placeholder.
- no silent lossy conversion.

---

# 4. Technical Gates

| Gate | Required Evidence |
| :--- | :--- |
| WebEngine gate | local BlockNote bundle loads inside Qt and accepts editing |
| Bridge gate | QWebChannel supports typed request/response/error exchange |
| Document gate | reflect-cpp DTOs serialize and validate BlockNote documents |
| Persistence gate | local save/load works offline without data loss |
| Security gate | editor has no raw filesystem, token or unrestricted native access |
| Plugin gate | failed plugin cannot crash the host |
| Sync gate | document access is constrained by authenticated channels |

---

# 5. Open Decisions

| Decision | Status | Notes |
| :--- | :--- | :--- |
| Wasmtime dependency provisioning | Open | Not in current `vcpkg.json`; needs supported port, overlay port or separate vendoring strategy |
| Couchbase Lite C++ packaging | Spike proof added | Optional C++ wrapper smoke test and repository save/load/list test run when `CPPWIKI_ENABLE_CBLITE_STORAGE=ON` and `CPPWIKI_CBLITE_ROOT` points to an extracted package |
| Editor bundle packaging | Partially decided | Current path loads from `frontend/editor/dist`; `editor_bundle` remains explicit by default, with `CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP=ON` available for linked dev builds. Production packaging may move to Qt resources or installed app data |
| QWebChannel schema | Phase 3 baseline | Versioned `{ apiVersion, ok, result/error }` envelope exists for bridge info, document list, document load and snapshot update. Generated TypeScript/C++ DTOs can still be revisited later |
| Frontend package manager | Tentative | npm is used now; can revisit if workspace tooling changes |
| Document hash / dirty check | Deferred | Not required for current autosave loop. Revisit for Phase 3.5 when adding conflict detection, skip-save optimization or sync |
| Page navigation shape | List first | Current UI uses list view. Tree view is deferred until hierarchy metadata exists |

---

# 6. Immediate Next Actions

1. Exercise the Phase 3 path manually: empty repository -> default page -> select page -> edit -> autosave -> restart -> load.
2. Add hierarchy-ready page metadata before implementing tree view: `parent_id`, `sort_order`, timestamps and workspace binding if needed.
3. Move page navigation from temporary React chrome into a Qt-owned list widget.
4. Replace the Qt list widget with tree view after the navigation model and selected-document state are stable.
5. Finish tree row actions: add-child button, context menu, delete, move up/down and drag-and-drop reordering/reparenting.
6. Add Qt-owned light/dark theme switching through Qlementine and propagate the active theme into the WebView editor.
7. Add a small user-visible save/error state in the desktop shell.
8. Decide whether document hash belongs in Phase 3.5 after the autosave loop is stable.
