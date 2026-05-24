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

The engine no longer has direct `src/ -> demo/` includes in the current tree,
but it is not yet a clean imported library boundary. The main violations are:

1. Some public `include/gubsy/...` headers still expose old first-party helper
   namespaces such as `glayout` and `ginput` through type aliases.
2. `GubsyRuntime` now owns an internal `EngineState`, but some subsystem
   facades still expose types that are too close to implementation details.
3. Engine boot flow still assumes the built-in menu/profile/settings shell is
   always registered. Mods are runtime-optional now, but broader app-owned
   registration hooks are still incomplete.
4. Engine path/runtime assumptions still lean on the current repo structure.
   The current path layer is better than before, but a library consumer should be
   able to provide app-owned paths rather than rely on source-tree defaults.

Cut plan
--------

1. Keep `src -> demo` includes at zero and add checks if this regresses.
2. Promote stable public headers under `include/gubsy/...` instead of exposing
   every `src/...` header.
3. Move repo-root/`src/` from public to private include paths once the public
   headers no longer need implementation headers.
4. Replace sample registration in engine boot with explicit app/bootstrap hooks.
5. Move sample-specific menu screens, layout ids, and debug windows out of the
   engine layer when they are not generally useful to the game kit.
6. Replace sample-specific input/state types in public headers with generic
   engine-owned interfaces or opaque callbacks.
7. Keep the sample as an in-repo consumer until it uses the same public
   dependency boundary as an external game. At that point, moving it to its own
   repo should be a mechanical step instead of a semantic one.

Current verification
--------------------

- `tools/public_api_smoke` includes only `gubsy/...` public headers from inside
  the top-level build.
- `tools/consumer_smoke` is a separate CMake project that imports Gubsy through
  `add_subdirectory`, links only `gubsy::engine`, and includes only
  `gubsy/...` headers.
- `scripts/check_consumption_boundary.sh` checks the current import boundary:
  no `src/ -> demo/` includes, no smoke-test includes of private
  `src/...` headers, and no sibling glib checkout requirement in CMake/docs.
- `ctest --test-dir build --output-on-failure` runs the public API smoke,
  consumption boundary check, and external consumer smoke when tools/tests are
  enabled.
- `GUB_BUILD_SAMPLE` and `GUB_BUILD_TOOLS` default on for a top-level Gubsy
  checkout and off when Gubsy is imported as a subproject.

Practical target
----------------

The correct end state is:

- `gubsy_engine` builds with no includes from `demo/`
- the sample links the engine through public interfaces only
- a downstream user can vendor the repo and build only `gubsy_engine`
- upgrading `gubsy` looks like a normal dependency update instead of a fork
  merge
