#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-consumer-smoke"

cmake -S "${repo_root}/tools/consumer_smoke" -B "${build_dir}" \
  -DGUB_MODE=consumer \
  -DGUB_STRICT=ON \
  -DGUB_WARN_AS_ERROR=ON \
  -DGUB_REQUIRE_DEPS=ON
cmake --build "${build_dir}" --target gubsy_consumer_smoke -j
if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
  while IFS= read -r -d '' dll; do
    cp -f "${dll}" "${build_dir}/"
  done < <(find "${build_dir}" -type f -name '*.dll' -print0)
  PATH="${build_dir}:${PATH}" "${build_dir}/gubsy_consumer_smoke.exe"
else
  "${build_dir}/gubsy_consumer_smoke"
fi
