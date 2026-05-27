Developer Setup
===============

Requirements
------------

- CMake 3.20+
- C++20 compiler
- Lua 5.4 dev headers for Gubsy developer/sample builds
- Native window/audio development headers for the platform
- Optional system SDL3, SDL3_image, SDL3_ttf, and SDL3_mixer packages

Gubsy standardizes on SDL3. By default, the build prefers system SDL3 CMake or
pkg-config packages when present, then falls back to pinned SDL3 source builds
through CMake `FetchContent`.

Gubsy is code-first library/tooling infrastructure. The shipped game release
pipeline belongs to downstream games such as Splonks. Use Gubsy package scripts
only for developer/demo/server-tool artifacts or explicit remote confidence
checks.

For a recorded local validation pass, run:

```
bash scripts/validate_local.sh all
bash scripts/validation_status.sh
```

This writes a timestamped log under `dist/validation/` and covers the normal
Gubsy code-first gates: developer build/tests, external consumer smoke, room
server smoke, lobby smoke, and the local developer/tooling package verifier.
The log records the host platform, git revision, PATH, MSYS2 environment when
present, CMake, Ninja, pkg-config, Git, compiler, Homebrew, and pacman versions
when those tools are available.
The status helper is the local audit gate: it checks that the latest full local
validation log matches the current commit, that the SDL3 shim cleanup and
consumer boundary guards still pass, and that the package workflow remains
manual-only.

Linux (Debian/Ubuntu)
---------------------

```
bash scripts/setup_debian.sh
bash scripts/build.sh
./build/gubsy          # alias: ./build/arti
```

Windows (MSYS2 UCRT64)
----------------------

The supported local package-verification path is MSYS2 UCRT64, matching
`.github/workflows/package.yml` and `scripts/package_windows.sh`.
Use the **MSYS2 UCRT64** terminal, not PowerShell or cmd.exe:

```
pacman -Syu
```

If MSYS2 asks you to close the terminal, reopen **MSYS2 UCRT64**, then install
the build tools:

```
pacman -S --needed --noconfirm \
  git \
  unzip \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-pkgconf
```

Build and run:

```
bash scripts/build.sh
./build/gubsy.exe
```

Validate the Windows developer/tooling package:

```
bash scripts/validate_local.sh package
```

vcpkg can still be used for experimental local dependency discovery, but it is
not the documented Windows package-verification path yet.

macOS (Homebrew)
----------------

```
brew install cmake ninja pkg-config lua
# Optional if you want system SDL3 instead of FetchContent:
brew install sdl3 sdl3_image sdl3_ttf sdl3_mixer
bash scripts/build.sh
./build/gubsy
```

CMake Notes
-----------

- Presets: `CMakePresets.json` provides a `dev` preset (Debug, strict warnings).
- Dependency discovery prefers CMake packages and falls back to pkg‑config when available.
- `GUB_FETCH_DEPS=ON` is the default fallback for missing SDL3 dependencies.
- Options (kept for backward compatibility in the codebase):
  - `GUB_REQUIRE_DEPS` (ON by default): fail configure if deps are missing.
  - `GUB_STRICT` and `GUB_WARN_AS_ERROR`: enable strict warnings and treat warnings as errors.

Troubleshooting
---------------

- If CMake can’t find a package, set package dirs explicitly, e.g.:
  - `-DSDL3_DIR=/path/to/SDL3/lib/cmake/SDL3`
  - `-DSDL3_image_DIR=/path/to/SDL3_image/lib/cmake/SDL3_image`
- Run script prefers X11 on i3 unless `SDL_VIDEODRIVER` is set.
