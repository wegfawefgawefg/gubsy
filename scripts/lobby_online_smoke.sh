#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build"
server_port="${ROOM_SERVER_PORT:-8789}"
server_url="http://127.0.0.1:${server_port}"

"${build_dir}/room_server" "--port=${server_port}" >/tmp/gubsy_lobby_online_room_server.log 2>&1 &
server_pid=$!

cleanup() {
    kill "${server_pid}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 0.5
"${build_dir}/lobby_online_smoke" "${server_url}"
