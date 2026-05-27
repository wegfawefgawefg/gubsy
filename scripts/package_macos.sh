#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "package_macos.sh must run on macOS" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-package-macos"
dist_dir="${repo_root}/dist/gubsy-macos"
app_dir="${dist_dir}/Gubsy.app"
contents_dir="${app_dir}/Contents"
macos_dir="${contents_dir}/MacOS"
resources_dir="${contents_dir}/Resources"
frameworks_dir="${contents_dir}/Frameworks"
roomd_dir="${dist_dir}/gubsy-roomd"

GUB_PRESET=package-macos "${repo_root}/scripts/build.sh"

rm -rf "${dist_dir}"
mkdir -p "${macos_dir}" "${resources_dir}" "${frameworks_dir}" "${roomd_dir}/bin" "${roomd_dir}/lib"

cp "${build_dir}/gubsy" "${macos_dir}/gubsy-bin"
cp "${build_dir}/gubsy-roomd" "${roomd_dir}/bin/gubsy-roomd"
cp -a "${repo_root}/data" "${resources_dir}/data"
mkdir -p "${resources_dir}/src"
cp -a "${repo_root}/src/assets" "${resources_dir}/src/assets"
cp -a "${repo_root}/demo" "${resources_dir}/demo"
mkdir -p "${resources_dir}/tools"
cp -a "${repo_root}/tools/mod_repo" "${resources_dir}/tools/mod_repo"

cat > "${macos_dir}/Gubsy" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
export GUB_PROJECT_ROOT="${root}/Resources"
export DYLD_LIBRARY_PATH="${root}/Frameworks${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
exec "${root}/MacOS/gubsy-bin" "$@"
EOF
chmod +x "${macos_dir}/Gubsy"

cat > "${contents_dir}/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key>
  <string>Gubsy</string>
  <key>CFBundleIdentifier</key>
  <string>dev.gubsy.sample</string>
  <key>CFBundleName</key>
  <string>Gubsy</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleVersion</key>
  <string>0.1.0</string>
  <key>CFBundleShortVersionString</key>
  <string>0.1.0</string>
</dict>
</plist>
EOF

copy_dylibs() {
    local exe="$1"
    local dst="$2"
    otool -L "${exe}" \
        | awk 'NR > 1 {print $1}' \
        | while read -r dep; do
            case "${dep}" in
                *SDL3*.dylib|*png*.dylib|*freetype*.dylib|*harfbuzz*.dylib|*pluto*.dylib|*vorbis*.dylib|*ogg*.dylib|*zstd*.dylib|*brotli*.dylib|*bz2*.dylib|*jpeg*.dylib|*webp*.dylib)
                    if [[ -f "${dep}" ]]; then
                        cp -f "${dep}" "${dst}/"
                    fi
                    ;;
            esac
        done
}

copy_dylibs "${macos_dir}/gubsy-bin" "${frameworks_dir}"
copy_dylibs "${roomd_dir}/bin/gubsy-roomd" "${roomd_dir}/lib"

find "${frameworks_dir}" "${roomd_dir}/lib" -type f -name "*.dylib" -exec chmod u+w {} +
while IFS= read -r dylib; do
    install_name_tool -id "@rpath/$(basename "${dylib}")" "${dylib}" 2>/dev/null || true
done < <(find "${frameworks_dir}" -type f -name "*.dylib")
install_name_tool -add_rpath "@executable_path/../Frameworks" "${macos_dir}/gubsy-bin" 2>/dev/null || true
install_name_tool -add_rpath "@executable_path/../lib" "${roomd_dir}/bin/gubsy-roomd" 2>/dev/null || true

rewrite_dylib_refs() {
    local target="$1"
    local dylib_dir="$2"
    otool -L "${target}" \
        | awk 'NR > 1 {print $1}' \
        | while read -r dep; do
            local local_dep="${dylib_dir}/$(basename "${dep}")"
            if [[ -f "${local_dep}" ]]; then
                install_name_tool -change "${dep}" "@rpath/$(basename "${dep}")" "${target}" 2>/dev/null || true
            fi
        done
}

rewrite_dylib_refs "${macos_dir}/gubsy-bin" "${frameworks_dir}"
while IFS= read -r dylib; do
    rewrite_dylib_refs "${dylib}" "${frameworks_dir}"
done < <(find "${frameworks_dir}" -type f -name "*.dylib")
rewrite_dylib_refs "${roomd_dir}/bin/gubsy-roomd" "${roomd_dir}/lib"
while IFS= read -r dylib; do
    rewrite_dylib_refs "${dylib}" "${roomd_dir}/lib"
done < <(find "${roomd_dir}/lib" -type f -name "*.dylib")

cat > "${roomd_dir}/run-gubsy-roomd.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export GUB_PROJECT_ROOT="${root}/.."
export DYLD_LIBRARY_PATH="${root}/lib${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
exec "${root}/bin/gubsy-roomd" "$@"
EOF
chmod +x "${roomd_dir}/run-gubsy-roomd.sh"

codesign --force --deep --sign - "${app_dir}" 2>/dev/null || true

echo "[package] ${dist_dir}"
