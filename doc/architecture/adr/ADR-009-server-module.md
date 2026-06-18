# ADR-009 — Server Module (Drogon Backend)

## Status

Proposed

## Date

2026-06-17

## Related docs

- `doc/roadmap/Current_Roadmap.md` (Milestone 7: Backend Skeleton)
- `doc/architecture/Architecture_Baseline_Libraries_and_Approaches.md`
- `doc/architecture/adr/ADR-008-authentik-auth-model.md`

---

# Context

CppWiki is a desktop-first, offline-first wiki platform built with Qt 6 and C++20. The desktop application currently owns the editor host, local persistence and document model. To support future collaboration, sync, workspace-level permissions and external integrations, the project needs an optional standalone backend module running inside the same repository.

The Architecture Baseline already accepts Drogon as the backend framework. The roadmap’s Milestone 7 asks for:

- Drogon application target;
- `/health` endpoint;
- auth middleware placeholder;
- lock and presence API stubs;
- structured logging with `spdlog`.

We now need to fix how this backend is physically laid out in the codebase, how it relates to the Qt desktop target, and what contracts it exposes from day one.

---

# Decision

Introduce a separate `server` CMake target under `src/server/` built with the Drogon framework. The server is a standalone executable target `cppwiki_server` that can be built and run independently of the Qt desktop target.

Key boundaries:

1. `cppwiki_server` is not linked into `cppwiki_app` at runtime.
2. The server may reuse `src/core/`, `src/document/`, and `src/storage/` logic as **libraries**, but not the GUI or bridge layers.
3. Drogon controllers, filters and plugins live under `src/server/` only.
4. Public transport contracts are modelled as typed DTOs with `reflect-cpp`.
5. Logging uses `spdlog` and is configured per-component.

---

# Directory layout

```text
src/
  server/
    server_main.cc
    server_application.cc
    server_application.h
    config/
      server_config.h
    controllers/
      health_controller.h
      health_controller.cc
      auth_controller.h
      auth_controller.cc
      lock_controller.h
      lock_controller.cc
      presence_controller.h
      presence_controller.cc
    filters/
      jwt_auth_filter.h
      jwt_auth_filter.cc
    dto/
      api_error.h
      health_response.h
      auth_responses.h
      lock_responses.h
      presence_responses.h
    logging/
      logger.h
      logger.cc
  core/      (shared)
  document/  (shared)
  storage/   (shared)
```

If reuse of `core`/`document`/`storage` creates circular linkage, extract small static libraries (e.g. `cppwiki_core`, `cppwiki_document`) before continuing.

---

# Target ownership

| Target | Type | Notes |
| :--- | :--- | :--- |
| `cppwiki_app` | `qt_add_executable` | Qt desktop shell; does not depend on Drogon. |
| `cppwiki_server` | `add_executable` | Drogon HTTP/WebSocket backend; does not depend on Qt Widgets/WebEngine. |
| `cppwiki_core` | `add_library` (future) | UUID, string helpers, constants. |
| `cppwiki_document` | `add_library` (future) | Document DTOs, validator, `reflect-cpp` types. |
| `cppwiki_storage` | `add_library` (future) | `LocalDocumentRepository` and adapters. |

For the skeleton stage, `cppwiki_server` may compile its own copies of the shared headers from `src/`. The repository should not run two build systems.

---

# API contract baseline

## Common envelope

All API responses share a stable envelope:

```json
{
  "apiVersion": "v1",
  "ok": true,
  "result": { }
}
```

or

```json
{
  "apiVersion": "v1",
  "ok": false,
  "error": {
    "code": "unauthorized",
    "message": "..."
  }
}
```

The envelope shape mirrors the existing QWebChannel bridge envelope used by `editor_bridge`.

## Routes

| Method | Route | Controller | Auth | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/v1/health` | `HealthController` | none | Liveness/readiness endpoint. |
| `POST` | `/api/v1/auth/refresh` | `AuthController` | none (token required) | Refresh access token placeholder. |
| `GET` | `/api/v1/auth/me` | `AuthController` | JWT | Current user placeholder. |
| `POST` | `/api/v1/locks/{pageId}` | `LockController` | JWT | Acquire or refresh page lock. |
| `DELETE` | `/api/v1/locks/{pageId}` | `LockController` | JWT | Release page lock. |
| `GET` | `/api/v1/locks/{pageId}` | `LockController` | JWT | Get lock state. |
| `POST` | `/api/v1/presence/{pageId}` | `PresenceController` | JWT | Post presence heartbeat. |
| `GET` | `/api/v1/presence/{pageId}` | `PresenceController` | JWT | List present users placeholder. |

JWT validation is implemented as a Drogon filter `JwtAuthFilter` that rejects unauthenticated requests with HTTP 401 and the same envelope shape.

---

# Consequences

## Positive

- The backend is physically isolated from the Qt desktop GUI.
- Drogon executables build fast and run as a lightweight service.
- The envelope shape stays consistent with the desktop bridge contract.
- Locks and presence endpoints exist as early stubs, ready for MVP implementation.

## Negative / Trade-offs

- We now maintain two executable targets.
- Shared code between Qt app and server may require library extraction later.
- Drogon and Qt share some event-loop/network assumptions; running both in one process is intentionally avoided.

## Architecture impact

- `src/CMakeLists.txt` must add a conditional `add_subdirectory(server)` or include the server files separately.
- `vcpkg.json` already includes `drogon`; no manifest change is needed.
- `cppwiki_server` must not pull in `Qt6::Widgets`, `Qt6::WebChannel` or `Qt6::WebEngineWidgets`.

## Risks and mitigations

| Risk | Mitigation |
| :--- | :--- |
| Linking Qt headers by accident from server code. | Code review + `target_link_libraries` restricted to Drogon/spdlog/reflectcpp. |
| Duplicated document DTOs in server vs desktop. | Keep shared headers under `src/document/`; extract a static library when duplication becomes problematic. |
| Drogon default feature set is too large. | `vcpkg.json` already disables default features. |

---

# Lifecycle

| Date | Status | Change | Author |
| :--- | :--- | :--- | :--- |
| 2026-06-17 | Proposed | Initial server module skeleton design | Cline |
