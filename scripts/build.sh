#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build"
mode="${1:-build}"

configure() {
    cmake --preset dev
}

if ! configure; then
    # Recover when the repo was moved and the cached source/build paths no longer match.
    rm -f "${build_dir}/CMakeCache.txt"
    rm -rf "${build_dir}/CMakeFiles"
    configure
fi

if [ "${mode}" = "--configure-only" ]; then
    exit 0
fi

cmake --build --preset dev -j
