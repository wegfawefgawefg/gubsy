#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
HTTP_PORT="${HTTP_PORT:-8792}"
PUNCH_PORT="${PUNCH_PORT:-8793}"
RELAY_PORT="${RELAY_PORT:-8794}"

"${BUILD_DIR}/gubsy-roomd" \
  --port="${HTTP_PORT}" \
  --punch-port="${PUNCH_PORT}" \
  --relay-port="${RELAY_PORT}" >/tmp/gubsy-room-relay-smoke.log 2>&1 &
server_pid=$!
trap 'kill "${server_pid}" 2>/dev/null || true' EXIT

for _ in $(seq 1 50); do
  if curl -fsS "http://127.0.0.1:${HTTP_PORT}/health" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

"${BUILD_DIR}/room_relay_smoke" "http://127.0.0.1:${HTTP_PORT}"
