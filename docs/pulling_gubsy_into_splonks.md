# Pulling Gubsy Into Splonks

This document captures the intended first integration of Gubsy into
`splonks-cpp`.

The important decision: Splonks should own the outer game loop. Gubsy should be
used as a library/game kit that Splonks drives.

## Goal

Make Splonks import Gubsy and use it for reusable shell systems:

1. Main menu.
2. Settings UI and settings persistence.
3. User profiles.
4. Input bind profiles.
5. Local player/profile selection.
6. Lobby/session shell.
7. Layout/menu debug tooling.
8. Optional mod/Lua systems disabled unless Splonks explicitly opts in.

The first milestone is not to replace Splonks gameplay. It is to let Splonks use
Gubsy for menus and app shell flow while Splonks keeps its world, entities,
renderer policy, assets, netcode, particles, and gameplay state.

## Ownership Decision

Use the host-driven library shape:

```cpp
SplonksApp splonks{};
GubsyRuntime gubsy{};
GubsyAppConfig config{};

gubsy_init(gubsy, config);

while (splonks.running) {
    splonks_poll_input(splonks);

    if (splonks_showing_menu(splonks)) {
        gubsy_update(gubsy, splonks.dt);
        gubsy_render(gubsy);
    } else {
        splonks_update(splonks);
        splonks_render(splonks);
    }
}

gubsy_shutdown(gubsy);
```

This is more normal for a C/C++ library. The game calls the library. The library
does not own the app unless there is a concrete reason.

Do not start with:

```cpp
gubsy_run_game(splonks_callbacks);
```

That makes Gubsy act like a framework/engine owner. It may become useful later,
but it is not the simpler first integration.

## Menu Commands

Gubsy menus should call host game functions through command callbacks.

Example shape:

```cpp
gubsy_register_menu_command(gubsy, "start_game", start_game_callback, &splonks);
gubsy_register_menu_command(gubsy, "quit_game", quit_game_callback, &splonks);
```

The callback owns the game-specific behavior:

```cpp
void start_game_callback(void* user_data) {
    auto* splonks = static_cast<SplonksApp*>(user_data);
    splonks_start_game(*splonks);
}
```

This is better than forcing Splonks to register a Gubsy-owned gameplay mode on
day one. If Splonks later needs Gubsy-managed modes for transitions or overlays,
add that with evidence.

## In-Game Menus

Gubsy should also be useful for pause menus and in-game overlays later.

The same host-driven shape works:

```cpp
splonks_update_world(splonks);
splonks_render_world(splonks);

if (splonks.pause_menu_open) {
    gubsy_update(gubsy, splonks.dt);
    gubsy_render(gubsy);
}
```

Splonks owns gameplay. Gubsy owns menu stack, layout, navigation, input profile
UI, and settings UI.

## First Integration Scope

Do first:

1. Add Gubsy as a dependency in Splonks CMake.
2. Link Splonks against `gubsy::engine`.
3. Initialize a `GubsyRuntime` from Splonks.
4. Disable optional mod/Lua systems initially.
5. Register Splonks menu commands such as start game, quit, open settings, and
   return to main menu.
6. Show Gubsy main menu before gameplay starts.
7. Let a menu command enter existing Splonks gameplay.
8. Keep Splonks gameplay update/render untouched.

Do not move on the first pass:

1. Splonks renderer.
2. Splonks world/entity logic.
3. Splonks particles.
4. Splonks asset loading.
5. Splonks netcode.
6. Splonks animation/sprite metadata.
7. Splonks audio emitters.
8. Splonks save/gameplay serialization.

## Likely Gubsy API Gaps

Splonks integration will probably force these public APIs:

1. Manual `gubsy_init`, `gubsy_update`, `gubsy_render`, `gubsy_shutdown`
   functions that do not require Gubsy to own the loop.
2. Public menu command registration.
3. Public screen push/pop helpers.
4. Public settings schema registration.
5. Public binds schema registration.
6. Public input profile load/save/select helpers.
7. Public profile selection helpers.
8. Public path configuration so Splonks can choose its data/config/save roots.
9. Public renderer/window integration points, or clear ownership rules if
   Splonks and Gubsy share SDL3 objects.

Do not solve all of these abstractly before integration. Let the first Splonks
compile attempt reveal the exact missing surface.

## What Might Move From Splonks To Gubsy Later

Move only when Splonks proves the boundary and another game would likely reuse
it.

Likely future Gubsy modules:

1. `src/assets/` and `include/gubsy/assets/`
   Asset IDs, manifests, loading registry, cache ownership, and hot reload
   primitives.
2. `src/particles/` and `include/gubsy/particles/`
   Data-driven particle emitters and update/draw command generation.
3. `src/anim/` and `include/gubsy/anim/`
   Sprite animation clips, frame spans, playback state, and metadata hooks.
4. `src/audio/` and `include/gubsy/audio/`
   Emitters, channels, distance/filter data, and playback handles.
5. `src/net/` and `include/gubsy/net/`
   Durable/ephemeral packet helpers, lockstep/session plumbing, barriers, and
   synchronization utilities.

These should be Gubsy-owned modules, not mandatory sibling `g*` repos. Separate
repos can still be useful as experiments, but the consumer story should be one
Gubsy dependency.

## What Should Stay In Splonks

1. Gameplay entities and world simulation.
2. Gameplay-specific metadata structs.
3. Concrete action IDs and input frame packing.
4. Renderer policy and camera ownership.
5. Game-specific asset interpretation.
6. Game-specific net protocol messages.
7. Game-specific save data.
8. Level/stage rules and transitions.

## Relationship To Demo Cleanup

The current `demo/` is not yet a clean external-style consumer. It still
includes private `src/...` headers.

Do not block Splonks integration on fully cleaning `demo/`.

Use Splonks as the stronger forcing function:

1. If Splonks needs an API, promote a narrow public Gubsy API.
2. If only `demo/` needs an API, keep it demo-local or defer.
3. If both Splonks and demo need the same behavior, it probably belongs in
   Gubsy public API.

## First Milestone Definition Of Done

1. Splonks CMake imports Gubsy and links `gubsy::engine`.
2. Splonks can initialize and shut down `GubsyRuntime`.
3. Optional Gubsy mod/Lua systems are disabled for Splonks.
4. Splonks can show a Gubsy menu before gameplay.
5. A Gubsy menu command can call into Splonks and start existing gameplay.
6. Splonks gameplay still runs through its existing update/render path.
7. No Splonks code includes Gubsy private `src/...` headers.
8. Gubsy's existing build, tests, and consumer smoke still pass.

## Verification

Run in Gubsy after public API changes:

```sh
./scripts/build.sh
ctest --test-dir build --output-on-failure
./scripts/check_consumption_boundary.sh
```

Run in Splonks after integration:

```sh
cmake --build build
```

Add a Splonks-side smoke target once the first integration compiles.
