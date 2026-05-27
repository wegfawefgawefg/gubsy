#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-package-linux"
dist_dir="${repo_root}/dist/gubsy-linux"

GUB_PRESET=package-linux "${repo_root}/scripts/build.sh"

rm -rf "${dist_dir}"
mkdir -p "${dist_dir}/bin" "${dist_dir}/lib"

copy_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -e "${src}" ]]; then
        cp -a "${src}" "${dst}"
    fi
}

copy_if_exists "${build_dir}/gubsy" "${dist_dir}/bin/"
copy_if_exists "${build_dir}/gubsy-roomd" "${dist_dir}/bin/"
copy_if_exists "${repo_root}/data" "${dist_dir}/"
mkdir -p "${dist_dir}/src"
copy_if_exists "${repo_root}/src/assets" "${dist_dir}/src/"
copy_if_exists "${repo_root}/demo" "${dist_dir}/"
mkdir -p "${dist_dir}/tools"
copy_if_exists "${repo_root}/tools/mod_repo" "${dist_dir}/tools/"

copy_runtime_deps() {
    local exe="$1"
    if [[ ! -x "${exe}" ]]; then
        return 0
    fi

    ldd "${exe}" \
        | awk '/=>/ {print $(NF - 1)}' \
        | while read -r dep; do
            [[ -f "${dep}" ]] || continue
            case "$(basename "${dep}")" in
                libSDL3*|libpng*|libfreetype*|libharfbuzz*|libpluto*|libvorbis*|libogg*|libzstd*|libbrotli*|libbz2*|libjpeg*|libwebp*)
                    cp -u "${dep}" "${dist_dir}/lib/"
                    ;;
            esac
        done
}

copy_runtime_deps "${dist_dir}/bin/gubsy"
copy_runtime_deps "${dist_dir}/bin/gubsy-roomd"

cat > "${dist_dir}/run-gubsy.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export GUB_PROJECT_ROOT="${root}"
export LD_LIBRARY_PATH="${root}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${root}/bin/gubsy" "$@"
EOF
chmod +x "${dist_dir}/run-gubsy.sh"

cat > "${dist_dir}/run-gubsy-roomd.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export GUB_PROJECT_ROOT="${root}"
export LD_LIBRARY_PATH="${root}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
exec "${root}/bin/gubsy-roomd" "$@"
EOF
chmod +x "${dist_dir}/run-gubsy-roomd.sh"

echo "[package] ${dist_dir}"
