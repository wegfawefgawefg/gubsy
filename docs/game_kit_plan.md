# Gubsy Game Kit Plan

`gubsy` should be a reusable C++ game kit, not a full general engine and not a
sample game that downstream projects fork. The first real target is
`splonks-cpp`: Splonks should be able to import `gubsy`, use the reusable shell
systems, and keep its gameplay/world/rendering policy in Splonks.

## Goal

Make `gubsy::engine` usable by games that want:

1. Window/audio/input bootstrapping.
2. Main menu, settings, profile, binds, and lobby/session UI infrastructure.
3. User profiles, input bind profiles, settings profiles, and save/profile
   relationship helpers.
4. Layout and navigation editing/debug tooling.
5. Optional mod/content/Lua systems when a game chooses to use them.

Splonks should be able to use the first four without inheriting the mod browser,
Lua mod host, mod server, demo content, or sample game assumptions.

## Non-Goals

1. Do not make Gubsy own Splonks gameplay, entities, physics, world simulation,
   renderer policy, or content-specific metadata.
2. Do not force mods or Lua into games that only want the menu/profile/lobby
   shell.
3. Do not make downstream games clone or link individual `g*` libs directly.
   Games should consume `gubsy` and use Gubsy-owned modules.
4. Do not move every reusable-looking system out of games immediately. Extract
   only when the boundary is clear and the game-kit use case benefits.

## Target Shape

```text
gubsy/include/gubsy/
  public API consumed by games

gubsy/src/
  reusable game-kit systems and app lifecycle helpers
  layout/
  input/
  sexp/
  settings/
  audio/
  ...

gubsy/demo/
  bundled sample/demo consumer used to exercise the kit

splonks-cpp/
  real game consumer that imports gubsy and owns gameplay
```

The sample can remain in the repo, but it must behave like a consumer of the
engine. It should not leak sample assumptions into the reusable target.

## Module Ownership Direction

The standalone `g*` repos are still useful as clean reference projects and
development testbeds. They do not need to be the consumer-facing shape inside
Gubsy.

The target direction is:

1. Gubsy imports the useful code and ideas into boring engine modules.
2. Consumer projects link/import `gubsy::engine`.
3. Consumer projects include Gubsy headers, not standalone glib headers.
4. Gubsy does not create pass-through wrapper APIs around separate libs.
5. If a subsystem belongs to the game kit, it lives under `src/` with a public
   facade under `include/gubsy/` only when consumers need it.

Examples:

```text
src/layout/
  layout data, layout store, layout editor core, layout debug UI

src/input/
  raw input collection, bind profiles, input profile persistence

src/settings/ and src/profiles/
  settings/profile/save config helpers
```

The current `libs/` folder is a migration artifact from the ripout work. It is
acceptable short-term, but the cleaner end state is Gubsy-owned modules under
`src/`. See `docs/src_demo_refactor_plan.md` for the concrete file move plan.

## Required Consumer Switches

Runtime configuration for optional systems now exists in
`include/gubsy/app.hpp`:

```cpp
struct GubsyAppConfig {
    bool enable_mods{false};
    bool enable_mod_browser{false};
    bool enable_mod_hot_reload{false};
    bool enable_lua_mod_host{false};
};
```

The bundled demo enables these explicitly. Splonks can leave them disabled.

Build options can come later if needed:

```cmake
option(GUB_ENABLE_MODS "Build Gubsy mod host support" ON)
option(GUB_ENABLE_LUA_MODS "Build Lua mod host support" ON)
```

Do runtime separation first. Build-time separation is easier once behavior is
already clean.

## Mod Coupling Removed

The boot path used to assume mods were always part of the app:

1. `engine/run.cpp` creates runtime mod directories.
2. `engine/run.cpp` initializes `ModManager`.
3. `engine/run.cpp` discovers mods, scans sprite defs, loads mod sounds, and
   activates Lua-hosted mods.
4. `engine/run.cpp` polls mod hot reload every frame.
5. `engine/engine_state.hpp` stores `ModManager*` directly.
6. `engine/engine_state.cpp` registers the mods menu screen during base engine
   init.

These steps are now optional subsystem work controlled by `GubsyAppConfig`.

## Core Always-On Systems

These are part of the game-kit value and should work for Splonks:

1. Graphics/window bootstrap.
2. Audio bootstrap and simple built-in sound loading.
3. Raw input device collection.
4. `ginput` bind profile storage and lookup.
5. User profiles and settings profiles.
6. Menu framework and menu navigation.
7. Main menu/settings/profile/binds screens that are not demo-specific.
8. Layout loading and layout editor/debug tooling.
9. Lobby/session shell where game-specific session details are supplied by the
   caller.

## Optional Systems

These should be opt-in:

1. Mod manager.
2. Mod browser menu screen.
3. Mod install/uninstall server/catalog client.
4. Lua mod host.
5. Mod API registry and Lua API binding.
6. Filesystem mod hot reload.
7. Mod asset scanning and mod sound loading.

An app that disables these should still build and run the normal menu/profile
shell.

## Session Contract Shape

The existing session contract can keep mod-related fields, but empty values must
mean "no mod/content contract."

Required behavior:

1. Splonks can use game version, net protocol, and content revision without
   using mods.
2. `mod_hash` and `required_mod_ids` are ignored when empty.
3. Lobby/session UI should not require the mod browser or Lua host.

## Implementation Order

1. Done: add `GubsyAppConfig` to the app hooks passed into `do_the_gubsy`.
2. Done: default optional mod/Lua systems off for library-style consumers.
3. Done: enable the current mod/Lua behavior explicitly from the bundled sample.
4. Done: gate mod directory creation, manager initialization, discovery, hot reload,
   Lua activation, and mod asset scanning behind config flags.
5. Done: gate mods menu registration behind `enable_mod_browser`.
6. Keep `EngineState::mod_manager` nullable short-term, but ensure all users
   check the feature state before touching it.
7. Move toward an optional `EngineModsState` or subsystem object once the
   runtime gates are proven.
8. Audit engine screens and debug panels for demo-specific assumptions.
9. Keep menu/profile/binds/lobby features enabled without mods.
10. Done: add public and external consumer smoke checks that start Gubsy with
    mods disabled.
11. Move current first-party `libs/` code into Gubsy `src/` modules in small
    steps, starting with `src/sexp`, then layout and input.

## Splonks Import Target

A future Splonks bootstrap should look roughly like this:

```cpp
GubsyRuntime runtime{};
GubsyAppHooks hooks{};
hooks.app_context = &splonks;
hooks.config.enable_mods = false;
hooks.config.enable_mod_browser = false;
hooks.config.enable_mod_hot_reload = false;
hooks.config.enable_lua_mod_host = false;

do_the_gubsy(runtime, hooks);
```

Splonks then registers its own modes, menu screens, settings schema, binds
schema, session details, and game-specific callbacks.

## Practical Definition Of Done

This stage is done when:

1. `gubsy::engine` builds without requiring the bundled sample executable.
2. The bundled sample still runs with the same mod-enabled behavior it has now.
3. A no-mod consumer can run menus, profiles, binds, settings, and lobby shell.
4. No always-on boot path touches mod manager, Lua, mod browser, mod server, or
   mod hot reload unless config enables it.
5. Consumers use Gubsy module headers through `gubsy::engine`, not standalone
   glib targets.
6. The README documents Gubsy as a game kit with optional mod/content support.

## Next Step

Continue moving implementation files from `engine/` into boring private module
folders when it improves ownership. Runtime optionality and the public consumer
facade are in place; do not start unrelated `gassets`, `gaudio`, or `ganim`
work as part of this stage.
