# SDL3 Dependency And Packaging Policy

Gubsy and downstream games should standardize on SDL3. Do not require players
to install SDL globally before running a game.

## Build Policy

Use this dependency order for developer and CI builds:

1. Prefer existing CMake package targets when the build environment provides
   SDL3, SDL3_ttf, SDL3_image, or SDL3_mixer.
2. Fall back to pkg-config on Unix-like developer machines.
3. If dependencies are still missing, fetch pinned SDL source releases with
   CMake `FetchContent`.

The `GUB_FETCH_DEPS` CMake option exists for this fallback path. It should stay
enabled by default for normal source builds so a clean machine can build Gubsy
without a global SDL3 install.

Release and CI builds should pin exact SDL versions. Prefer source archives with
hashes, submodules, or a lockable dependency mirror over floating branch names.

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

## Source Tree Direction

Keep SDL2 compatibility names and local shim headers out of normal source.
Public headers and downstream consumers should see an explicitly SDL3-based
dependency.

Shared Gubsy consumers such as Splonks should not independently fetch their own
conflicting SDL copies when Gubsy is already part of the same CMake graph. The
top-level project should arrange for one SDL3 target set to be visible to all
engine and game targets.

## Open Packaging Work

- Add install/package rules that copy SDL runtime libraries beside Gubsy tools
  and sample executables.
- Add per-platform release presets for Windows, macOS, Linux, and Android.
- Decide whether release builds prefer shared SDL libraries or static linking
  per platform.
- Add CI jobs that prove clean source builds work without preinstalled SDL3.
- Add Android packaging once the Gradle project exists.
