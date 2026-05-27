#!/usr/bin/env bash
set -euo pipefail

case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) ;;
    *)
        echo "package_windows.sh must run on Windows through Git Bash/MSYS/MinGW/Cygwin" >&2
        exit 1
        ;;
esac

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-package-windows"
dist_dir="${repo_root}/dist/gubsy-windows"
source "${repo_root}/scripts/package_runtime_libs.sh"

GUB_PRESET=package-windows "${repo_root}/scripts/build.sh"

rm -rf "${dist_dir}"
mkdir -p "${dist_dir}"

cp "${build_dir}/gubsy.exe" "${dist_dir}/"
cp "${build_dir}/gubsy-roomd.exe" "${dist_dir}/"
cp -a "${repo_root}/data" "${dist_dir}/data"
mkdir -p "${dist_dir}/src"
cp -a "${repo_root}/src/assets" "${dist_dir}/src/assets"
cp -a "${repo_root}/demo" "${dist_dir}/demo"
mkdir -p "${dist_dir}/tools"
cp -a "${repo_root}/tools/mod_repo" "${dist_dir}/tools/mod_repo"

package_copy_runtime_libs_from_tree "${build_dir}" "${dist_dir}" ".dll"

cat > "${dist_dir}/run-gubsy.bat" <<'EOF'
@echo off
set GUB_PROJECT_ROOT=%~dp0
"%~dp0gubsy.exe" %*
EOF

cat > "${dist_dir}/run-gubsy-roomd.bat" <<'EOF'
@echo off
set GUB_PROJECT_ROOT=%~dp0
"%~dp0gubsy-roomd.exe" %*
EOF

echo "[package] ${dist_dir}"
