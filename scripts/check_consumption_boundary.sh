#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

failed=0

check_no_matches() {
  local label="$1"
  local pattern="$2"
  shift 2

  local matches
  matches="$(grep -R -n -E "${pattern}" "$@" || true)"
  if [[ -n "${matches}" ]]; then
    printf '[boundary] %s\n%s\n' "${label}" "${matches}" >&2
    failed=1
  fi
}

check_no_matches \
  "engine/include code must not include game headers" \
  '#include[[:space:]]+[<"]game/' \
  "${repo_root}/engine" \
  "${repo_root}/include"

check_no_matches \
  "consumer smoke sources must not include private engine headers" \
  '#include[[:space:]]+[<"]engine/' \
  "${repo_root}/tools/public_api_smoke" \
  "${repo_root}/tools/consumer_smoke"

check_no_matches \
  "CMake must not require sibling glib checkouts" \
  '\.\./g(sexp|layout|input)' \
  "${repo_root}/CMakeLists.txt" \
  "${repo_root}/docs"

if [[ "${failed}" -ne 0 ]]; then
  exit 1
fi

printf '[boundary] ok\n'
