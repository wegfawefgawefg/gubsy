#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
HTTP_PORT="${GUB_ROOM_PUNCH_SMOKE_HTTP_PORT:-19788}"
UDP_PORT="${GUB_ROOM_PUNCH_SMOKE_UDP_PORT:-19789}"

"${BUILD_DIR}/gubsy-roomd" \
  --host=127.0.0.1 \
  --port="${HTTP_PORT}" \
  --rendezvous-port="${UDP_PORT}" >/tmp/gubsy-room-punch-smoke.log 2>&1 &
ROOMD_PID=$!

cleanup() {
  kill "${ROOMD_PID}" >/dev/null 2>&1 || true
  wait "${ROOMD_PID}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in {1..50}; do
  if curl -fsS "http://127.0.0.1:${HTTP_PORT}/health" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

"${BUILD_DIR}/room_punch_smoke" "http://127.0.0.1:${HTTP_PORT}"
