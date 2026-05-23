#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-consumer-smoke"

cmake -S "${repo_root}/tools/consumer_smoke" -B "${build_dir}" \
  -DGUB_STRICT=ON \
  -DGUB_WARN_AS_ERROR=ON \
  -DGUB_REQUIRE_DEPS=ON
cmake --build "${build_dir}" --target gubsy_consumer_smoke -j
"${build_dir}/gubsy_consumer_smoke"
