#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${GUBSY_BUILDDIR:-${repo_root}/build}"
binary="${build_dir}/gubsy-roomd"
service_src="${repo_root}/deploy/gubsy-roomd.service"
env_src="${repo_root}/deploy/gubsy-roomd.env.example"

if [ ! -x "${binary}" ]; then
  echo "missing ${binary}; build Gubsy with GUB_BUILD_TOOLS=ON first" >&2
  exit 1
fi

sudo install -d -m 0755 /usr/local/bin
sudo install -m 0755 "${binary}" /usr/local/bin/gubsy-roomd

if ! id -u gubsy-roomd >/dev/null 2>&1; then
  sudo useradd --system --home-dir /nonexistent --shell /usr/sbin/nologin gubsy-roomd
fi

if [ ! -f /etc/gubsy-roomd.env ]; then
  sudo install -m 0644 "${env_src}" /etc/gubsy-roomd.env
fi

sudo install -m 0644 "${service_src}" /etc/systemd/system/gubsy-roomd.service
sudo systemctl daemon-reload
sudo systemctl enable gubsy-roomd.service

echo "installed gubsy-roomd"
echo "edit /etc/gubsy-roomd.env if needed, then run:"
echo "  sudo systemctl restart gubsy-roomd"
echo "  curl http://127.0.0.1:8788/health"
