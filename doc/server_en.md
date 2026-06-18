# CppWiki Server

This document describes the CppWiki server module — an optional Drogon-based backend that currently exposes an API skeleton and is being prepared for future collaboration, synchronization, and external integrations.

> **Status:** skeleton stage (Milestone 7). All endpoints except `/health` return stubs inside the correct response envelope.

---

## Table of contents

1. [Purpose](#purpose)
2. [Architecture and boundaries](#architecture-and-boundaries)
3. [Build and run](#build-and-run)
4. [Configuration](#configuration)
5. [Logging](#logging)
6. [Routes and contracts](#routes-and-contracts)
7. [Authentication and filters](#authentication-and-filters)
8. [Controllers](#controllers)
9. [DTOs and response format](#dtos-and-response-format)
10. [Future work](#future-work)

---

## Purpose

The server module solves two problems at once:

1. **Give CppWiki a network-accessible service layer** for remote access, collaborative editing, page locking, presence, permissions, and integrations.
2. **Keep the desktop application offline-first** — the server is built and started as a separate binary `cppwiki-server` and has no link-time or runtime dependency on Qt Widgets or Qt WebEngine.

The desktop app and the server may reuse shared code from `src/core/`, `src/document/`, and `src/storage/`, but they remain separate build targets.

---

## Architecture and boundaries

```
src/
  server/
    server_main.cc              # entry point
    server_application.{h,cc}   # Drogon initialization and event loop
    server_config.h             # static default settings
    controllers/                # HTTP controllers
      health_controller.{h,cc}
      auth_controller.{h,cc}
      lock_controller.{h,cc}
      presence_controller.{h,cc}
    filters/                    # request processing filters
      jwt_auth_filter.{h,cc}
    dto/                        # typed API contracts
      api_response.h
      health_response.h
      auth_responses.h
      lock_responses.h
      presence_responses.h
    logging/                    # spdlog configuration
      logger.{h,cc}
  core/        (shared)
  document/    (shared)
  storage/     (shared)
```

### Build targets

| Target | Type | Description |
| :--- | :--- | :--- |
| `cppwiki_app` | `qt_add_executable` | Qt desktop client; no Drogon dependency. |
| `cppwiki_server` | `add_executable` | Drogon HTTP/WebSocket backend; no Qt dependency. |
| `cppwiki_core` | `add_library` (future) | UUID, string helpers, constants. |
| `cppwiki_document` | `add_library` (future) | Document DTOs and validator. |
| `cppwiki_storage` | `add_library` (future) | Repository and storage adapters. |

---

## Build and run

The backend builds with the same CMake system as the desktop application. The `vcpkg.json` manifest already declares `drogon`, `spdlog`, and `reflectcpp`.

### Building the server

```bash
# with CMakePresets if configured
# or the classic way:
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target cppwiki_server
```

### Running

```bash
./build/src/server/cppwiki-server
```

After startup the server listens on the address and port defined by `ServerConfig` defaults:

```text
0.0.0.0:8080
```

Worker thread count (`0` means Drogon auto-select) and log level are also configured in `server_config.h`.

### Default routes

```text
GET    /api/v1/health              # no auth
POST   /api/v1/auth/refresh        # requires Bearer header
GET    /api/v1/auth/me             # requires Bearer header
POST   /api/v1/locks/{pageId}      # requires Bearer header
GET    /api/v1/locks/{pageId}      # requires Bearer header
DELETE /api/v1/locks/{pageId}      # requires Bearer header
POST   /api/v1/presence/{pageId}   # requires Bearer header
GET    /api/v1/presence/{pageId}   # requires Bearer header
```

---

## Configuration

At the skeleton stage configuration is static and expressed by the `cppwiki::server::ServerConfig` struct:

```cpp
struct ServerConfig {
  std::string http_host = "0.0.0.0";
  std::uint16_t http_port = 8080;
  std::int32_t thread_num = 0;  // 0 lets Drogon choose based on hardware
  std::string log_level = "info";
};
```

Future iterations are expected to load these values from a JSON config file or environment variables.

---

## Logging

The server uses `spdlog` through a single shared logger:

```cpp
namespace cppwiki::server::logging {

[[nodiscard]] auto ServerLogger() -> std::shared_ptr<spdlog::logger>;
void InitializeLogging();

}
```

- Uses a colored stdout sink.
- Logger name: `cppwiki_server`.
- Pattern: `[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v`.
- Default level: `info`.
- Initialization is thread-safe (`std::call_once`).

---

## Routes and contracts

All responses are wrapped in a stable JSON envelope.

### Success envelope

```json
{
  "apiVersion": "v1",
  "ok": true,
  "result": { /* payload */ }
}
```

### Error envelope

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

The envelope matches the contract already used by the `editor_bridge` QWebChannel bridge, making it easier for the existing frontend to consume the backend later.

### Route table

| Method | Route | Controller | Auth | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/v1/health` | `HealthController` | none | Liveness/readiness probe. |
| `POST` | `/api/v1/auth/refresh` | `AuthController` | Bearer | Token refresh stub. |
| `GET` | `/api/v1/auth/me` | `AuthController` | Bearer | Current user stub. |
| `POST` | `/api/v1/locks/{pageId}` | `LockController` | Bearer | Acquire or refresh a page lock. |
| `DELETE` | `/api/v1/locks/{pageId}` | `LockController` | Bearer | Release a page lock. |
| `GET` | `/api/v1/locks/{pageId}` | `LockController` | Bearer | Get lock state. |
| `POST` | `/api/v1/presence/{pageId}` | `PresenceController` | Bearer | Post presence heartbeat. |
| `GET` | `/api/v1/presence/{pageId}` | `PresenceController` | Bearer | List present users. |

---

## Authentication and filters

### JwtAuthFilter

```cpp
namespace cppwiki::server::filters {

class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter> {
  void doFilter(const drogon::HttpRequestPtr& request,
                drogon::FilterCallback&& fcb,
                drogon::FilterChainCallback&& fccb) override;
  // ...
};

}
```

At the skeleton stage the filter deliberately only handles the validation that can be safely implemented now:

- Checks for the presence of an `Authorization` header.
- Verifies that it starts with `Bearer `.
- Extracts the token and attaches it to the request attributes under the key `jwt_token`.

If the header is missing or malformed, the filter returns `HTTP 401` with the error envelope:

```json
{
  "ok": false,
  "error": {
    "code": "unauthorized",
    "message": "Missing or invalid Authorization header"
  }
}
```

> **Note.** Signature, issuer, audience, and expiration checks are deferred to the Authentik integration described in ADR-008 (Milestone 6).

Controllers that need a user id currently read it from the `jwt_token` attribute. At this stage that value is used as a trace-like identifier rather than a validated subject.

---

## Controllers

### HealthController

Service liveness probe. Returns `HTTP 200` with a small payload:

```json
{
  "apiVersion": "v1",
  "ok": true,
  "result": {
    "status": "ok",
    "version": "0.1.0"
  }
}
```

### AuthController

Currently stub-only:

- `POST /api/v1/auth/refresh` — echoes back an `access_token` with type `Bearer` and `expires_in = 3600`.
- `GET /api/v1/auth/me` — returns a stub profile:

```json
{
  "sub": "<token from attributes>",
  "email": "user@example.com",
  "display_name": "Stub User",
  "roles": ["wiki.viewer"]
}
```

### LockController

Stubs for the page-locking API:

```json
// POST /api/v1/locks/{pageId}
{
  "page_id": "...",
  "acquired": true,
  "released": false,
  "owner_user_id": "...",
  "expires_in_seconds": 30
}

// DELETE /api/v1/locks/{pageId}
{
  "page_id": "...",
  "acquired": false,
  "released": true,
  "owner_user_id": "",
  "expires_in_seconds": 0
}

// GET /api/v1/locks/{pageId}
{
  "page_id": "...",
  "locked": false,
  "owner_user_id": "",
  "acquired_at": "",
  "expires_in_seconds": 0
}
```

### PresenceController

Stubs for co-editing presence:

```json
// POST /api/v1/presence/{pageId}
{
  "page_id": "...",
  "user_id": "...",
  "heartbeat_interval_seconds": 10
}

// GET /api/v1/presence/{pageId}
{
  "page_id": "...",
  "users": []
}
```

---

## DTOs and response format

Server DTOs are typed and serialized through `reflect-cpp`. The central template:

```cpp
template <typename T>
struct ApiResponse {
  std::string api_version = "v1";
  bool ok = false;
  std::optional<ApiError> error;
  std::optional<T> result;
};
```

Helpers:

- `dto::SuccessJson(payload)` — wraps any `reflect-cpp`-serializable DTO `T` into a success envelope.
- `dto::ErrorJson(code, message)` — returns an error envelope payload.

### DTO groups

| Group | Structures | Purpose |
| :--- | :--- | :--- |
| `health_response` | `HealthResponse` | Status and version. |
| `auth_responses` | `RefreshResponse`, `UserProfileResponse` | Tokens and profile. |
| `lock_responses` | `LockStateResponse`, `LockActionResponse` | Page locks. |
| `presence_responses` | `PresentUser`, `PresenceHeartbeatResponse`, `PresenceListResponse` | Online users. |

---

## Future work

The server module is a foundation, not a final product. The next iterations should address:

1. **Authentik-powered authorization**
   - JWKS signature validation.
   - Verifying `iss`, `aud`, and `exp`.
   - Returning a real `sub` separate from the raw token.

2. **Real locking and presence state**
   - In-memory or persistent backends (Redis/Couchbase/PostgreSQL).
   - TTL-based fair expiration.
   - Conflict responses when acquiring someone else's lock.

3. **Extract shared DTOs into libraries**
   - Split `cppwiki_core`, `cppwiki_document`, and `cppwiki_storage` to avoid duplication between the Qt app and the backend.

4. **File- or env-based configuration**
   - Drogon JSON config or environment variables for host, port, and log level.

5. **Tests**
   - Controller integration tests via `drogon::HttpTest`.
   - Filter tests for missing or malformed `Authorization` headers.

6. **OpenAPI documentation**
   - Swagger/OpenAPI spec once route contracts stabilize.

7. **Storage integration**
   - Wire `src/storage/` into the server when a remote document mode is introduced.

---

## Related documents

- `doc/architecture/adr/ADR-009-server-module.md` — architecture decision for the server module.
- `doc/architecture/adr/ADR-008-authentik-auth-model.md` — authorization model.
- `doc/roadmap/Current_Roadmap.md` — roadmap, Milestone 7.
- `src/server/CMakeLists.txt` — backend build configuration.

---

## Quick reference: build, run, and smoke test

```bash
# 1. Build the server
cmake --build build --target cppwiki_server

# 2. Run it
./build/src/server/cppwiki-server

# 3. Health check
curl http://localhost:8080/api/v1/health

# 4. Filter check — without a token this should return 401
curl -X POST http://localhost:8080/api/v1/locks/test-page

# 5. Endpoint with an arbitrary token
curl -H "Authorization: Bearer fake-token" \
     -X POST http://localhost:8080/api/v1/locks/test-page
```
