# Demo Public API Cleanup Plan

`demo/` should become the in-repo proof that a real game can import and use
Gubsy as a library. Today it still builds as an in-repo integration layer with
direct access to private `src/...` headers.

This plan removes that privilege in stages without weakening the library
boundary.

## Goal

Make `demo/` consume Gubsy through public headers under `include/gubsy/` instead
of private `src/...` headers.

The final shape should be:

```text
include/gubsy/   public API used by games
src/             private implementation
demo/            example game using public Gubsy API
```

`demo/` should still build into `./build/gubsy` and preserve the current lobby,
menus, settings, profile, binds, mod browser, built-in mod, local play, and
debug/editor behavior.

## Current State

The external import path works:

1. `tools/consumer_smoke` is a separate CMake project.
2. It imports Gubsy with `add_subdirectory`.
3. It links only `gubsy::engine`.
4. It includes only `gubsy/...` public headers.
5. `ctest` passes this smoke test.

The bundled demo is not clean yet:

1. `demo/` has 89 C++ files.
2. `demo/` directly includes private `src/...` headers from about 40 files.
3. The biggest private dependencies are:
   - `src/engine_state.hpp`: 24 includes.
   - `src/menu/menu_manager.hpp`: 11 includes.
   - `src/menu/menu_screen.hpp`: 11 includes.
   - `src/menu/menu_commands.hpp`: 10 includes.
   - `src/alerts.hpp`: 10 includes.
   - `src/mod_host.hpp`: 7 includes.
   - `src/player.hpp`: 7 includes.
   - `src/binds_profiles.hpp`: 6 includes.
   - `src/graphics.hpp`: 5 includes.
   - `src/session_contract.hpp`: 4 includes.
4. CMake currently gives the `gubsy` demo target `${GUB_SOURCE_DIR}` as a
   private include directory, which is what makes `#include "src/..."` work.

## Non-Goals

1. Do not expose `src/` as a public include path.
2. Do not make downstream games link separate `ginput`, `glayout`, or `gsexp`
   targets.
3. Do not blindly promote every private header into `include/gubsy/`.
4. Do not hide large private systems behind vague `utils` or catch-all headers.
5. Do not remove current demo behavior while cleaning the API.
6. Do not start unrelated `gassets`, `ganim`, `gparticles`, or `gnetcode` work
   during this cleanup.

## API Promotion Rules

1. Promote narrow public headers only when `demo/` proves a real game needs the
   API.
2. Prefer stable data handles, small context objects, and explicit functions
   over exposing `EngineState`.
3. Keep implementation-owned structs private when they are just storage layout.
4. If an API is only needed by the bundled demo, put it behind a clearly named
   public demo/app integration hook rather than leaking internal state.
5. Public headers must not include `src/...`.
6. After each slice, build and run the consumer smoke test.

## Ordered Cleanup Slices

### 1. Replace Easy Existing Facades

Several private includes already have public equivalents. Start here because it
should be mostly mechanical.

Replace direct demo includes with existing public headers:

1. `src/binds_profiles.hpp` -> `gubsy/input/binds_profile.hpp` or
   `gubsy/input/binds.hpp`.
2. `src/input_sources.hpp` -> `gubsy/input/sources.hpp`.
3. `src/settings_schema.hpp` -> `gubsy/settings/schema.hpp`.
4. `src/game_settings.hpp` -> `gubsy/settings/game_settings.hpp`.
5. `src/session_contract.hpp` -> `gubsy/lobby/session_contract.hpp`.
6. `src/session_link.hpp` -> `gubsy/lobby/session_link.hpp`.
7. `src/net_transport.hpp` -> `gubsy/lobby/net_transport.hpp`.
8. `src/room_matchmaking.hpp` -> `gubsy/lobby/room_matchmaking.hpp`.
9. `src/matchmaking.hpp` -> `gubsy/lobby/matchmaking.hpp`.
10. `src/player.hpp` -> `gubsy/profiles/player.hpp`.
11. `src/menu/menu_ids.hpp` -> `gubsy/menu/ids.hpp`.
12. `src/menu/menu_commands.hpp` -> `gubsy/menu/commands.hpp`.
13. `src/menu/menu_manager.hpp` -> `gubsy/menu/manager.hpp`.
14. `src/menu/menu_screen.hpp` -> `gubsy/menu/screen.hpp`.
15. `src/menu/menu_system.hpp` -> `gubsy/menu/system.hpp`.

Expected result: fewer private includes with minimal API design work.

### 2. App And Runtime Access

`src/engine_state.hpp` is the largest blocker. Do not expose it.

Promote or add public APIs that let a game do the real work it currently does
through `EngineState`.

Needed public surface:

1. Runtime accessors for app context and config.
2. Runtime accessors for current mode, timing, and frame info.
3. Runtime registration for modes, menu screens, settings schemas, binds
   schemas, layout definitions, and debug windows.
4. Runtime queries for players, profiles, selected profile IDs, and input
   devices.
5. Runtime mutation functions for common game-kit operations instead of direct
   field writes.

Likely headers:

1. `include/gubsy/runtime.hpp`
2. `include/gubsy/app.hpp`
3. New `include/gubsy/modes.hpp`
4. New `include/gubsy/debug.hpp`

Expected result: `demo/` stops including `src/engine_state.hpp`.

### 3. Menu Integration

The menu screens currently reach into menu internals and `EngineState`.

Keep public menu APIs data-oriented:

1. Promote only the menu types needed to define screens and widgets.
2. Provide a public `MenuContext` or `GubsyMenuContext` that contains safe
   accessors rather than raw engine storage.
3. Keep widget construction and command dispatch public.
4. Keep renderer/cache/repeat/focus internals private unless games truly need
   them.
5. Make demo menu screens register through public calls.

Likely headers:

1. Existing `include/gubsy/menu/screen.hpp`.
2. Existing `include/gubsy/menu/commands.hpp`.
3. Existing `include/gubsy/menu/manager.hpp`.
4. Existing `include/gubsy/menu/system.hpp`.

Expected result: demo menu screens include `gubsy/menu/...`, not
`src/menu/...`.

### 4. Alerts, Audio, Graphics, Render

These are real game-kit services, but they should not expose storage structs.

Promote focused service APIs:

1. `gubsy/alerts.hpp`: push alert, clear alerts, query active alerts if needed.
2. `gubsy/audio.hpp`: play UI/game sounds, set volume, query audio availability.
3. `gubsy/graphics.hpp`: renderer/window access only if the game legitimately
   draws through SDL directly.
4. `gubsy/render.hpp`: helper drawing functions that are intended for game use.

Be careful with `graphics.hpp`: exposing raw SDL renderer is practical, but it
couples Gubsy consumers to SDL3. That is acceptable if we decide Gubsy is
SDL3-based, but it should be intentional.

Expected result: gameplay/demo drawing code stops including private rendering
headers.

### 5. Mods And Content Hooks

The demo uses private mod APIs heavily because it exercises the mod browser and
Lua content host.

Promote the smallest stable mod-facing surface:

1. `gubsy/mods/host.hpp`: register APIs, activate mods, query loaded mods.
2. `gubsy/mods/install.hpp`: install/uninstall/update operations used by menu
   screens.
3. `gubsy/mods/config.hpp`: mod server config and catalog source setup.
4. `gubsy/mods/types.hpp`: public mod IDs, manifests, diagnostics, and status.

Do not make all mod manager internals public. The demo should talk to a mod
service API, not own the mod manager storage.

Expected result: lobby/mods screens and demo Lua API registration use
`gubsy/mods/...`.

### 6. Input Runtime And Device State

The demo currently reads private input/device state for gameplay input frames.

Public options:

1. Promote safe query functions for action/button/axis state.
2. Promote device source descriptions and selected input source state.
3. Keep raw SDL device arrays private unless there is a direct gameplay need.
4. If games need raw reads, expose explicit `gubsy/input/runtime.hpp` functions
   rather than `DeviceState`.

Expected result: `demo/input_frame.cpp` and menu input code use public input
queries.

### 7. IDs And Small Generic Utilities

Some private includes are small ID helpers:

1. `src/menu_layout_ids.hpp`
2. `src/vid_pool.hpp`
3. `src/runtime_settings.hpp`

Decide case by case:

1. If the type is generic and game-facing, promote it.
2. If it is only a temporary implementation detail, move the needed constants
   into demo-owned headers.
3. Avoid creating a broad public `gubsy/utils.hpp`.

### 8. Remove Demo Private Include Privilege

After the previous slices, enforce the boundary.

1. Remove `${GUB_SOURCE_DIR}` from `target_include_directories(gubsy PRIVATE ...)`.
2. Keep only `include`, `demo`, third-party, and needed system/vendor include
   paths.
3. Extend `scripts/check_consumption_boundary.sh` to fail on
   `#include "src/..."` inside `demo/`.
4. Keep existing checks that public headers and consumer smokes do not include
   `src/...`.

Expected result: `demo/` behaves like an in-tree external consumer.

## Suggested Commit Order

1. Mechanical replacements for existing public facades.
2. Public runtime/app accessor slice.
3. Public menu context/screen registration slice.
4. Public alerts/audio/graphics/render slice.
5. Public mods slice.
6. Public input runtime slice.
7. Small ID/helper cleanup slice.
8. CMake boundary enforcement slice.

Each commit should preserve behavior and keep the demo runnable.

## Verification Gates

Run after each slice:

```sh
./scripts/build.sh
ctest --test-dir build --output-on-failure
./scripts/check_consumption_boundary.sh
```

Final additional checks:

```sh
grep -R '#include[[:space:]]*[<"]src/' demo include/gubsy tools/consumer_smoke tools/public_api_smoke
grep -n '\${GUB_SOURCE_DIR}' CMakeLists.txt
```

The first command should find no private `src/...` includes in `demo` or public
consumer code. The second command should show no repo-root include directory on
the `gubsy` demo target and no repo-root public include directory on
`gubsy_engine`.

## Definition Of Done

1. `demo/` contains no `#include "src/..."` or `#include <src/...>`.
2. `demo/` builds and runs against `gubsy::engine`.
3. `demo/` no longer receives `${GUB_SOURCE_DIR}` as an include directory just
   to see private implementation headers.
4. Public headers under `include/gubsy/` do not include `src/...`.
5. `tools/consumer_smoke` still imports Gubsy as a separate project and passes.
6. `./scripts/build.sh`, `ctest --test-dir build --output-on-failure`, and
   `./scripts/check_consumption_boundary.sh` pass.
