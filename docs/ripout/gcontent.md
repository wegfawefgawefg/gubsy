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
4. No sandbox policy in v1.
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

## Ambiguities

1. Should mod script stages copy Factorio's `settings/data/control` split, or
   use a simpler Gubsy-specific sexpr/Lua stage model?
2. What remote catalog fields are mandatory for the first clean implementation:
   download URL, hashes, signatures, changelog, screenshots, dependencies,
   compatibility, size, author, and tags?
