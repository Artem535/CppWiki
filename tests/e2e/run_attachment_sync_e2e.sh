#!/usr/bin/env bash
set -euo pipefail

test_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
compose_file="${test_root}/tests/e2e/attachment-sync/docker-compose.yml"
project_name="cppwiki-attachment-sync-e2e"
build_directory="${CPPWIKI_ATTACHMENT_E2E_BUILD_DIRECTORY:-${test_root}/build/attachment-e2e}"

cleanup() {
  rtk docker compose -p "${project_name}" -f "${compose_file}" down --volumes --remove-orphans
}
trap cleanup EXIT

if [[ ! -f "${build_directory}/build.ninja" ]]; then
  rtk cmake --preset attachment-e2e
fi

rtk cmake --build "${build_directory}" --target cppwiki_attachment_sync_e2e_tests --parallel
rtk docker compose -p "${project_name}" -f "${compose_file}" up --detach --wait couchbase sync-gateway
rtk docker compose -p "${project_name}" -f "${compose_file}" run --rm --no-deps sync-gateway-init
CPPWIKI_ATTACHMENT_E2E_GATEWAY_URL=ws://127.0.0.1:14984/cppwiki \
  "${build_directory}/tests/cppwiki_attachment_sync_e2e_tests"
