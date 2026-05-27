#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
dist_dir="${repo_root}/dist/gubsy-linux"

"${repo_root}/scripts/package_linux.sh"

required_files=(
    "${dist_dir}/bin/gubsy"
    "${dist_dir}/bin/gubsy-roomd"
    "${dist_dir}/lib/libSDL3.so.0"
    "${dist_dir}/lib/libSDL3_image.so.0"
    "${dist_dir}/lib/libSDL3_mixer.so.0"
    "${dist_dir}/lib/libSDL3_ttf.so.0"
    "${dist_dir}/PACKAGE_MANIFEST.txt"
    "${dist_dir}/run-gubsy.sh"
    "${dist_dir}/run-gubsy-roomd.sh"
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
grep -q "^platform=linux$" "${dist_dir}/PACKAGE_MANIFEST.txt"
grep -q "^mode=release$" "${dist_dir}/PACKAGE_MANIFEST.txt"

ldd_output="$(LD_LIBRARY_PATH="${dist_dir}/lib" ldd "${dist_dir}/bin/gubsy")"
if grep -q "not found" <<<"${ldd_output}"; then
    echo "${ldd_output}" >&2
    exit 1
fi
for lib in libSDL3.so.0 libSDL3_image.so.0 libSDL3_mixer.so.0 libSDL3_ttf.so.0; do
    if ! grep -q "${dist_dir}/lib/${lib}" <<<"${ldd_output}"; then
        echo "[verify-package] ${lib} did not resolve from ${dist_dir}/lib" >&2
        echo "${ldd_output}" >&2
        exit 1
    fi
done

"${dist_dir}/run-gubsy-roomd.sh" --help >/tmp/gubsy-roomd-package-help.txt
grep -q "Usage: gubsy-roomd" /tmp/gubsy-roomd-package-help.txt

echo "[verify-package] ${dist_dir} ok"
