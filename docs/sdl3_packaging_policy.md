# SDL3 Dependency And Packaging Policy

Gubsy and downstream games should standardize on SDL3. Do not require players
to install SDL globally before running a game.

Goal: set up Gubsy and Splonks so development builds, library consumption, and
player-facing release packages have explicit modes, reproducible dependency
resolution, and correct SDL runtime distribution on each supported platform.

## Build Modes

Use these terms consistently:

- **Developer mode** means building Gubsy as the active project. It should build
  `gubsy::engine`, the bundled sample, local tools, smokes, and guard tests.
  This is the mode for changing Gubsy itself, often while testing a game beside
  it.
- **Consumer mode** means Gubsy is being built as a library by another
  developer's game. It should build `gubsy::engine` by default, avoid Gubsy's
  sample and tool executables unless explicitly requested, and let the top-level
  game own executable and dependency policy.
- **Release packaging** means producing a player-facing game/tool bundle. It is
  not just a CMake consumption mode: it copies or bundles SDL runtime artifacts,
  assets, configs, and platform metadata into something a player can run without
  installing development packages.

Current CMake defaults distinguish developer and consumer mode by whether Gubsy
is the top-level project. Building Gubsy directly enables local extras by
default; consuming it through another top-level project disables those extras by
default.

## Mode Matrix

| Concern | Developer Mode | Consumer Mode | Release Packaging |
| --- | --- | --- | --- |
| Primary user | Gubsy developer | Game developer | Player |
| Top-level project | Gubsy | Game that embeds Gubsy, such as Splonks | Game or tool package job |
| Main target | `gubsy::engine`, sample, tools | `gubsy::engine` | final executable/package |
| Gubsy sample/tools | on by default | off by default | package only when explicitly shipping a Gubsy tool |
| Tests/smokes | on by default | off unless requested | package verification only |
| Dependency owner | Gubsy defaults | top-level game | package preset/job |
| SDL runtime files | useful for local run convenience | not copied by Gubsy | must be bundled with the shipped executable |
| Player install requirement | none relevant | none relevant | no global SDL install |

This means consumer mode is not the same thing as release packaging. A game can
consume Gubsy in consumer mode during development and still need a separate
release packaging step to produce a player-facing bundle.

Consumer mode is a Gubsy mode, not a Splonks mode. Splonks is an application,
not a library, so Splonks only needs local developer mode and release packaging
mode unless it later grows a library target for other games.

## CMake Mode Direction

Keep the existing top-level heuristic as the default behavior, but make the
intent explicit in CMake before adding more packaging logic:

- `GUB_MODE=developer|consumer|release` should be added as an optional explicit
  override.
- `developer` should enable sample/tools/tests by default.
- `consumer` should build only `gubsy::engine` by default.
- `release` should be reserved for package-producing builds and should not be
  inferred merely because `CMAKE_BUILD_TYPE=Release`.
- Existing options such as `GUB_BUILD_SAMPLE`, `GUB_BUILD_TOOLS`,
  `GUB_ENABLE_LUA_MOD_HOST`, and `GUB_FETCH_DEPS` should remain explicit
  overrides after the mode defaults are chosen.

Splonks should get the same vocabulary on its side:

- `SPLONKS_MODE=developer|release` or equivalent presets should distinguish
  local iteration from package-producing builds.
- Splonks does not need a consumer mode while it is only an application.
- Splonks is the final game executable owner, so Splonks release packaging owns
  the SDL runtime files for the Splonks package.
- If Splonks later consumes Gubsy through the same CMake graph, Splonks should
  set Gubsy to consumer mode and keep one shared SDL target set.

## Presets And Platform Scaffolds

Use CMake presets as the visible entry points for platform builds. The local
`how-to-multi-backend-rendering` reference is SDL2-based, but its structure is a
useful model:

- desktop developer presets, such as Linux debug/release.
- platform presets, such as `android-arm64`.
- a separate Android Gradle project that owns APK/AAB packaging.
- small scripts for Android setup, native build, APK build, install, launch, and
  logcat.

Gubsy should use presets for engine/tool/sample development and package jobs.
Splonks should use presets for game developer builds and release package jobs.
The preset name should describe the intent and platform; `CMAKE_BUILD_TYPE`
alone is not enough to distinguish local release-optimized development from a
player-facing package.

## Dependency Policy

Use this dependency order for developer, consumer, and CI builds:

1. Prefer existing CMake package targets when the build environment provides
   SDL3, SDL3_ttf, SDL3_image, or SDL3_mixer.
2. Fall back to pkg-config on Unix-like developer machines.
3. If dependencies are still missing, fetch pinned SDL source releases with
   CMake `FetchContent`.

The `GUB_FETCH_DEPS` CMake option exists for this fallback path. It should stay
enabled by default for normal source builds so a clean machine can build Gubsy
without a global SDL3 install.

Release packaging and CI builds should pin exact SDL versions. Prefer source
archives with hashes, submodules, or a lockable dependency mirror over floating
branch names.

## Shipping Policy

Release packages should include the SDL runtime artifacts they need. A user
should be able to install or unzip the game and run it without installing
`libsdl3-dev`, Homebrew packages, MSYS2 packages, or system-wide DLLs.

Platform expectations:

- Windows: ship `SDL3.dll` and any SDL extension DLLs next to the game
  executable or in the installer/Steam depot.
- macOS: bundle SDL frameworks or dylibs inside the `.app` bundle and sign the
  complete app bundle.
- Android: use SDL's Android AAR/Prefab flow or build SDL as part of the
  Gradle/CMake project. The APK/AAB owns the native SDL libraries.
- Linux: package through the chosen distribution channel. Steam runtime,
  Flatpak, AppImage, or local shared libraries with an rpath are all acceptable.
  Do not assume a player's system has SDL3 development packages installed.

Package managers are fine for developer setup. They are not a player-facing
runtime requirement.

## Runtime Library Bundling

Distribution code should copy runtime libraries from the CMake targets that are
actually linked into the executable being packaged. Do not hardcode a global SDL
install path as the normal package source.

Minimum runtime set when using shared libraries:

- SDL3
- SDL3_ttf
- SDL3_image
- SDL3_mixer
- any transitive shared libraries required by those SDL extension libraries

Expected ownership:

- Gubsy packages `gubsy-roomd` only as a server tool. It does not need SDL
  runtime libraries unless the packaged target links SDL.
- Gubsy packages the bundled sample only for Gubsy developer/demo releases. That
  package must include the SDL runtime libraries it links.
- Splonks packages the Splonks game executable and owns copying SDL runtime
  libraries into the Splonks release bundle.

Platform-specific rules:

- Windows: copy required `.dll` files beside the `.exe`.
- macOS: put required `.dylib` or framework contents inside the `.app`, fix
  install names/rpaths, and sign the final bundle.
- Linux: use the chosen package format's dependency model. If shipping local
  `.so` files, put them in the bundle and set an rpath such as `$ORIGIN/lib`.
  The first Linux implementation uses `scripts/package_linux.sh` in each repo to
  create a local `dist/` bundle with SDL-related shared libraries and a wrapper
  that sets `LD_LIBRARY_PATH`.
- Android: the Gradle/APK/AAB build owns native `.so` packaging. Desktop copy
  rules do not apply.

## Android Direction

Android should be treated as a first-class platform scaffold, not as a desktop
install rule:

- Splonks should eventually own an `android/` Gradle app because Splonks is the
  final player-facing game.
- Gradle should call CMake through `externalNativeBuild`.
- The Android native target should follow SDL's expected app shape for SDL3.
  SDL2 examples often name the native target `main`; verify the SDL3 Android
  flow before copying that detail.
- The Java/Kotlin activity should use SDL3's Android glue or AAR/Prefab path.
  Do not vendor SDL2 Java glue into the SDL3 stack.
- Prefer one scripted dev loop: setup SDK/NDK, create/start emulator, build
  native code, build APK, install, launch, and stream filtered logcat.
- If the Android build statically links SDL into the app native library, the
  activity may only need to load the app library. If SDL is packaged as separate
  `.so` files, the activity/package must load and include those libraries
  explicitly.

Gubsy should not own Splonks' Android app. Gubsy may keep Android-compatible
CMake and headers, but the game package owns the APK/AAB.

## Source Tree Direction

Keep SDL2 compatibility names and local shim headers out of normal source.
Public headers and downstream consumers should see an explicitly SDL3-based
dependency.

Shared Gubsy consumers such as Splonks should not independently fetch their own
conflicting SDL copies when Gubsy is already part of the same CMake graph. The
top-level project should arrange for one SDL3 target set to be visible to all
engine and game targets.

## Current Implementation

- Gubsy has explicit `GUB_MODE=developer|consumer|release` handling. Developer
  and release modes build local extras by default; consumer mode builds only the
  engine target unless extras are requested.
- Splonks has explicit `SPLONKS_MODE=developer|release` handling. Splonks has no
  consumer mode while it remains only a game executable.
- Both repos expose Linux package presets and `scripts/package_linux.sh`.
- The Linux package scripts build release package presets, create local
  `dist/` bundles, copy SDL-related shared libraries from the linked binaries,
  and create wrapper scripts that run with the bundled library directory first
  in `LD_LIBRARY_PATH`.
- Both repos expose native macOS and Windows package presets plus
  `scripts/package_macos.sh` and `scripts/package_windows.sh`. These are
  platform-host scripts: run the macOS scripts on macOS and the Windows scripts
  on Windows through Git Bash/MSYS/MinGW/Cygwin.
- Gubsy package launchers set `GUB_PROJECT_ROOT` so packaged sample runs use the
  bundled `data/`, `demo/`, `src/assets/`, and `tools/mod_repo/` trees instead
  of the developer source checkout.
- Splonks package launchers run from the package resource root so relative
  `assets/` and `data/` paths resolve inside the package.
- Linux package validation should include an `ldd` check proving SDL resolves
  from `dist/.../lib` and a packaged executable smoke run through the wrapper.
- macOS package validation should include an `otool -L` check, an app launch
  smoke, and signing/notarization checks before real distribution.
- Windows package validation should include a clean-machine or clean-shell run
  where the `.exe` resolves SDL DLLs from the package directory.

## Open Packaging Work

- Validate and harden the macOS scripts on macOS, including full transitive
  dylib/framework copying, install-name fixups, rpaths, app launch smokes,
  signing, and notarization.
- Validate and harden the Windows scripts on Windows, including full DLL
  copying and package smoke runs from the package directory.
- Decide whether release builds prefer shared SDL libraries or static linking
  per platform.
- Replace the first Linux local-bundle script with the final Linux distribution
  channel if needed: Steam runtime, Flatpak, AppImage, distro packages, or a
  stricter local `.so` bundle with rpath/patchelf.
- Add Android Gradle scaffold and dev-loop scripts for Splonks.
- Add Android packaging once the Gradle project exists.
- Add CI jobs that prove clean source builds work without preinstalled SDL3.
- Add package smoke checks in CI that run packaged executables from the install
  tree without development-library paths.
