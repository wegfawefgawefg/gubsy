# Engine/Game Split Plan

This document describes the concrete split we want between the reusable engine
and any game built on top of it.

Goal
----

`gubsy` should be usable as a library dependency.

A game should be able to:

- vendor `gubsy` as a submodule, subtree, or `FetchContent` dependency
- link `gubsy::engine`
- build its own game without editing engine internals
- upgrade `gubsy` like a normal dependency instead of maintaining an engine fork

Required boundary
-----------------

The engine must own infrastructure and services:

- windowing
- input device collection
- audio plumbing
- rendering infrastructure
- menu framework
- settings/profile persistence infrastructure
- save/load infrastructure hooks
- matchmaking/transport/session plumbing
- mod discovery/install/runtime boundaries
- debug infrastructure

The game must own semantics and policy:

- game state
- gameplay rules
- input meaning
- rendering meaning
- asset metadata meaning
- networking schema and authority policy
- game-defined menus and debug panels
- mod API surface

Hard rule
---------

`engine/` must not include headers from `demo/`.

If a feature needs game data, the engine should get that data through an
explicit registration point, callback, app context, or public interface.

Current violations
------------------

The main violations today are:

1. Engine runtime types depend on game types.
   Examples:
   - `engine/globals.hpp`
   - `engine/engine_state.hpp`
   - `engine/input_system.hpp`
   - `engine/input_queries.cpp`
2. Engine boot performs sample/game registration.
   Example:
   - `engine/run.cpp`
3. Engine menu code depends on game ids and game state.
   Examples:
   - `engine/menu/menu_system_state.hpp`
   - `engine/menu/menu_render.cpp`
   - `engine/menu/screens/settings_hub_screen.cpp`
   - `engine/menu/screens/settings_category_screen.cpp`
4. Engine debug code depends on sample/game state.
   Example:
   - `engine/imgui_debug/session_window.cpp`

Cut categories
--------------

1. Boot boundary
   - Replace sample-specific registration in engine boot with explicit app
     hooks.
   - The sample should register modes, menu screens, mod APIs, and game-side
     debug panels from its own code.

2. Input boundary
   - Stop storing sample-defined input frame types in engine headers.
   - Engine should own raw device state collection.
   - Game should build and own its interpreted per-player input state.

3. Menu boundary
   - Engine menu framework should only know about engine ids/types and optional
     app context pointers.
   - Sample screens and sample layout ids should live outside `engine/`.

4. Debug boundary
   - Engine debug infrastructure should allow registering app panels.
   - Sample-specific windows should move out of `engine/`.

5. State boundary
   - Engine state should be engine-only.
   - Game state should be app-owned and passed explicitly where needed.

Sample role
-----------

The bundled sample can stay in this repo for now, but only as a consumer:

- useful for engine development
- useful for regression coverage
- useful as a reference integration

It should not be part of the engine dependency surface.

Build policy
------------

- `gubsy_engine` is the reusable library target.
- The bundled sample and local tools are developer extras.
- Top-level builds can enable them by default.
- Vendored/subproject builds should not pull them in unless explicitly enabled.

Next implementation steps
-------------------------

1. Remove game includes from `engine/globals.hpp` and `engine/engine_state.hpp`.
2. Replace sample registration in `engine/run.cpp` with app hooks.
3. Move sample-specific menu/debug pieces out of `engine/`.
4. Make `gubsy_engine` compile with no `demo/` includes at all.
