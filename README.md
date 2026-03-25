<p align="center">
  <img src="docs/images/logo.png" alt="gubsy" width="384" />
</p>

gubsy
=====

`gubsy` is a small C++20 game engine/runtime with a bundled game layer used to exercise the engine.
It currently includes SDL2 rendering, audio, input, menu systems, UI layout tooling, player/profile
state, and Lua-driven mod loading.

<p align="center">
  <img src="docs/images/lobby.png" alt="gubsy lobby screenshot" width="900" />
</p>

What Is In This Repo
--------------------

- `engine/`: reusable engine/runtime code plus engine-owned built-in assets
- `game/`: the current game-specific layer, shipped game content, and built-in mods
- `tools/`: standalone servers and smoke-test binaries
- `data/`: writable runtime state for this game project
- `imgui/`: Dear ImGui sources used by the engine
- `scripts/`: local build/run helpers

Current Engine Features
-----------------------

- C++20 + CMake build
- SDL2-based windowing, rendering, image loading, fonts, and audio
- Input system for keyboard, mouse, and gamepad bindings
- Menu framework with screen registration, command routing, and per-screen state
- Player profiles, binds profiles, and game settings persistence
- UI layout loading/editing support
- Lua 5.4 mod host with runtime API registration and mod activation/reload
- ImGui debug/editor tooling in the engine layer

Build
-----

Requirements:
- CMake 3.20+
- A C++20 compiler
- SDL2
- GLM
- Lua 5.4
- SDL2_image
- SDL2_ttf
- SDL2_mixer

Linux (Debian/Ubuntu):
- `bash scripts/setup_debian.sh`
- `bash scripts/build.sh`

Run:
- `./build/gubsy`
- or `bash scripts/run.sh`

The build also creates `./build/arti` as a symlink alias to `gubsy`.

VS Code
-------

- Build task: `cmake: build (dev)`
- Run task: `run gubsy`
- F5 launch config: `Run gubsy (Debug, Linux)` or `Run gubsy (Debug, macOS)`

The workspace uses the `dev` CMake preset with strict warnings enabled. On GCC/Clang that includes
`-Wall -Wextra -Wpedantic`, plus additional warning flags and `-Werror`.

Formatting
----------

- The repo includes a top-level `.clang-format`
- VS Code is configured to use `clang-format`
- `compile_commands.json` is generated into `build/` by CMake

Project Layout Notes
--------------------

- The engine entrypoints live under `engine/`
- The executable target is defined in `CMakeLists.txt` as `gubsy`
- The bundled game layer registers modes, menus, binds, settings schemas, and mod APIs from `game/main.cpp`

Docs
----

- Developer setup: `docs/dev_setup.md`
- Engine roadmap: `docs/engine_roadmap.md`
- Engine 0.1 checklist: `docs/engine_0_1_checklist.md`
- Code-first engine intent: `docs/code_first_engine_intent.md`
- Repo layout: `docs/repo_layout.md`
- Content policy: `docs/content_policy.md`
- Library consumption: `docs/library_consumption.md`
- Cooperative multiplayer policy: `docs/cooperative_multiplayer_policy.md`
- Session contract: `docs/session_contract.md`
- Networking boundary: `docs/networking_boundary.md`
- Session browser flow: `docs/session_browser_flow.md`
- Steam onboarding TODO: `docs/steam_onboarding_todo.md`
- Menu system notes: `docs/menu.md`
- UI layout system notes: `docs/ui_layout_system.md`
- Mod/API notes: `docs/mod_api_system.md`
