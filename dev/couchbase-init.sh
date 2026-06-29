#!/usr/bin/env bash
set -euo pipefail

echo "CB_HOST=${CB_HOST}"
echo "Checking Couchbase cluster status..."

if ! couchbase-cli server-list \
  --cluster "http://${CB_HOST}:8091" \
  --username "${CB_ADMIN_USER}" \
  --password "${CB_ADMIN_PASSWORD}" >/dev/null 2>&1; then

  echo "Initializing Couchbase cluster..."

  couchbase-cli cluster-init \
    --cluster "http://${CB_HOST}:8091" \
    --cluster-username "${CB_ADMIN_USER}" \
    --cluster-password "${CB_ADMIN_PASSWORD}" \
    --services data,index,query \
    --cluster-ramsize 1024 \
    --cluster-index-ramsize 256 \
    --index-storage-setting default
else
  echo "Cluster already initialized."
fi

echo "Checking bucket ${CB_BUCKET}..."

if ! couchbase-cli bucket-list \
  --cluster "http://${CB_HOST}:8091" \
  --username "${CB_ADMIN_USER}" \
  --password "${CB_ADMIN_PASSWORD}" \
  | grep -q "^${CB_BUCKET} "; then

  echo "Creating bucket ${CB_BUCKET}..."

  couchbase-cli bucket-create \
    --cluster "http://${CB_HOST}:8091" \
    --username "${CB_ADMIN_USER}" \
    --password "${CB_ADMIN_PASSWORD}" \
    --bucket "${CB_BUCKET}" \
    --bucket-type couchbase \
    --bucket-ramsize "${CB_BUCKET_RAMSIZE_MB}" \
    --storage-backend couchstore \
    --bucket-replica 0 \
    --enable-flush 1 \
    --wait
else
  echo "Bucket already exists."
fi

echo "Checking collection ${CB_BUCKET}._default.documents..."

collections_json="$(
  curl -fsS \
    -u "${CB_ADMIN_USER}:${CB_ADMIN_PASSWORD}" \
    "http://${CB_HOST}:8091/pools/default/buckets/${CB_BUCKET}/scopes"
)"

if printf '%s' "${collections_json}" | grep -q '"name":"documents"'; then
  echo "Collection documents already exists."
else
  echo "Creating collection documents..."

  curl -fsS \
    -u "${CB_ADMIN_USER}:${CB_ADMIN_PASSWORD}" \
    -X POST \
    "http://${CB_HOST}:8091/pools/default/buckets/${CB_BUCKET}/scopes/_default/collections" \
    -d "name=documents" >/dev/null
fi

echo "Creating/updating Sync Gateway user..."

couchbase-cli user-manage \
  --cluster "http://${CB_HOST}:8091" \
  --username "${CB_ADMIN_USER}" \
  --password "${CB_ADMIN_PASSWORD}" \
  --set \
  --rbac-username "${CB_SYNC_USER}" \
  --rbac-password "${CB_SYNC_PASSWORD}" \
  --auth-domain local \
  --roles "bucket_full_access[${CB_BUCKET}]"

echo "Couchbase bootstrap completed."
