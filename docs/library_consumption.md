# Library Consumption Model

`gubsy` should be usable as a library dependency, not only as a forked engine
repo that users edit internally.

Intended usage model
--------------------

- A game vendors `gubsy` as a submodule, subtree, or `FetchContent`
  dependency.
- The game links `gubsy::engine`.
- The game owns its own code, state model, content, serialization, and gameplay
  policy.
- The engine provides infrastructure and services.

What the bundled sample is for
------------------------------

The bundled sample is still useful, but its role is narrow:

- development testbed for engine work
- regression coverage for engine APIs
- example consumer that shows how a game should integrate

It is not supposed to be part of the engine dependency surface.

Build policy
------------

- `gubsy_engine` is the reusable library target.
- The bundled sample and local tools are dev extras.
- When building `gubsy` as the top-level project, the sample and tools are on by
  default.
- When `gubsy` is brought in as a subproject, the sample and tools should be off
  by default so downstream users do not pay to compile them unless they opt in.

Current boundary violations
---------------------------

The engine is not yet truly standalone. The main violations are:

1. Engine runtime types still depend on sample/game types.
   Examples: `engine/globals.hpp`, `engine/engine_state.hpp`,
   `engine/input_system.hpp`, `engine/input_queries.cpp`.
2. Engine boot flow still performs sample-specific registration.
   Example: `engine/run.cpp`.
3. Engine menu/debug code still hardcodes sample UI/layout/state types.
   Examples: `engine/menu/screens/*.cpp`, `engine/menu/menu_system_state.hpp`,
   `engine/imgui_debug/session_window.cpp`.
4. Engine path/runtime assumptions still lean on the current repo structure.
   The current path layer is better than before, but a library consumer should be
   able to provide app-owned paths rather than rely on source-tree defaults.

Cut plan
--------

1. Remove all `engine -> game` includes.
2. Replace sample registration in engine boot with explicit app/bootstrap hooks.
3. Move sample-specific menu screens, layout ids, and debug windows out of the
   engine layer.
4. Replace sample-specific input/state types in engine headers with generic
   engine-owned interfaces or opaque callbacks.
5. Keep the sample as an in-repo consumer until the boundary is clean enough
   that moving it to its own repo would be a mechanical step instead of a
   semantic one.

Practical target
----------------

The correct end state is:

- `gubsy_engine` builds with no includes from `game/`
- the sample links the engine through public interfaces only
- a downstream user can vendor the repo and build only `gubsy_engine`
- upgrading `gubsy` looks like a normal dependency update instead of a fork
  merge
