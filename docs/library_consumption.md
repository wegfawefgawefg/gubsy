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

The engine no longer has direct `engine/ -> game/` includes in the current tree,
but it is not yet a clean imported library boundary. The main violations are:

1. Public `include/gubsy/...` headers are still mostly thin facades over
   `engine/...` headers, so the repo root remains a public include path.
2. `GubsyRuntime` is still backed directly by `EngineState`; this is acceptable
   during migration, but it means engine internals are still visible to any
   consumer that wants to inspect them.
3. Engine boot flow still assumes the built-in menu/profile/settings shell is
   always registered. Mods are runtime-optional now, but broader app-owned
   registration hooks are still incomplete.
4. Engine path/runtime assumptions still lean on the current repo structure.
   The current path layer is better than before, but a library consumer should be
   able to provide app-owned paths rather than rely on source-tree defaults.

Cut plan
--------

1. Keep `engine -> game` includes at zero and add checks if this regresses.
2. Promote stable public headers under `include/gubsy/...` instead of exposing
   every `engine/...` header.
3. Move repo-root/`engine/` from public to private include paths once the public
   headers no longer need implementation headers.
4. Replace sample registration in engine boot with explicit app/bootstrap hooks.
5. Move sample-specific menu screens, layout ids, and debug windows out of the
   engine layer when they are not generally useful to the game kit.
6. Replace sample-specific input/state types in public headers with generic
   engine-owned interfaces or opaque callbacks.
7. Keep the sample as an in-repo consumer until the boundary is clean enough
   that moving it to its own repo would be a mechanical step instead of a
   semantic one.

Current verification
--------------------

- `tools/public_api_smoke` includes only `gubsy/...` public headers from inside
  the top-level build.
- `tools/consumer_smoke` is a separate CMake project that imports Gubsy through
  `add_subdirectory`, links only `gubsy::engine`, and includes only
  `gubsy/...` headers.
- `scripts/check_consumption_boundary.sh` checks the current import boundary:
  no `engine/ -> game/` includes, no smoke-test includes of private
  `engine/...` headers, and no sibling glib checkout requirement in CMake/docs.
- `GUB_BUILD_SAMPLE` and `GUB_BUILD_TOOLS` default on for a top-level Gubsy
  checkout and off when Gubsy is imported as a subproject.

Practical target
----------------

The correct end state is:

- `gubsy_engine` builds with no includes from `game/`
- the sample links the engine through public interfaces only
- a downstream user can vendor the repo and build only `gubsy_engine`
- upgrading `gubsy` looks like a normal dependency update instead of a fork
  merge
