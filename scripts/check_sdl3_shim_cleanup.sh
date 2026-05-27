#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

failed=0

check_missing_path() {
  local path="$1"
  if [[ -e "${repo_root}/${path}" ]]; then
    printf '[sdl3-shim] deleted shim path returned: %s\n' "${path}" >&2
    failed=1
  fi
}

check_no_matches() {
  local label="$1"
  local pattern="$2"
  shift 2

  local matches
  matches="$(grep -R -n -E "${pattern}" "$@" || true)"
  if [[ -n "${matches}" ]]; then
    printf '[sdl3-shim] %s\n%s\n' "${label}" "${matches}" >&2
    failed=1
  fi
}

check_missing_path "SDL.h"
check_missing_path "SDL_mixer.h"
check_missing_path "SDL2"
check_missing_path "include/SDL.h"
check_missing_path "include/SDL2"

check_no_matches \
  "Gubsy code must use direct SDL3 includes and SDL3 API names" \
  '#include[[:space:]]+<SDL2|#include[[:space:]]+<SDL_mixer|#include[[:space:]]+<SDL\.h|SDL_ENABLE_OLD_NAMES|SDL_CONTROLLER|GameController|GAMECONTROLLER|SDL_RenderCopy|SDL_RenderDrawRectF|SDL_RenderFillRectF|SDL_RenderSetClipRect|SDL_GetRendererOutputSize|SDL_QueryTexture' \
  "${repo_root}/src" \
  "${repo_root}/include" \
  "${repo_root}/demo" \
  "${repo_root}/tools"

check_no_matches \
  "legacy sdl_shim naming must not return" \
  'sdl_shim' \
  "${repo_root}/src" \
  "${repo_root}/include" \
  "${repo_root}/demo" \
  "${repo_root}/tools" \
  "${repo_root}/CMakeLists.txt"

if [[ "${failed}" -ne 0 ]]; then
  exit 1
fi

printf '[sdl3-shim] ok\n'
