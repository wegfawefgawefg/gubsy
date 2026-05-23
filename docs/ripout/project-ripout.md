# Project Ripout Plan

This plan orders the next reusable `g-*` library extractions. The goal is a
small set of vendorable C++20 libraries that let `gubsy`, `splonks-cpp`, and
future games share engine-grade systems without making every game depend on
Gubsy itself.

## Shared Standards

1. Each library gets its own repo under `~/Coding/Gamedev`.
2. Each repo uses C++20, CMake, `.clang-format`, build/run scripts, tests, dry
   README, simple SVG logo, and the same low-abstraction style.
3. Each repo follows `/home/vega/AGENTS.md`: direct C+-ish C++, mostly flat
   layout, clear file ownership, target file size around 300-500 lines.
4. Core targets avoid SDL, ImGui, OpenGL, Lua, engine globals, and game-specific
   state.
5. Optional adapters are allowed, but they must sit at the edge and must not
   pull backend dependencies into the core target.
6. Source data formats use `gsexp`. These are fresh second/third
   implementations, not compatibility wrappers around old TOML/JSON/YAML shapes.
7. Repos should support vendored source via CMake `add_subdirectory`.
8. Libraries should compose through ids, typed records, and explicit caller-owned
   glue, not hidden global registries.
9. Runtime hot paths must avoid string parsing, generic metadata walks, and
   allocation-heavy lookup.
10. Do not extract game rules just because the code is reusable-looking.
11. Do not ship "for now" compatibility hacks. Build the clean library we
    actually want, then migrate old callers to it.

## Dependency Shape

Preferred high-level stack:

```text
gsexp
  parser and writer helpers

gconfig
  settings/profile/config documents

gmods
  packages, mods, catalogs, server/install metadata, enabled order, roots

gassets
  asset ids, typed asset records, override resolution, dependency graph

ganim
  animation clips, frames, timing, animator state

gparticles
  particle specs, emitters, simulation, draw commands

gnetcode
  UDP-capable lockstep/rollback/session protocol building blocks

gubsy or game
  rendering, audio backend, gameplay metadata, app policy, concrete UI
```

`gubsy` can compose all of these into a code-first engine. The extracted
libraries must not depend on `gubsy`.

## Reference Notes

1. Factorio is a useful reference for mod shape, not a format to copy. It uses
   mandatory mod metadata, Lua data/control files, versioned dependencies,
   optional dependencies, incompatibilities, and a dependency-aware load order.
   `gmods` should use sexpr files and Gubsy naming, but the same concepts are
   worth carrying over.
2. Factorio orders active mods by dependency chain depth and then internal name.
   Use that as the default deterministic ordering model unless a Gubsy-specific
   reason appears.
3. Factorio's prototype/data stages are Lua-authored content systems. We do not
   need a separate C++-authored content path unless a concrete Gubsy or Splonks
   use case appears.
4. Span and Splonks both treat frame duration and offset/center-like fields as
   animation authoring data. Collision boxes, tile flags, and gameplay emit
   points stay game-owned sidecar metadata.

## Ordered Work

1. `gassets`
   Build the asset-agnostic registry first. It becomes the shared layer for
   asset ids, manifests, overrides, dependency checks, and lookup.
   See [gassets.md](gassets.md).

2. `gmods`
   Build package/mod identity and policy after `gassets` so content roots can
   feed asset catalogs cleanly. Include local manifests, remote catalog/server
   records, and install/uninstall state in the planned feature set.
   See [gcontent.md](gcontent.md).

3. `ganim`
   Extract animation clips/frames/animator state after `gassets` exists, so the
   relationship between asset records and typed animation loading is clear.
   See [ganim.md](ganim.md).

4. `gparticles`
   Extract particle specs/simulation after `ganim`, because particles may refer
   to animation/sprite assets but should not depend on a renderer.
   See [gparticles.md](gparticles.md).

5. `gnetcode`
   Extract a game-agnostic form of Splonks' lockstep/rollback/UDP netcode after
   the asset/content stack has stable boundaries. Some Splonks details are
   game-specific, but the durable netcode shape should be a real library, not a
   temporary adapter.
   See [gnetcode.md](gnetcode.md).

6. `gaudio`
   Audio files start as normal `gassets` records. A later `gaudio` can own the
   directional audio, channels, filtering, emitters, and backend-neutral audio
   policy from Splonks if that boundary proves useful.
   See [gaudioassets.md](gaudioassets.md).

7. `gcore`
   Create as the shared boring base for ids, hashing, stable handles, simple
   pools, and tiny common types once the next repos need them.
   See [gcore.md](gcore.md).

## Near-Term Execution Strategy

1. Create each repo with docs first.
2. Implement the clean full core for the known feature set. Avoid temporary
   "good enough for now" shapes that will be migrated away immediately.
3. Add a demo that exercises realistic Gubsy/Splonks-shaped data.
4. Add tests before attempting migration.
5. Publish to GitHub after the library builds and README is accurate.
6. Migrate one real caller only after the extracted API survives the demo.

## Open Ambiguity Index

Answer these before the implementation goal starts:

1. `gassets` uses sexpr manifests.
2. `gmods` should include local manifests, remote catalog/server records, and
   install/uninstall state in the planned feature set.
3. Working name is probably `gmods`; confirm whether this should cover both
   C++-authored content and Lua-authored content.
4. `ganim` should use asset ids and fit into the `gassets`/`gmods` stack, but
   the exact atlas/sprite-sheet ownership is still open.
5. `gparticles` can depend on `ganim`.
6. `gnetcode` should include UDP-capable real netcode, not only packet structs.
7. Snapshot data is game-authored. `gnetcode` should use caller/game snapshot
   structs through a clean interface.
8. Audio files are `gassets` records first. A later `gaudio` can own directional
   audio, channels, filtering, and emitters.
9. `gcore` probably should exist for hashing, ids, stable handles, pools, and
   common tiny types.
10. Reuse the interrupted empty `~/Coding/Gamedev/gassets` scaffold.
