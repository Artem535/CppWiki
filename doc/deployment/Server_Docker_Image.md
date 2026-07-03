# CppWiki Server Docker Image

This image packages only the backend server. It does not build or ship the Qt desktop application
or the BlockNote editor bundle.

## Build

From the repository root:

```bash
docker build -f docker/server.Dockerfile -t cppwiki-server:local .
```

## CI/CD

GitHub Actions builds the image with `.github/workflows/server-image.yml`.

Behavior:

- pull requests build the image but do not publish it;
- pushes to `main` or `master` build and publish the image to GitHub Container Registry;
- version tags such as `v0.1.0` publish semver tags;
- manual runs are available through `workflow_dispatch`.

Published image name:

```text
ghcr.io/<owner>/cppwiki-server
```

Typical tags:

- branch tag, for example `main`;
- commit tag, for example `sha-<short-sha>`;
- release tags for `v*.*.*`, for example `0.1.0` and `0.1`.

Repository setting required:

- `Settings -> Actions -> General -> Workflow permissions` must allow read/write permissions, or
  the workflow must keep `permissions.packages: write`.

The Docker build uses the `server-release` CMake preset:

- `CPPWIKI_BUILD_DESKTOP_APP=OFF`
- `CPPWIKI_BUILD_SERVER=ON`
- `CPPWIKI_BUILD_TESTS=OFF`
- `CPPWIKI_ENABLE_CBLITE_STORAGE=OFF`
- `BUILD_SHARED_LIBS=OFF`

Dependency build caching:

- the Dockerfile copies `vcpkg.json` before the rest of the source tree;
- `vcpkg install` runs in its own layer, so regular source changes do not invalidate downloaded or
  built third-party packages;
- GitHub Actions stores Docker BuildKit cache with `cache-to: type=gha,mode=max`;
- the first CI build can still be slow, especially while building `grpc`/`protobuf` for OTLP, but
  later builds should reuse the dependency layer unless `vcpkg.json`, the Ubuntu version or the
  Dockerfile dependency stage changes.

If the dependency build remains too slow for clean runners, add a separate prebuilt builder image
published to GHCR and use it as the first stage for `docker/server.Dockerfile`.

## Run

The image includes `/etc/cppwiki/server.yaml` copied from `config/server.docker.yaml`.

```bash
docker run --rm \
  --name cppwiki-server \
  -p 8080:8080 \
  cppwiki-server:local
```

Health check:

```bash
curl -fsS http://127.0.0.1:8080/api/v1/health
```

## Runtime Config

For a real deployment, mount a host-specific config instead of baking machine-specific values into
the image:

```bash
docker run --rm \
  --name cppwiki-server \
  -p 8080:8080 \
  -v "$PWD/config/server.yaml:/etc/cppwiki/server.yaml:ro" \
  cppwiki-server:local
```

Alternatively, keep the bundled `/etc/cppwiki/server.yaml` and override deployment-specific values
with environment variables:

```bash
docker run --rm \
  --name cppwiki-server \
  -p 8080:8080 \
  -e CPPWIKI_AUTH_ISSUER="http://10.71.30.92:9000/application/o/cpp-wiki/" \
  -e CPPWIKI_AUTH_AUDIENCE="2n9RY6M8Oz6Cr3Ar6z9QIgh4WPD1KH8nD1IZRwLe" \
  -e CPPWIKI_AUTH_JWKS_URL="http://10.71.30.92:9000/application/o/cpp-wiki/jwks/" \
  -e CPPWIKI_SYNC_GATEWAY_URL="http://10.71.30.92:4984/cppwiki" \
  -e CPPWIKI_SYNC_ADMIN_URL="http://10.71.30.92:4985/cppwiki" \
  cppwiki-server:local
```

Environment overrides are applied after the YAML file and CLI flags.

| Variable | Meaning |
| :--- | :--- |
| `CPPWIKI_BIND_HOST` | HTTP bind address. Use `0.0.0.0` in containers. |
| `CPPWIKI_PORT` | HTTP port. |
| `CPPWIKI_LOG_LEVEL` | `trace`, `debug`, `info`, `warn`, `error`, `critical` or `off`. |
| `CPPWIKI_AUTH_ISSUER` | Expected JWT issuer. Must match token `iss` exactly. |
| `CPPWIKI_AUTH_AUDIENCE` | Expected JWT audience. Must match token `aud`. |
| `CPPWIKI_AUTH_JWKS_URL` | JWKS endpoint for token verification. |
| `CPPWIKI_SYNC_ENABLED` | `true`/`false`, `1`/`0`, `yes`/`no` or `on`/`off`. |
| `CPPWIKI_SYNC_GATEWAY_URL` | Public Sync Gateway replication URL. |
| `CPPWIKI_SYNC_ADMIN_URL` | Private Sync Gateway Admin API URL. |
| `CPPWIKI_SYNC_DATABASE_NAME` | Sync database name, usually `cppwiki`. |

Important config rules:

- `bind_host` must be `0.0.0.0` inside a container;
- `auth.issuer`, `auth.jwks_url` and the desktop login URL must use the same issuer host;
- `auth.audience` must match the JWT `aud` claim;
- `sync.gateway_url` is the public Sync Gateway replication URL;
- `sync.admin_url` is the private Sync Gateway Admin API URL and must not be exposed to untrusted
  networks.

When running in the same Docker Compose network as Sync Gateway, the backend may use:

```yaml
sync:
  gateway_url: http://sync-gateway:4984/cppwiki
  admin_url: http://sync-gateway:4985/cppwiki
```

When running outside that network, use the reachable VPN/LAN/tunnel address instead.
