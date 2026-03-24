#!/usr/bin/env bash
set -euo pipefail

echo "[artificial] Updating apt and installing build + runtime development deps..."
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config git gdb \
  libglm-dev \
  liblua5.4-dev \
  libsdl2-dev \
  libsdl2-image-dev \
  libsdl2-ttf-dev \
  libsdl2-mixer-dev

echo "[artificial] Installed gubsy Debian/Ubuntu dependencies."
