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
  "src/include code must not include game headers" \
  '#include[[:space:]]+[<"]demo/' \
  "${repo_root}/src" \
  "${repo_root}/include"

check_no_matches \
  "consumer smoke sources must not include private src headers" \
  '#include[[:space:]]+[<"]src/' \
  "${repo_root}/tools/public_api_smoke" \
  "${repo_root}/tools/consumer_smoke"

check_no_matches \
  "public gubsy headers must not include private src headers" \
  '#include[[:space:]]+[<"]src/' \
  "${repo_root}/include/gubsy"

check_no_matches \
  "CMake must not require sibling glib checkouts" \
  '\.\./g(sexp|layout|input)' \
  "${repo_root}/CMakeLists.txt" \
  "${repo_root}/docs"

if awk '
  /target_include_directories\(gubsy_engine BEFORE/ { in_block = 1; next }
  in_block && /^[[:space:]]*PRIVATE/ { in_public = 0 }
  in_block && /^[[:space:]]*PUBLIC/ { in_public = 1; next }
  in_block && /^\)/ { in_block = 0; in_public = 0 }
  in_block && in_public && /\$\{GUB_SOURCE_DIR\}$/ { found = 1 }
  END { exit found ? 0 : 1 }
' "${repo_root}/CMakeLists.txt"; then
  printf '[boundary] gubsy_engine must not expose repo root as a public include path\n' >&2
  failed=1
fi

if [[ "${failed}" -ne 0 ]]; then
  exit 1
fi

printf '[boundary] ok\n'
