#!/usr/bin/env bash
set -euo pipefail

case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) ;;
    *)
        echo "verify_package_windows.sh must run in the MSYS2 UCRT64 terminal on Windows" >&2
        exit 1
        ;;
esac

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo "verify_package_windows.sh must run in the MSYS2 UCRT64 terminal. Current MSYSTEM=${MSYSTEM:-unset}" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
dist_dir="${repo_root}/dist/gubsy-windows"

"${repo_root}/scripts/package_windows.sh"

required_files=(
    "${dist_dir}/gubsy.exe"
    "${dist_dir}/gubsy-roomd.exe"
    "${dist_dir}/PACKAGE_MANIFEST.txt"
    "${dist_dir}/run-gubsy.bat"
    "${dist_dir}/run-gubsy-roomd.bat"
    "${dist_dir}/data/settings_profiles/top_level_game_settings.lisp"
    "${dist_dir}/src/assets/fonts"
    "${dist_dir}/demo/main.cpp"
    "${dist_dir}/tools/mod_repo"
)

for path in "${required_files[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "[verify-package] missing ${path}" >&2
        exit 1
    fi
done

grep -q "^app=gubsy$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^platform=windows$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^mode=release$" "${dist_dir}/PACKAGE_MANIFEST.txt"

require_dll() {
    local label="$1"
    local pattern="$2"
    if ! find "${dist_dir}" -maxdepth 1 -type f -iname "${pattern}" | grep -q .; then
        echo "[verify-package] missing ${label} DLL in ${dist_dir}" >&2
        exit 1
    fi
}

require_dll "SDL3" "*SDL3.dll"
require_dll "SDL3_image" "*SDL3_image*.dll"
require_dll "SDL3_mixer" "*SDL3_mixer*.dll"
require_dll "SDL3_ttf" "*SDL3_ttf*.dll"

PATH="${dist_dir}:${PATH}" "${dist_dir}/gubsy-roomd.exe" --help >/tmp/gubsy-roomd-windows-package-help.txt
grep -q "Usage: gubsy-roomd" /tmp/gubsy-roomd-windows-package-help.txt

echo "[verify-package] ${dist_dir} ok"
