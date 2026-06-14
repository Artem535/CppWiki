#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
metadata_file="${repo_root}/third_party/couchbase-lite.version"
deps_root="${repo_root}/.deps/couchbase-lite"
cache_root="${repo_root}/.deps/cache/couchbase-lite"

usage() {
  cat <<'EOF'
Usage: scripts/bootstrap-cblite.sh [platform]

Platforms:
  linux-x86_64
  macos-arm64
  windows-x86_64

The script reads third_party/couchbase-lite.version, downloads the pinned
Couchbase Lite archive, verifies SHA256, and unpacks it into:

  .deps/couchbase-lite/<platform>

EOF
}

detect_platform() {
  uname_s=$(uname -s)
  uname_m=$(uname -m)

  case "${uname_s}:${uname_m}" in
    Linux:x86_64|Linux:amd64)
      printf '%s\n' "linux-x86_64"
      ;;
    Darwin:arm64|Darwin:aarch64)
      printf '%s\n' "macos-arm64"
      ;;
    *)
      printf >&2 'Unsupported host platform: %s %s\n' "${uname_s}" "${uname_m}"
      printf >&2 'Pass one of: linux-x86_64, macos-arm64, windows-x86_64\n'
      exit 2
      ;;
  esac
}

metadata_value() {
  key=$1
  value=$(awk -F= -v key="${key}" '
    $1 == key {
      sub(/^[^=]*=/, "")
      print
      exit
    }
  ' "${metadata_file}")
  printf '%s\n' "${value}"
}

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf >&2 'Required command not found: %s\n' "$1"
    exit 2
  fi
}

download_file() {
  url=$1
  output=$2

  if command -v curl >/dev/null 2>&1; then
    curl --fail --location --show-error --output "${output}" "${url}"
    return
  fi

  if command -v wget >/dev/null 2>&1; then
    wget --output-document="${output}" "${url}"
    return
  fi

  printf >&2 'Required command not found: curl or wget\n'
  exit 2
}

verify_sha256() {
  file=$1
  expected=$2

  if command -v sha256sum >/dev/null 2>&1; then
    actual=$(sha256sum "${file}" | awk '{ print $1 }')
  elif command -v shasum >/dev/null 2>&1; then
    actual=$(shasum -a 256 "${file}" | awk '{ print $1 }')
  else
    printf >&2 'Required command not found: sha256sum or shasum\n'
    exit 2
  fi

  if [ "${actual}" != "${expected}" ]; then
    printf >&2 'SHA256 mismatch for %s\n' "${file}"
    printf >&2 'Expected: %s\n' "${expected}"
    printf >&2 'Actual:   %s\n' "${actual}"
    exit 1
  fi
}

extract_archive() {
  archive=$1
  output_dir=$2
  tmp_dir="${output_dir}.tmp"

  rm -rf "${tmp_dir}"
  mkdir -p "${tmp_dir}"

  case "${archive}" in
    *.zip)
      require_command unzip
      unzip -q "${archive}" -d "${tmp_dir}"
      ;;
    *.tar.gz|*.tgz)
      require_command tar
      tar -xzf "${archive}" -C "${tmp_dir}"
      ;;
    *.tar.xz)
      require_command tar
      tar -xJf "${archive}" -C "${tmp_dir}"
      ;;
    *)
      printf >&2 'Unsupported archive extension: %s\n' "${archive}"
      exit 2
      ;;
  esac

  normalized_root=$(find "${tmp_dir}" -type d -name include -prune -print | head -n 1 | sed 's#/include$##')
  if [ -z "${normalized_root}" ]; then
    printf >&2 'Could not find include/ in extracted archive.\n'
    exit 1
  fi

  if [ ! -d "${normalized_root}/lib" ] && [ ! -d "${normalized_root}/bin" ]; then
    printf >&2 'Could not find lib/ or bin/ next to include/ in extracted archive.\n'
    exit 1
  fi

  rm -rf "${output_dir}"
  mkdir -p "$(dirname -- "${output_dir}")"
  mv "${normalized_root}" "${output_dir}"
  rm -rf "${tmp_dir}"
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  usage
  exit 0
fi

platform=${1:-$(detect_platform)}
case "${platform}" in
  linux-x86_64|macos-arm64|windows-x86_64)
    ;;
  *)
    printf >&2 'Unsupported platform: %s\n' "${platform}"
    usage
    exit 2
    ;;
esac

if [ ! -f "${metadata_file}" ]; then
  printf >&2 'Metadata file not found: %s\n' "${metadata_file}"
  exit 1
fi

metadata_key=$(printf '%s' "${platform}" | tr '-' '_')
version=$(metadata_value version)
url=$(metadata_value "${metadata_key}_url")
sha256=$(metadata_value "${metadata_key}_sha256")

if [ -z "${version}" ] || [ -z "${url}" ] || [ -z "${sha256}" ]; then
  printf >&2 'Couchbase Lite metadata is incomplete for %s.\n' "${platform}"
  printf >&2 'Fill version, %s_url and %s_sha256 in %s.\n' "${metadata_key}" "${metadata_key}" "${metadata_file}"
  exit 1
fi

mkdir -p "${cache_root}"
archive_name="couchbase-lite-${version}-${platform}-${url##*/}"
archive_path="${cache_root}/${archive_name}"
output_dir="${deps_root}/${platform}"

if [ -f "${archive_path}" ]; then
  printf 'Using cached archive: %s\n' "${archive_path}"
else
  printf 'Downloading Couchbase Lite %s for %s...\n' "${version}" "${platform}"
  download_file "${url}" "${archive_path}"
fi

printf 'Verifying SHA256...\n'
verify_sha256 "${archive_path}" "${sha256}"

printf 'Extracting into %s...\n' "${output_dir}"
extract_archive "${archive_path}" "${output_dir}"

printf '\nCouchbase Lite is ready.\n'
printf 'Configure CMake with:\n'
printf '  -DCPPWIKI_ENABLE_CBLITE_STORAGE=ON -DCPPWIKI_CBLITE_ROOT=%s\n' "${output_dir}"
