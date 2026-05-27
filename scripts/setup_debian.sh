#!/usr/bin/env bash
set -euo pipefail

echo "[artificial] Updating apt and installing build + runtime development deps..."
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkg-config git gdb \
  liblua5.4-dev \
  wayland-protocols \
  libwayland-dev \
  libxkbcommon-dev \
  libx11-dev \
  libxext-dev \
  libxcursor-dev \
  libxi-dev \
  libxfixes-dev \
  libxrandr-dev \
  libxrender-dev \
  libxss-dev \
  libasound2-dev \
  libpulse-dev \
  libpipewire-0.3-dev \
  libdecor-0-dev \
  libdrm-dev \
  libgbm-dev \
  libudev-dev

echo "[artificial] Installed gubsy Debian/Ubuntu dependencies."
