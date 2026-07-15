# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Task workflow

Every task in this repo follows this sequence:

1. Check the current branch — if the task is new, create a new branch.
2. Work with an issue — open the existing one (read it and its comments) or create a new one.
3. Set the `in-progress` label on the issue.
4. Do the work.
5. After each significant action, leave a comment on the issue.
6. When done, open an MR.
7. The MR description must state that it closes the issue (e.g. `Closes #N`).
8. The MR description must be written in Russian.

## Conventions

- All documentation is written in AsciiDoc.
- All commands/programs are run through `rtk` (not invoked directly).

### Documentation site (Antora)

Documentation lives under `doc/` as AsciiDoc, organized as a single Antora component (`doc/antora.yml`,
pages under `doc/modules/ROOT/pages/**`). A root `antora-playbook.yml` builds the site from that source.

```bash
rtk npx antora generate antora-playbook.yml   # output: build/site/index.html
rtk npx http-server build/site                # optional local preview server
```

All docs under `doc/` and the former `ML LLM Delivery Pipeline/` templates, including the PRD, have been
migrated to AsciiDoc.

## What this is

CppWiki — a desktop-first, offline-first wiki platform. A Qt6/C++20 desktop app (`cppwiki_app`) embeds a
BlockNote/React editor via `QWebEngineView` and syncs documents through a userver-based C++ backend
(`cppwiki_server`) backed by Couchbase Sync Gateway, with Authentik for OIDC auth.

## Build

Vcpkg manifest mode; `VCPKG_ROOT` must be set. Qt6 (6.5+) is *not* installed via vcpkg — use system Qt or point
`CMAKE_PREFIX_PATH` at an external install.

```bash
export VCPKG_ROOT=/path/to/vcpkg

# Desktop app + tests
rtk cmake --preset debug [-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64]
rtk cmake --build --preset debug

# Server only (no Qt/desktop app, no tests)
rtk cmake --preset server-debug
rtk cmake --build --preset server-debug
```

Presets: `debug`, `release`, `server-debug`, `server-release` (see `CMakePresets.json`). Relevant `CPPWIKI_*`
options are in the root `CMakeLists.txt` (`CPPWIKI_BUILD_TESTS`, `CPPWIKI_BUILD_SERVER`,
`CPPWIKI_BUILD_DESKTOP_APP`, `CPPWIKI_ENABLE_CLANG_TIDY`, `CPPWIKI_ENABLE_CBLITE_STORAGE`,
`CPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP`).

clang-tidy is configured but off by default:

```bash
rtk cmake --preset debug -DCPPWIKI_ENABLE_CLANG_TIDY=ON
```

Formatting:

```bash
rtk clang-format -i src/**/*.h src/**/*.cc
```

### Frontend editor bundle

The BlockNote editor lives in `frontend/editor` (Vite + React + TypeScript, npm).

```bash
cd frontend/editor
rtk npm ci
rtk npm run build   # tsc --noEmit && vite build
```

The desktop app loads `frontend/editor/dist/index.html`; if the bundle is missing it falls back to a
static page with build instructions. `cppwiki_app` does not build the bundle by default — pass
`-DCPPWIKI_BUILD_EDITOR_BUNDLE_WITH_APP=ON` (or build the `editor_bundle` CMake target directly) to wire it in.

## Tests

Tests are only built with `CPPWIKI_BUILD_TESTS=ON` (default on the `debug`/`release` presets, off on
`server-*` presets). Each test binary is its own CMake target in `tests/CMakeLists.txt`, named
`cppwiki_<area>_tests`, e.g. `cppwiki_document_validator_tests`, `cppwiki_document_sync_service_tests`,
`cppwiki_server_config_tests`, `cppwiki_conflict_merge_model_tests`.

```bash
rtk cmake --build --preset debug
rtk ctest --test-dir build/debug                       # all tests
rtk ctest --test-dir build/debug -R cppwiki_document_validator_tests   # single test
```

Or run a built test binary directly, e.g. `./build/debug/tests/cppwiki_bridge_tests`.

## Local dev environment

`dev/docker-compose.yml` stands up Authentik (OIDC provider) + Postgres + Redis for local auth testing, and a
Couchbase Sync Gateway config lives under `dev/sync-gateway`. `config/server.yaml` is the server's runtime
config (bind address, auth issuer/JWKS, sync gateway URLs, role/group → channel mappings).

## Architecture

### Desktop app (`src/`, target `cppwiki_app`)

- `app/` — `Application` (process-level Qt setup, top-level object wiring), `ProgramSettings`,
  application stylesheet, and the editor-bundle-missing fallback page.
- `gui/` — `MainWindow` (desktop shell), `IPage`/`Page` (owns the `QWebEngineView` editor host),
  document tree view/model/delegate, context menus, settings dialog, presence strip widget, and
  `gui/merge/` for conflict-resolution UI (`ConflictMergeDialog`/`Model`/`Resolution`).
- `bridge/` — `EditorBridge`: the QWebChannel bridge between C++ and the in-browser BlockNote editor.
  This is the trust boundary — the JS editor must never get raw filesystem, token, database, or
  unrestricted network access; everything crosses through this bridge.
- `document/` — document model (`Document`, `BlockNoteSnapshot`) and `DocumentValidator`.
- `storage/` — `LocalDocumentRepository` interface with `FileDocumentRepository` (plain filesystem) and
  `CbliteDocumentRepository` (Couchbase Lite, gated by `CPPWIKI_ENABLE_CBLITE_STORAGE`); selected via
  `RepositoryFactory`.
- `sync/` — `DocumentSyncService`/`SyncService` talk to the backend for push/pull and conflict detection;
  `sync_bootstrap` wires sync into app startup.
- `auth/` — `AuthSessionManager` (OIDC/token lifecycle) and `AuthTokenStore` (secure token persistence via
  qtkeychain).
- `backend/` — `BackendClient`: HTTP client for the `cppwiki_server` API.
- `core/` — shared constants, logging setup, `qt_string` helpers, UUID helpers.

### Server (`src/server/`, target `cppwiki_server`, built on the userver v3.0 framework)

- `app/` — `main.cc` and `ServerApplication` entry point.
- `components/` — userver components: `CppwikiServerComponent` (wiring/registration),
  `SyncBootstrapComponent` (Sync Gateway integration bootstrap).
- `config/` — `RuntimeConfig` (parses `config/server.yaml`) and `StaticConfigFactory` (builds the userver
  static config from it).
- `handlers/` — one HTTP handler per concern: `admin_handler`, `health_handler`, `lock_handler`,
  `presence_handler`, `sync_config_handler`, `workspace_handler`, `openapi_handler`/`swagger_ui_handler`
  (serves `assets/openapi_spec.h` + Swagger UI), `protected_page_handler`, `options_handler` (CORS).
- `middleware/` — `AuthCheckerImpl` (JWT/OIDC verification against the configured issuer/JWKS),
  `LoggingMiddleware`.
- `service/` — business logic backing the handlers: `LockService`, `PresenceService`,
  `SyncGatewayAdapter` (talks to Couchbase Sync Gateway's admin/REST API).
- `dto/` — reflect-cpp response structs per resource (`health_response.h`, `lock_response.h`,
  `presence_response.h`, `sync_config_response.h`, `workspace_response.h`, `admin_response.h`),
  `response_envelope.h` (common envelope), `json_adapter.h`.

The server's auth model, sync channel mapping, and lock/presence concepts are documented in
`doc/modules/ROOT/pages/architecture/Server_and_Realtime_Editing_Architecture.adoc`,
`Desktop_Backend_Sync_Interaction.adoc`, and `Sync_Document_Model_and_Conflict_Contract.adoc` — read these
before making non-trivial changes to sync, locking, or auth flows.

### Third-party / vendored

- `third_party/qlementine` — Qt widget styling library, built as a subdirectory (desktop app only).
- Server deps (`userver`, `qtkeychain`) are fetched via CMake `FetchContent`, not vcpkg.

## C++ style

Follows the Google C++ Style Guide baseline, enforced by `.clang-format` and `.clang-tidy`. Key points not
obvious from the config alone (full rationale in `doc/architecture/Project_Structure_and_Style.md`):

- Headers (`.h`) and sources (`.cc`); tests are `*_test.cc`.
- Headers must be self-contained (include what you use, don't rely on transitive includes) and use
  include guards (`CPPWIKI_SRC_<PATH>_H_`), not `#pragma once`.
- Include order: related header, C headers, C++ stdlib, third-party/Qt, project headers — one blank line
  between non-empty groups.
- CMake is target-based only: `target_sources`/`target_link_libraries`/`target_include_directories`; no
  global include dirs or global compile flags. Qt executables use `qt_add_executable` +
  `qt_finalize_executable`.
- New modules under `src/` get their own narrow CMake target, following the existing directory layout
  (`core/`, `gui/`, `bridge/`, `storage/`, `auth/`, `sync/`, `server/...`).

## Documentation map

- `doc/modules/ROOT/pages/PRD_v9_Block_Document_Edition.adoc` — product requirements.
- `doc/modules/ROOT/pages/roadmap/Current_Roadmap.adoc`, `doc/modules/ROOT/pages/backlog/` — active
  planning.
- `doc/modules/ROOT/pages/architecture/adr/` — architecture decision records (editor choice, document
  model, collaboration strategy, plugin system, rendering pipeline, Confluence integration, Authentik auth
  model, server framework choice).
- `doc/modules/ROOT/pages/architecture/*.adoc` — deep-dives on desktop/backend sync interaction, the sync
  document model and conflict contract, the QWebChannel editor bridge, and server internals (some have
  `_ru` Russian variants).
- Docs are expected to trace back to delivery-discovery decisions (problem statement, scope, success
  criteria, gates) rather than replace them; AI/LLM/RAG-flavored initiatives use the templates in
  `doc/modules/ROOT/pages/ml-llm-delivery-pipeline/`.
