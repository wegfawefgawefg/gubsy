#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
scope="${1:-all}"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
validation_dir="${repo_root}/dist/validation"

usage() {
    cat >&2 <<EOF
Usage: $0 [dev|consumer|room|lobby|package|all]

Runs local Gubsy validation and writes a timestamped evidence log. This is the
normal feedback path for Gubsy changes; GitHub package jobs are manual remote
confidence checks only.
EOF
}

host_platform() {
    case "${OS:-}:$(uname -s)" in
        Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) printf 'windows' ;;
        *:Darwin) printf 'macos' ;;
        *:Linux) printf 'linux' ;;
        *)
            echo "Unsupported host platform: ${OS:-}:$(uname -s)" >&2
            exit 1
            ;;
    esac
}

run_step() {
    echo
    echo "[validate] $*"
    "$@"
}

print_first_line() {
    local label="$1"
    shift
    local cmd="$1"
    local output
    local first_line
    if command -v "${cmd}" >/dev/null 2>&1; then
        output="$("$@" 2>&1 || true)"
        IFS= read -r first_line <<< "${output}"
        echo "${label}=${first_line}"
    fi
}

write_environment() {
    local platform="$1"
    echo "[environment]"
    echo "platform=${platform}"
    echo "scope=${scope}"
    echo "timestamp_utc=${timestamp}"
    echo "git_revision=$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
    echo "uname=$(uname -a)"
    echo "msystem=${MSYSTEM:-}"
    echo "path=${PATH}"
    print_first_line cmake_version cmake --version
    print_first_line ninja_version ninja --version
    print_first_line pkg_config_version pkg-config --version
    print_first_line git_version git --version
    print_first_line cc_version cc --version
    print_first_line cxx_version c++ --version
    print_first_line gcc_version gcc --version
    print_first_line gxx_version g++ --version
    print_first_line clang_version clang --version
    print_first_line clangxx_version clang++ --version
    print_first_line brew_version brew --version
    print_first_line pacman_version pacman --version
}

validate_dev() {
    run_step "${repo_root}/scripts/build.sh"
    run_step ctest --test-dir "${repo_root}/build" --output-on-failure
    echo "[validated] developer build and tests"
}

validate_consumer() {
    run_step "${repo_root}/scripts/consumer_smoke.sh"
    echo "[validated] external consumer smoke"
}

validate_room() {
    run_step "${repo_root}/scripts/room_smoke.sh"
    echo "[validated] room server smoke"
}

validate_lobby() {
    run_step "${repo_root}/scripts/lobby_online_smoke.sh"
    echo "[validated] lobby online smoke"
}

validate_package() {
    local platform="$1"
    case "${platform}" in
        linux)
            run_step "${repo_root}/scripts/verify_package_linux.sh"
            ;;
        macos)
            run_step "${repo_root}/scripts/verify_package_macos.sh"
            ;;
        windows)
            run_step "${repo_root}/scripts/verify_package_windows.sh"
            ;;
        *)
            echo "No package verifier for platform: ${platform}" >&2
            exit 1
            ;;
    esac
    echo "[validated] ${platform} developer/tooling package"
}

case "${scope}" in
    dev|consumer|room|lobby|package|all) ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac

platform="$(host_platform)"
mkdir -p "${validation_dir}"
log_path="${validation_dir}/${platform}-${scope}-${timestamp}.log"

(
    cd "${repo_root}"
    write_environment "${platform}"
    case "${scope}" in
        dev)
            validate_dev
            ;;
        consumer)
            validate_consumer
            ;;
        room)
            validate_room
            ;;
        lobby)
            validate_lobby
            ;;
        package)
            validate_package "${platform}"
            ;;
        all)
            validate_dev
            validate_consumer
            validate_room
            validate_lobby
            validate_package "${platform}"
            ;;
    esac
    echo
    echo "[validate] evidence log: ${log_path}"
) 2>&1 | tee "${log_path}"

echo "[validate] wrote ${log_path}"
