#!/usr/bin/env bash
set -euo pipefail

# Prefer X11 on i3 unless user overrides SDL_VIDEODRIVER
if [ -n "${DISPLAY:-}" ] && [ -z "${SDL_VIDEODRIVER:-}" ]; then
  export SDL_VIDEODRIVER=x11
fi

"$(dirname "$0")"/build.sh
"$(dirname "$0")"/../build/gubsy
