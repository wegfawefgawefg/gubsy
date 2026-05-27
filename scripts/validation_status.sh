#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
strict=0

usage() {
    cat >&2 <<EOF
Usage: $0 [--strict]

Prints the current local evidence status for Gubsy's code-first validation
policy. With --strict, exits nonzero until the local validation evidence and
policy checks are current.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict)
            strict=1
            ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
    shift
done

has_glob() {
    compgen -G "$1" >/dev/null
}

latest_glob() {
    local pattern="$1"
    if has_glob "${pattern}"; then
        ls -t ${pattern} 2>/dev/null | head -n 1
    fi
}

log_has() {
    local path="$1"
    local needle="$2"
    grep -Fq "${needle}" "${path}"
}

failures=0

check_latest_log() {
    local label="$1"
    local pattern="$2"
    local expected_revision="$3"
    shift 3

    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        failures=$((failures + 1))
        return
    fi

    local actual_revision
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${latest}" | tail -n 1)"
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        failures=$((failures + 1))
        return
    fi

    local needle
    for needle in "$@"; do
        if ! log_has "${latest}" "${needle}"; then
            printf '[missing] %s: %s is missing "%s"\n' \
                "${label}" \
                "${latest#${repo_root}/}" \
                "${needle}"
            failures=$((failures + 1))
            return
        fi
    done

    printf '[ok]      %s: %s\n' "${label}" "${latest#${repo_root}/}"
}

check_command() {
    local label="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        printf '[ok]      %s\n' "${label}"
    else
        printf '[missing] %s\n' "${label}"
        failures=$((failures + 1))
    fi
}

check_file() {
    local label="$1"
    local path="$2"
    if [[ -e "${path}" ]]; then
        printf '[ok]      %s: %s\n' "${label}" "${path#${repo_root}/}"
    else
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        failures=$((failures + 1))
    fi
}

cd "${repo_root}"
current_revision="$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"

echo "Gubsy validation status"
echo "git_revision=${current_revision}"
echo

echo "[local]"
check_latest_log \
    "Full local validation" \
    "${repo_root}/dist/validation/*-all-*.log" \
    "${current_revision}" \
    "[validated] developer build and tests" \
    "[validated] external consumer smoke" \
    "[validated] room server smoke" \
    "[validated] lobby online smoke" \
    "developer/tooling package"
check_file "Linux/tool package manifest" "${repo_root}/dist/gubsy-linux/PACKAGE_MANIFEST.txt"
echo

echo "[boundaries]"
check_command "SDL3 shim cleanup guard" "${repo_root}/scripts/check_sdl3_shim_cleanup.sh"
check_command "Consumer boundary guard" "${repo_root}/scripts/check_consumption_boundary.sh"
echo

echo "[ci]"
if grep -Eq '^  pull_request:|branches:' .github/workflows/package.yml; then
    echo "[missing] Package workflow appears to include PR or branch triggers"
    failures=$((failures + 1))
else
    echo "[ok]      Package workflow has no PR or branch trigger patterns"
fi
if grep -Eq '^  workflow_dispatch:' .github/workflows/package.yml; then
    echo "[ok]      Package workflow is manual-only"
else
    echo "[missing] Package workflow manual trigger evidence"
    failures=$((failures + 1))
fi
echo

if [[ "${failures}" -eq 0 ]]; then
    echo "[status] complete local evidence present"
else
    echo "[status] ${failures} missing evidence item(s)"
fi

if [[ "${strict}" -eq 1 && "${failures}" -ne 0 ]]; then
    exit 1
fi
