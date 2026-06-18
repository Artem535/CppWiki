# Server and Realtime Editing Architecture

**Product:** CppWiki / Wiki Platform v9 - Block Document Edition  
**Status:** Draft / aligned with the selected `oat++` backend  
**Date:** 2026-06-18  
**Related docs:** `doc/PRD_v9_Block_Document_Edition.md`, `doc/roadmap/Current_Roadmap.md`, `doc/roadmap/Realistic_Delivery_Pipeline.md`, `doc/architecture/Architecture_Baseline_Libraries_and_Approaches.md`, `doc/architecture/adr/ADR-003-collaboration-strategy.md`, `doc/architecture/adr/ADR-008-authentik-auth-model.md`

---

# 1. Purpose

This document fixes the backend architecture for CppWiki after the framework decision has been settled on `oat++`.

The goal is to keep the server useful without making it the center of the product too early. The desktop app must keep working offline. The backend must be optional in the early phases, but the architecture must already support:

- health and diagnostics;
- explicit public/protected route separation;
- document lock ownership and heartbeat;
- presence updates;
- future authenticated APIs;
- a later realtime collaboration path.

The backend must not become a hidden second editor. It coordinates access and collaboration state. The editor remains BlockNote inside QWebEngine, and local document persistence remains the desktop application's responsibility in the MVP path.

---

# 2. Core Decisions

## 2.1. Fixed backend stack

Use these libraries and tools for the server layer:

- `oat++` for HTTP endpoints and future WebSocket services;
- `oatpp-swagger` for local API documentation;
- `CLI11` for command-line overrides;
- `reflect-cpp` for typed YAML config loading and DTO mapping;
- `spdlog` for structured logging.

## 2.2. Collaboration model

Use `Single Writer / Multi Reader` for the MVP path.

That means:

- one active editor owns the document lock;
- other users can view the latest saved version;
- presence is visible but not collaborative merging;
- stale locks can be force released by authorized users;
- realtime co-editing is deferred until the stable document model and server contracts exist.

## 2.3. Realtime future

Keep the architecture compatible with a later collaborative editing layer.

The future path should remain possible through:

```text
BlockNote -> Tiptap -> ProseMirror -> Yjs -> WebSocket provider
```

The backend should therefore transport document IDs, page IDs, lock state, presence and collaboration events. It should not depend on editor DOM, browser internals or raw BlockNote runtime state.

---

# 3. System Overview

```text
Qt Desktop Shell
  - navigation
  - settings
  - title bar
  - local document repository
  - editor host
        |
        | QWebChannel
        v
BlockNote Editor in QWebEngine
  - edit document
  - emit snapshot updates
  - receive initial document and read-only state

Desktop App <---- HTTP/WebSocket ----> oat++ Backend
  - health
  - locks
  - presence
  - auth boundary
  - future workspace/page APIs

Backend <----> Authentik
Backend <----> Couchbase Sync Gateway / Couchbase Server
Backend <----> future collaboration gateway
```

Responsibility split:

- Qt owns the product shell and the local document experience.
- The web editor owns only the block editing surface.
- The backend owns access control, lock state, presence and API boundaries.
- Sync and auth integrations remain behind clear service boundaries.

---

# 4. Current Server Layout

The current code layout is intentionally small and maps cleanly to the phase 5 skeleton:

```text
src/server/
  app/
    main.cc
    server_application.cc
    server_application.h
  config/
    server_config.cc
    server_config.h
    server_config_dto.h
  dto/
    api_dto.h
  http/
    health_controller.h
  openapi/
    swagger.cc
    swagger.h
```

Current responsibilities:

- `main.cc` parses CLI options, loads YAML config and configures logging.
- `server_config.*` loads typed config from YAML and applies overrides.
- `health_controller.h` exposes the health endpoint and CORS preflight.
- `swagger.*` registers API docs when Swagger is enabled.
- `server_application.*` wires the router, controllers and network server.

Planned additions should stay narrow and explicit:

```text
src/server/
  auth/
  middleware/
  locks/
  presence/
  service/
  realtime/
```

If a new server feature cannot fit in one of those boundaries, it probably needs a smaller boundary first.

---

# 5. Request Pipeline

## 5.1. Startup

Startup order:

1. initialize oatpp runtime;
2. configure base logging;
3. parse CLI overrides;
4. load YAML config through `reflect-cpp`;
5. apply CLI overrides on top of config;
6. configure log level;
7. build router and register controllers;
8. start the server.

The current config contract is intentionally small:

```yaml
bind_host: 127.0.0.1
port: 8080
enable_swagger: true
log_level: info
```

CLI overrides remain available for local development and tests:

- `--config`
- `--bind-host`
- `--port`
- `--log-level`
- `--swagger`
- `--no-swagger`

## 5.2. Middleware meaning

Middleware is code that runs before a controller handles the request.

Its job is to:

- log the request;
- attach request metadata;
- reject unauthorized traffic;
- separate public routes from protected routes;
- normalize response headers and error handling.

For this project, middleware should stay boring and predictable. It is not a place for business logic.

## 5.3. Route layers

Use explicit route layers:

### Public routes

- `/api/v1/health`
- `/swagger/*` when enabled
- later: login callback or public metadata endpoints if needed

### Protected routes

- workspace APIs
- page APIs
- document metadata APIs
- lock APIs
- presence APIs
- audit or admin APIs

The public layer must remain reachable without JWT. The protected layer must be the only place where authentication and authorization are enforced.

---

# 6. API Shape

## 6.1. Envelope

Keep the response shape explicit and stable:

```json
{
  "apiVersion": 1,
  "ok": true,
  "result": {}
}
```

Errors should use the same outer envelope with an error payload instead of ad hoc string responses.

## 6.2. DTO rules

Use typed DTOs for transport payloads.

Rules:

- transport DTOs are not domain models;
- typed config and DTO parsing should use `reflect-cpp`;
- raw JSON should be preserved only where the editor snapshot must remain lossless;
- document IDs and block IDs must stay stable;
- API contracts must be versioned.

## 6.3. Health endpoint

The current health endpoint is intentionally simple:

- `GET /api/v1/health`
- `OPTIONS /api/v1/health`

It reports basic liveness and returns CORS headers for local development and browser-based tooling.

---

# 7. Realtime Editing Strategy

## 7.1. MVP path

The MVP path is not simultaneous co-editing. It is lock-based coordination around local editing.

Flow:

1. user selects a page;
2. desktop app requests lock ownership from backend;
3. backend grants or denies the lock;
4. if granted, editor opens in editable mode;
5. desktop app sends heartbeat every 10 seconds;
6. if heartbeat stops, the lock expires;
7. authorized users can force release stale locks;
8. other users see read-only content and presence state.

This gives the product a stable collaboration boundary without pretending that realtime merge is already solved.

## 7.2. Presence

Presence is ephemeral state.

It should carry information like:

- who is viewing the page;
- whether the user is editing or reading;
- last seen time;
- current page or workspace scope.

Presence must not mutate document content.

## 7.3. Future realtime collaboration

When the block model, IDs and persistence are stable, the backend can evolve toward a realtime collaboration gateway.

That future layer should:

- operate on document IDs and block IDs;
- exchange patches or CRDT updates, not DOM fragments;
- keep editor implementation details out of the transport protocol;
- preserve fallback lock-based behavior for environments that do not use realtime co-editing.

The realtime path may live inside `oat++` or as a separate collaboration service if protocol complexity grows. The architecture should not force that decision too early.

---

# 8. Authentication and Middleware

## 8.1. Auth phase

The server auth phase is separate from the health and lock skeleton.

When auth lands, the server should:

- validate JWTs on protected routes;
- keep public routes readable without a token;
- map identity claims to workspace and page permissions;
- log access denials and lock takeovers;
- never expose tokens to the web editor.

## 8.2. Middleware chain

Recommended request chain for protected routes:

1. request ID middleware;
2. logging middleware;
3. auth middleware;
4. permission or role checks;
5. controller handler;
6. structured error conversion.

The auth middleware should stay narrow. It validates identity and exposes claims. It should not contain workspace business rules.

## 8.3. Desktop token handling

Desktop login must keep tokens out of plain text config files.

The expected desktop flow is:

- system browser login;
- local callback or custom URI scheme;
- token exchange on the native side;
- secure storage in system keyring;
- backend receives only the access token when needed.

---

# 9. Data and Storage Boundaries

## 9.1. Local editing remains local

The desktop application remains responsible for:

- local document list;
- local document selection;
- autosave;
- validation before persistence;
- offline recovery.

The backend should not become the canonical local document store in the MVP path.

## 9.2. Stable identity

Required stable fields:

- `workspace_id`;
- `page_id`;
- `document_id`;
- `block_id`;
- `parent_id`;
- `sort_order`;
- timestamps.

These IDs are what make future sync, lock takeover and realtime collaboration possible without a data migration rewrite.

## 9.3. Hashes

Document hash storage is optional and deferred.

Only add a hash if it helps one of these:

- dirty-state optimization;
- sync conflict detection;
- save skipping;
- change auditing.

Do not add it just because it is possible.

---

# 10. Observability

Logging should make the phase boundaries visible.

Minimum log fields:

- component;
- operation;
- request ID;
- page ID or document ID when relevant;
- lock owner when relevant;
- error category.

Do not log:

- access tokens;
- refresh tokens;
- raw confidential document content;
- browser callback secrets.

The current health endpoint is the minimum liveness check. Future routes should return structured errors and logs that can be correlated with user-facing problems.

---

# 11. Testing Strategy

Minimum server tests:

- config load and override tests;
- health endpoint test;
- Swagger registration test;
- protected/public route separation test;
- lock acquire/release/heartbeat tests;
- presence stub test;
- auth middleware test when phase 6 lands;
- integration test that the backend starts with the configured port and log level.

Minimum architecture tests for the product flow:

- desktop app must still run without the backend;
- local editing must still work if the server is down;
- lock denial must switch the editor to read-only mode;
- presence must not mutate document snapshots.

---

# 12. Phase Map

## Phase 5

Implement the server skeleton:

- health endpoint;
- logging;
- YAML config;
- CLI11 overrides;
- Swagger;
- public/protected route split;
- lock and presence stubs.

## Phase 6

Add auth:

- OIDC flow;
- keyring token storage;
- JWT validation middleware;
- protected API enforcement.

## Phase 7

Make the lock model authoritative:

- lock ownership;
- heartbeat;
- read-only fallback;
- force release.

## Phase 8 and later

Add sync and then realtime collaboration:

- Sync Gateway integration;
- channel mapping;
- conflict handling;
- future collaboration gateway.

---

# 13. Change Rule

Any future change that affects the backend framework, the auth model, the lock model, the WebSocket protocol or the request envelope requires an ADR update.

The `oat++` choice is fixed for the current architecture.
