# SDL3 Dependency And Packaging Policy

Gubsy and downstream games should standardize on SDL3. Do not require players
to install SDL globally before running a game.

Goal: set up Gubsy and Splonks for practical SDL3-based development and
distribution without default CI waits. Gubsy is a code-first library/tooling
dependency with local build/test/consumer validation and manual remote checks
only. Splonks is the shipped game: it owns developer/release modes, desktop SDL
runtime bundling, manual/tag-based release packaging, Android runtime
validation and release signing/AAB, an iOS simulator scaffold based on the
proven local `how-to-multi-backend-rendering` reference, iOS
signing/provisioning/TestFlight work, and clean per-platform dev setup for
Linux, macOS, Windows, Android, and iOS.

The immediate onboarding bar is simple: a new Splonks developer on Linux,
macOS, or Windows should be able to clone the repo, follow one platform-specific
setup section, run one build command, and launch the game locally without
understanding Gubsy internals or waiting on GitHub Actions.

## Developer Onboarding Bar

Before treating the distribution cleanup as complete, Splonks needs a clean
developer path for each supported development platform:

- Linux: install documented native build packages, run the standard build
  script or CMake preset, and launch Splonks from the local checkout.
- macOS: install Xcode command line tools and documented Homebrew basics, run
  the standard build script or CMake preset, and launch Splonks from the local
  checkout.
- Windows: install the documented supported toolchain, currently the MSYS2/UCRT
  path unless we add a Visual Studio path, run the standard build script or
  CMake preset, and launch Splonks from the local checkout.
- Android: install JDK/SDK/NDK prerequisites, run the Android setup/build
  scripts, install the APK, and run a smoke launch on an emulator or device.
- iOS: use macOS/Xcode, mirror the proven `how-to-multi-backend-rendering`
  `ios-sim` scaffold, build the simulator app, and document the later signing
  and device/TestFlight requirements.

Gubsy developer setup is secondary to this onboarding goal. Splonks developers
should consume Gubsy through the documented CMake/dependency path and should not
have to manually package Gubsy during normal game development.

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
- iOS should follow the local `how-to-multi-backend-rendering` reference's
  working simulator shape: a CMake preset that uses the Xcode generator,
  `CMAKE_SYSTEM_NAME=iOS`, an iPhone simulator sysroot, arm64 simulator arch,
  bundled SDL, and `MACOSX_BUNDLE` app metadata. Splonks then owns signing,
  provisioning, launch scripting, and TestFlight/App Store packaging.

Gubsy should use presets for engine/tool/sample development and package jobs.
Splonks should use presets for game developer builds and release package jobs.
The preset name should describe the intent and platform; `CMAKE_BUILD_TYPE`
alone is not enough to distinguish local release-optimized development from a
player-facing package.

## Dependency Policy

Resolve SDL as one complete stack, not one library at a time. A build may use:

1. Parent-provided CMake targets for SDL3, SDL3_ttf, SDL3_image, and SDL3_mixer.
2. System/package-manager SDL3, SDL3_ttf, SDL3_image, and SDL3_mixer.
3. Pinned fetched SDL3, SDL3_ttf, SDL3_image, and SDL3_mixer source releases.

Do not mix a fetched SDL3 core with system SDL3 add-ons, or system SDL3 core
with fetched SDL3 add-ons. Gubsy exposes `GUB_SDL_DEPS=auto|system|fetch`;
Splonks exposes `SPLONKS_SDL_DEPS=auto|system|fetch`. The normal presets use
`fetch` for reproducible clean-clone builds. `system` is available for local
package-manager development, but it must find the full SDL stack or fail.

The `GUB_FETCH_DEPS` and `SPLONKS_FETCH_DEPS` CMake options gate the pinned
FetchContent path. They should stay enabled by default for normal source builds
so a clean machine can build without a global SDL3 install.

Release packaging and CI builds should pin exact dependency revisions. Prefer
source archives with hashes, submodules, lockable dependency mirrors, or exact
Git commit SHAs over floating branch or tag names.

## Shipping Policy

Release packages should include the SDL runtime artifacts they need. A user
should be able to install or unzip the game and run it without installing
`libsdl3-dev`, Homebrew packages, MSYS2 packages, or system-wide DLLs.

Platform expectations:

- Windows: ship `SDL3.dll` and any SDL extension DLLs next to the game
  executable or in the installer/Steam depot.
- macOS: bundle SDL frameworks or dylibs inside the `.app` bundle and sign the
  complete app bundle. Default macOS release/package validation is Apple
  Silicon arm64-only; universal or Intel packages are an explicit exception,
  not the normal path.
- Android: use SDL's Android AAR/Prefab flow or build SDL as part of the
  Gradle/CMake project. The APK/AAB owns the native SDL libraries.
- iOS: build SDL and the game into the Xcode app target. The `.app` bundle owns
  native libraries, assets, entitlements, signing, and provisioning. Do not
  expect player devices to have any global SDL install.
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
- iOS: the Xcode app bundle owns native library embedding, assets, entitlements,
  signing, and provisioning. Desktop copy rules do not apply.

## Android Direction

Android should be treated as a first-class platform scaffold, not as a desktop
install rule:

- Splonks should eventually own an `android/` Gradle app because Splonks is the
  final player-facing game.
- Gradle should call CMake through `externalNativeBuild`.
- The Android native target should follow SDL's expected app shape for SDL3.
  Splonks uses an Android output library named `main` for SDL activity loading.
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

## iOS Direction

iOS should be treated as a first-class mobile release target, but not as an
automatic CI path during normal development:

- Splonks should eventually own an iOS app target because Splonks is the final
  player-facing game.
- The iOS build should use the SDL3-supported iOS/Xcode path and should not
  introduce SDL2 compatibility glue.
- Use `/home/vega/Coding/GameDev/how-to-multi-backend-rendering` as the local
  reference for the first scaffold. Its `ios-sim` preset validates CMake/Xcode
  project generation and native build.
- The first Splonks milestone should mirror that proven simulator build shape,
  then swap in SDL3 and Splonks assets/runtime wiring.
- The app target owns touch input, safe-area/layout behavior, asset bundling,
  entitlements, signing, provisioning, and TestFlight/App Store packaging.
- Gubsy should remain portable engine/library code for iOS consumers. It should
  not own the Splonks iOS app project.
- Remote iOS validation should be manual or tag/release based. Apple signing and
  provisioning make iOS a poor fit for automatic per-push package builds.

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
- Gubsy and Splonks `FetchContent` fallbacks pin SDL3, SDL3_ttf, SDL3_image,
  and SDL3_mixer to exact upstream release commit SHAs instead of mutable tag
  names. Splonks also pins its imgui fallback to an exact release commit SHA.
- Gubsy and Splonks resolve SDL atomically. The default presets use the pinned
  fetched SDL stack, while `GUB_SDL_DEPS=system` and `SPLONKS_SDL_DEPS=system`
  require all SDL libraries to come from the system/package-manager stack.
- Both repos expose Linux package presets and `scripts/package_linux.sh`.
- The Linux package scripts build release package presets, create local
  `dist/` bundles, copy SDL-related shared libraries from the linked binaries,
  and create wrapper scripts that run with the bundled library directory first
  in `LD_LIBRARY_PATH`.
- Each repo keeps SDL/transitive runtime library name matching in
  `scripts/package_runtime_libs.sh` so Linux, macOS, and Windows package scripts
  use the same copy policy instead of drifting across platform-specific files.
- Both repos expose native macOS and Windows package presets plus
  `scripts/package_macos.sh` and `scripts/package_windows.sh`. These are
  platform-host scripts: run the macOS scripts on macOS and the Windows scripts
  on Windows through Git Bash/MSYS/MinGW/Cygwin.
- Splonks has an Android Gradle scaffold under `android/`, an `android-arm64`
  CMake preset, and `scripts/android/` dev-loop scripts. The scaffold expects an
  official SDL3 Android AAR in `android/app/libs/` and enables Android Gradle
  Plugin Prefab support so CMake can consume the native SDL package.
- Splonks' Android activity extracts APK `assets/` into app-private storage,
  seeds missing `data/` files without overwriting player settings, and starts
  native code with `--project-root <path>` so the existing filesystem-relative
  asset loaders can run against a package-local directory.
- Splonks' Android activity accepts a `dev.splonks.game.ARGS` intent extra for
  native smoke arguments. `scripts/android/run_smoke.sh` launches the installed
  app with `--check-state-fingerprint-smoke` and verifies the expected logcat
  success line.
- `scripts/android/fetch_sdl3_aar.sh` downloads the pinned official SDL3 Android
  archive for the repo's current SDL pin and verifies its checksum before
  extracting the AAR into the Gradle app.
- Splonks commits a Gradle wrapper under `android/`, so APK builds do not rely
  on a global Gradle install once the Android SDK/NDK and SDL3 AAR are present.
- Android setup still requires JDK 17+, Android command-line tools, and the
  SDK/NDK packages installed by Splonks' `scripts/android/setup_sdk.sh`.
- Gubsy package launchers set `GUB_PROJECT_ROOT` so packaged sample runs use the
  bundled `data/`, `demo/`, `src/assets/`, and `tools/mod_repo/` trees instead
  of the developer source checkout.
- Splonks package launchers run from the package resource root so relative
  `assets/` and `data/` paths resolve inside the package.
- Native package scripts write `PACKAGE_MANIFEST.txt` into each package root
  with app name, platform, release mode, source revision, and generation time;
  package verifiers assert the manifest identity fields.
- Linux package validation should include an `ldd` check proving SDL resolves
  from `dist/.../lib` and a packaged executable smoke run through the wrapper.
- Both repos have `scripts/verify_package_linux.sh` to run the Linux package
  build, file layout assertions, bundled SDL `ldd` checks, and packaged smoke
  commands.
- Native macOS and Windows verifiers assert that the package includes SDL3,
  SDL3_image, SDL3_mixer, and SDL3_ttf runtime artifacts before running package
  smoke commands on those hosts.
- Gubsy's GitHub Actions package workflow is manual-only. It exists for remote
  platform confidence when explicitly requested, not for every push.
- Splonks' GitHub Actions package workflow runs only when manually dispatched or
  when a `v*` version tag is pushed. It owns release-oriented package
  validation for Linux, macOS, Windows, and Android.
- iOS is an explicit target, but no iOS CI/package path exists yet. Add it only
  after the Xcode/signing/provisioning path is designed.
- Gubsy's package workflow uploads developer/tooling package directories as
  release-check artifacts only when manually dispatched.
- Splonks' package workflow uploads versioned Linux/macOS/Windows release
  archives with SHA-256 files. It also uploads an Android debug APK, and uploads
  a signed Android release AAB when Android signing secrets are configured.
- Splonks' Linux developer onboarding has been verified from a fresh clone with
  no adjacent Gubsy checkout: the dev verifier configures, builds, and runs a
  headless smoke through the built game binary.
- macOS package validation should include an `otool -L` check, an app launch
  smoke, and signing/notarization checks before real distribution.
- Windows package validation should include a clean-machine or clean-shell run
  where the `.exe` resolves SDL DLLs from the package directory.
- Android validation now includes native CMake configure/build through Gradle,
  x86_64 emulator APK smoke, asset extraction/loading, logcat success
  detection, and signed arm64 release AAB generation with a throwaway upload
  keystore. Final Play distribution still needs the real upload key and Play
  Console upload validation.
- Splonks has iOS simulator and device CMake/Xcode scaffolds, simulator
  build/install/launch scripting, and a device archive/export script that emits
  an IPA, checksum, and manifest. These paths still need validation on macOS
  with Xcode and an Apple Developer team.

## Open Packaging Work

Priority order for the next phase:

1. Validate Splonks macOS developer onboarding and package scripts on a real
   macOS/Xcode/Homebrew machine.
2. Validate Splonks Windows developer onboarding and package scripts on a real
   Windows MSYS2/UCRT64 machine.
3. Validate Splonks iOS simulator launch and device archive/export on
   macOS/Xcode with real signing/provisioning.
4. Validate final Android Play distribution with the real upload key and Play
   Console upload path.

Detailed remaining work:

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
- Validate Android release upload with production signing material.
- Validate iOS CMake/Xcode scaffolds, touch/safe-area behavior, asset bundling,
  signing/provisioning, and TestFlight/App Store release packaging on macOS.
- Keep remote package checks manual or tag/release based. Do not make mobile or
  desktop package builds part of the normal push feedback loop.
