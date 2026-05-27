#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "verify_package_macos.sh must run on macOS" >&2
    exit 1
fi

if [[ "$(uname -m)" != "arm64" ]]; then
    echo "verify_package_macos.sh must run on an Apple Silicon Mac because the package is arm64-only." >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
dist_dir="${repo_root}/dist/gubsy-macos"
app_dir="${dist_dir}/Gubsy.app"
frameworks_dir="${app_dir}/Contents/Frameworks"
roomd_dir="${dist_dir}/gubsy-roomd"

"${repo_root}/scripts/package_macos.sh"

required_files=(
    "${app_dir}/Contents/MacOS/Gubsy"
    "${app_dir}/Contents/MacOS/gubsy-bin"
    "${app_dir}/Contents/Info.plist"
    "${app_dir}/Contents/Resources/data/settings_profiles/top_level_game_settings.lisp"
    "${app_dir}/Contents/Resources/src/assets/fonts"
    "${app_dir}/Contents/Resources/demo/main.cpp"
    "${app_dir}/Contents/Resources/tools/mod_repo"
    "${dist_dir}/PACKAGE_MANIFEST.txt"
    "${roomd_dir}/bin/gubsy-roomd"
    "${roomd_dir}/run-gubsy-roomd.sh"
)

for path in "${required_files[@]}"; do
    if [[ ! -e "${path}" ]]; then
        echo "[verify-package] missing ${path}" >&2
        exit 1
    fi
done

grep -q "^app=gubsy$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^platform=macos$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^mode=release$" "${dist_dir}/PACKAGE_MANIFEST.txt"

require_dylib() {
    local label="$1"
    local pattern="$2"
    if ! find "${frameworks_dir}" -type f -name "${pattern}" | grep -q .; then
        echo "[verify-package] missing bundled ${label} dylib in ${frameworks_dir}" >&2
        exit 1
    fi
}

require_dylib "SDL3" "*SDL3*.dylib"
require_dylib "SDL3_image" "*SDL3_image*.dylib"
require_dylib "SDL3_mixer" "*SDL3_mixer*.dylib"
require_dylib "SDL3_ttf" "*SDL3_ttf*.dylib"

archs="$(lipo -archs "${app_dir}/Contents/MacOS/gubsy-bin")"
if [[ " ${archs} " != *" arm64 "* ]]; then
    echo "[verify-package] gubsy-bin is missing arm64 slice; archs=${archs}" >&2
    exit 1
fi
if [[ " ${archs} " == *" x86_64 "* ]]; then
    echo "[verify-package] gubsy-bin should be arm64-only but includes x86_64; archs=${archs}" >&2
    exit 1
fi

otool -L "${app_dir}/Contents/MacOS/gubsy-bin"
"${roomd_dir}/run-gubsy-roomd.sh" --help >/tmp/gubsy-roomd-macos-package-help.txt
grep -q "Usage: gubsy-roomd" /tmp/gubsy-roomd-macos-package-help.txt

echo "[verify-package] ${dist_dir} ok"
