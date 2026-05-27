#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
mode="${1:-build}"
preset="${GUB_PRESET:-dev}"
case "${preset}" in
    dev) build_dir="${repo_root}/build" ;;
    consumer) build_dir="${repo_root}/build-consumer" ;;
    package-linux) build_dir="${repo_root}/build-package-linux" ;;
    *) build_dir="${repo_root}/build/${preset}" ;;
esac

configure() {
    cmake --preset "${preset}"
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

cmake --build --preset "${preset}" -j
