# Current Roadmap

**Product:** CppWiki / Wiki Platform v9 — Block Document Edition  
**Status:** Draft  
**Date:** 2026-06-11  
**Related docs:** `doc/PRD_v9_Block_Document_Edition.md`, `doc/architecture/Architecture_Baseline_Libraries_and_Approaches.md`, `doc/architecture/Project_Structure_and_Style.md`

---

# 1. Current Baseline

The project currently has:

- Qt/C++ project skeleton with CMake presets.
- vcpkg manifest for reflect-cpp, spdlog and Drogon.
- Qt is resolved from the system or external Qt installation, not from vcpkg.
- `cppwiki::Application` and `cppwiki::MainWindow`.
- `QWebEngineView` wired as the editor host.
- Vite/React editor bundle under `frontend/editor`.
- BlockNote installed and rendered in the startup editor page.
- Architecture and project-structure documentation.

The current UI is a proof-of-connection page, not the final product shell.

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
- add fixture tests for supported starter blocks.

Exit criteria:

- sample BlockNote document can be serialized/deserialized;
- invalid payloads are rejected before persistence;
- fixture tests cover paragraph, heading and list blocks.

## Milestone 4: Local Persistence Spike

Goal: prove offline save/load loop.

Scope:

- choose Couchbase Lite integration path for C++;
- persist page metadata and document JSON;
- load last document at app startup;
- keep save failures visible in UI.

Exit criteria:

- edit -> save -> restart -> load works locally;
- failed save does not destroy previous valid document;
- persistence errors are surfaced in the desktop UI.

## Milestone 5: Product Shell

Goal: replace the demo shell with a minimal wiki workspace UI.

Scope:

- page tree placeholder;
- document title;
- sync/status area;
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
| Document gate | reflect-cpp DTOs serialize and validate starter block documents |
| Persistence gate | local save/load works offline without data loss |
| Security gate | editor has no raw filesystem, token or unrestricted native access |
| Plugin gate | failed plugin cannot crash the host |
| Sync gate | document access is constrained by authenticated channels |

---

# 5. Open Decisions

| Decision | Status | Notes |
| :--- | :--- | :--- |
| Wasmtime dependency provisioning | Open | Not in current `vcpkg.json`; needs supported port, overlay port or separate vendoring strategy |
| Couchbase Lite C++ packaging | Spike proof added | Optional C++ wrapper smoke test runs when `CPPWIKI_ENABLE_CBLITE_STORAGE=ON` and `CPPWIKI_CBLITE_ROOT` points to an extracted package |
| Editor bundle packaging | Partially decided | Current path loads from `frontend/editor/dist`; `editor_bundle` remains explicit by default, with `CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP=ON` available for linked dev builds. Production packaging may move to Qt resources or installed app data |
| QWebChannel schema | Phase 1 baseline done | Versioned `{ apiVersion, ok, result/error }` envelope exists for the current editor bridge. Generated TypeScript/C++ DTOs still belong to the document-model phase |
| Frontend package manager | Tentative | npm is used now; can revisit if workspace tooling changes |

---

# 6. Immediate Next Actions

1. Add a minimal document DTO using reflect-cpp.
2. Add schema version and starter block validation.
3. Add fixture tests for paragraph, heading and starter list blocks.
4. Route `wiki.documents.updateSnapshot` through validation before accepting snapshots.
5. Start the local persistence spike only after invalid snapshots are rejected with structured errors.
