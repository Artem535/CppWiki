# ADR-009: Server Framework Selection — userver

**Status:** Accepted  
**Date:** 2026-06-18  
**Author:** Artem  
**Related ADRs:** ADR-003 Collaboration Strategy, ADR-008 Authentik Auth Model

---

## Context

Phase 5 of CppWiki required a backend skeleton that supports:

- health endpoint;
- structured logging;
- config + CLI overrides;
- explicit public/protected route split;
- lock and presence stubs;
- future expansion to auth, Sync Gateway and optional realtime collaboration.

An earlier version of the architecture listed `oat++` as the selected backend framework. After a focused migration effort (Phase 5), the backend is being built on `userver`.

## Decision

Use **userver** as the C++ server framework for CppWiki.

userver was chosen for this phase because it provides:

- an explicit `components_manager` / static-config model that encourages deterministic startup;
- built-in handler base classes, request context, tracing spans and middleware hooks;
- a coroutine runtime with HTTP and WebSocket handler support in the core library;
- clear auth-checker extension points for public/protected route separation;
- first-class JSON/YAML support inside the framework, reducing the need for a separate JSON library for server DTOs;
- OpenTelemetry-ready span tags and logging integration points (USERVER_FEATURE_OTLP available).

The desktop app, Qt/main event loop and local editing path remain untouched by this decision, because `cppwiki_server` is a separate binary.

## Consequences

- `vcpkg.json` no longer depends on `oatpp`, `oatpp-swagger`, `oatpp-websocket` or `yaml-cpp`.
- The server is consumed via `FetchContent(userver)`, pinned to a release tag (`v3.0`).
- Configuration is split into:
  - `RuntimeConfig` parsed from CLI and optional YAML (reflect-cpp remains for the YAML contract);
  - generated userver static config passed to `components::Run`.
- Handlers, middleware, services and DTOs now live in `src/server/{app,components,config,handlers,middleware,service,dto}`.
- Swagger/OpenAPI UI is kept as a lightweight manual verification surface for the backend contract, including bearer-token testing of protected routes.
- Auth checker is implemented as a custom JWT validator for protected routes, backed by Authentik-issued bearer tokens and JWKS verification.

## Alternatives Considered

| Option | Reason for Rejection |
| :--- | :--- |
| Keep oat++ | Removed to consolidate on a framework with stronger static-config, span/tracing and auth extensibility for upcoming phases. |
| Drogon | Drogon is capable, but userver's task-processor model aligns better with the planned observability and service-component boundaries. |
| POCO | Too large for the current skeleton and less coroutine-native. |
| Custom server | Rebuilding HTTP routing, request context and auth plumbing would introduce unnecessary risk. |

## Open Decisions

- Whether to enable `USERVER_FEATURE_OTLP=ON` in CI/release profiles once OTLP endpoint config is added.
- How to version-pin userver long-term; currently pinned to `v3.0` and can be advanced through an explicit ADR update.

## Migration Notes

- All oat++ source files under `src/server/` were removed.
- `config/server.yaml` remains reflect-cpp-parseable and now also carries backend JWT settings (`issuer`, `audience`, `jwks_url`) for protected-route validation.
- Health endpoint contract preserved: `{ apiVersion, ok, result }`.
