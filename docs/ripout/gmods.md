# gmods Plan

Working repo name: `gmods`.

`gmods` owns content package and mod identity. It decides which packages are
available, compatible, enabled, ordered, installable, and visible in remote
catalogs. It does not load concrete assets.

The library should assume Lua/data-authored content by default. C++ systems live
in the host engine/game. Do not build a separate C++-authored content path unless
a concrete use case appears.

## Responsibilities

1. Parse local package/mod manifests.
2. Store package id, title, author, version, description, game compatibility,
   dependencies, optional dependencies, conflicts, APIs, and content root path.
3. Resolve enabled package order.
4. Validate missing dependencies, disabled dependencies, conflicts, duplicate
   package ids, version constraints, and dependency cycles.
5. Store enabled/disabled selection files.
6. Represent remote catalog entries.
7. Represent install/uninstall state.
8. Represent mod server metadata and catalog fetch/install contracts.
9. Provide ordered roots to `gassets`.
10. Keep server/catalog protocol separate from local package resolution.
11. Support required/base packages.
12. Support content hashes/signatures for compatibility checks and remote catalog
    integrity.

## Not Responsible

1. No asset record parsing.
2. No image/audio/animation loading.
3. No game scripting runtime.
4. No sandbox policy in the first clean implementation.
5. No menu UI.
6. No Gubsy `EngineState`.
7. No hardcoded Gubsy mod server unless explicitly chosen.
8. No old `info.toml` compatibility path. Use sexpr manifests and migrate
   callers.

## Relationship To Other Libraries

1. `gmods` feeds ordered roots into `gassets`.
2. `gassets` resolves concrete assets after `gmods` decides root order.
3. `gmenu` can build mod screens using `gmods` data, but `gmods` does not
   depend on `gmenu`.
4. `gconfig` may store user selection/preferences, but `gmods` owns the
   package records and resolver.
5. `gnetcode` can use `gmods` and `gassets` hashes for session compatibility,
   but hash calculation must be cached and explicit so runtime cost stays
   controlled.

## Implementation Steps

1. Define the full clean sexpr manifest and catalog shapes first.
2. Port the useful Gubsy mod info fields into plain structs.
3. Implement dependency ordering.
4. Add validation reports.
5. Add selection file load/save.
6. Add demo with base package, optional mod, missing dep, and override order.
7. Add remote catalog records.
8. Add install/uninstall planning structs.
9. Add mod-server/catalog fetch contracts and a backend edge for actual network
   fetches.
10. Publish repo.

## Decisions

1. Working repo name is `gmods`.
2. Use one manifest model for Lua/data-authored content. C++ systems are host
   code, not mod content records.
3. Use semantic/versioned dependency constraints with operators, similar in
   capability to Factorio dependencies.
4. Resolve dependency order deterministically by dependency depth, then stable
   internal mod id/name order.
5. Disabled required/base packages are impossible. Treat attempts as errors.
6. Package roots should be caller-resolved absolute paths internally, while
   manifests can contain relative paths.
7. Own content hash/signature fields now.
8. Existing Gubsy behavior is a simpler baseline: it reads dependencies,
   optional dependencies, game version, and orders required deps before
   dependents. The new library should be cleaner and more complete.
9. Use staged Lua/data content. Factorio's `settings/data/control` split is a
   useful model, but the names should be chosen deliberately for this stack
   rather than copied blindly.
10. Remote catalog records should carry all fields needed for install UI,
    compatibility, verification, and dependency resolution. Do not rely on a
    second hidden server contract for critical install data.

## Script/Data Stages

Factorio uses three broad mod lifecycle areas:

1. `settings`: declare mod settings and setting defaults before content is
   built.
2. `data`: define content/prototypes before the game starts.
3. `control`: run runtime scripts/events after the game is active.

That split is not a universal standard, but it is a proven model for Lua-authored
mod content. Current Gubsy is less formal: it has mod metadata, dependencies,
APIs, content/demo Lua files, and mod-server catalog data, but it does not expose
a clean staged lifecycle.

Recommended `gmods` stage shape:

1. `settings`: declare settings/config options contributed by a package.
2. `content`: declare assets, prototypes, recipes, items, menus, or other data
   records.
3. `runtime`: attach runtime scripts/event handlers if the host enables a Lua
   runtime.

The stage names are clearer for this codebase than `settings/data/control` while
still mapping cleanly to the Factorio idea. If we later want Factorio familiarity
over clarity, this is easy to rename before implementation.

Reference:

1. https://lua-api.factorio.com/latest/auxiliary/mod-structure.html
2. https://lua-api.factorio.com/latest/auxiliary/data-lifecycle.html

## Remote Catalog Fields

The clean catalog should include:

1. `id`
2. `title`
3. `author`
4. `version`
5. `summary`
6. `description`
7. `dependencies`
8. `optional_dependencies`
9. `conflicts`
10. `game_version` or engine compatibility constraints
11. `apis`
12. `download_url`
13. `files` with path, size, and hash
14. `total_bytes`
15. `content_hash`
16. `signature` when signing is enabled
17. `changelog`
18. `screenshots`
19. `tags`
20. `license` if packages may be redistributed

Gubsy's current catalog already uses the core of this shape: id, title, author,
version, summary/description, dependencies, game version, APIs, files, and total
bytes. The new version should make the compatibility, verification, and optional
dependency fields explicit.

## Remaining Questions

1. Should the public stage names be `settings/content/runtime` as recommended
   here, or exact Factorio-style `settings/data/control`?
