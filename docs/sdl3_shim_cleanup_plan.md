# SDL3 Shim Cleanup

Gubsy builds directly against SDL3. The old local compatibility headers from
the SDL2-to-SDL3 migration have been removed so engine, demo, tool, and public
headers now expose the real SDL3 dependency boundary.

## Current State

The build links SDL3, SDL3_ttf, SDL3_image, and SDL3_mixer. Gubsy source and
public headers should use these include forms directly:

- `<SDL3/SDL.h>`
- `<SDL3/SDL_events.h>`
- `<SDL3/SDL_keyboard.h>`
- `<SDL3/SDL_render.h>`
- `<SDL3_ttf/SDL_ttf.h>`
- `<SDL3_image/SDL_image.h>`
- `<SDL3_mixer/SDL_mixer.h>`

These local shim headers were deleted:

- `SDL.h`
- `include/SDL.h`
- `SDL2/SDL.h`
- `SDL2/SDL_events.h`
- `SDL2/SDL_image.h`
- `SDL2/SDL_keyboard.h`
- `SDL2/SDL_mixer.h`
- `SDL2/SDL_render.h`
- `SDL2/SDL_ttf.h`
- `include/SDL2/SDL.h`
- `SDL_mixer.h`

Gubsy does not define `SDL_ENABLE_OLD_NAMES` in normal builds. Code should use
SDL3 names directly, including gamepad terminology, SDL3 event constants, SDL3
renderer functions, and SDL3 text input signatures.

## Cleanup Goals

1. Keep Gubsy source and public headers explicitly SDL3-based.
2. Keep the fake SDL2 include tree deleted.
3. Keep `src/sdl_event_adapter.*` as an architectural event/input adapter, not
   as an SDL compatibility shim.
4. Keep downstream consumers, including Splonks, using one shared SDL3 target
   set from the top-level CMake graph.

## Guard Check

The same check is enforced by CTest as `sdl3_shim_guard`. You can also run it
directly:

```sh
./scripts/check_sdl3_shim_cleanup.sh
```

The vendored ImGui tree may still contain unused upstream SDL2 backend files.
Those are third-party reference files and are not part of Gubsy's SDL backend.

Run this validation after any SDL dependency or event/input change:

- `./scripts/build.sh`
- `ctest --test-dir build --output-on-failure`
- `./scripts/room_smoke.sh`
- `./scripts/lobby_online_smoke.sh`

## Non-Goals

- Do not reintroduce real SDL2 packages.
- Do not keep dual SDL2/SDL3 support unless a real product requirement appears.
- Do not ask players or downstream consumers to install SDL globally.

## Splonks Comparison

Splonks is already in the cleaner state. It uses real SDL3 headers such as
`<SDL3/SDL.h>`, `<SDL3_image/SDL_image.h>`, `<SDL3_ttf/SDL_ttf.h>`, and
`<SDL3_mixer/SDL_mixer.h>`. It does not have local fake `SDL2/` shim headers.
