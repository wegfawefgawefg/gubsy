# gassets Plan

`gassets` is an asset-agnostic registry and manifest library. It knows what
assets exist and how records override each other. It does not know how to load
textures, audio, animations, particles, or gameplay data.

All manifests are S-expressions. Do not add JSON/TOML compatibility importers
for the old Gubsy shapes; migrate old callers to the new format.

## Responsibilities

1. Define stable asset ids and asset type ids.
2. Store asset records with key, type, source path, owner/root id, metadata file
   path, dependencies, tags, and passive fields.
3. Load and save small asset manifest files.
4. Merge records from ordered roots.
5. Resolve overrides deterministically.
6. Report duplicate records, missing dependencies, dependency cycles, unknown
   types, and invalid paths.
7. Provide lookup by id, key, type, and owner/root.
8. Preserve enough source-location information for useful diagnostics.
9. Keep paths as caller-owned filesystem strings; do not assume install layout.
10. Support a one-root non-modded game as well as a multi-root mod stack.
11. Provide file timestamp/hash helpers for callers that need hot reload,
    cache validation, or content compatibility checks.

## Not Responsible

1. No mod enable/disable policy.
2. No remote catalog or install logic.
3. No texture/image/audio decoding.
4. No renderer handles.
5. No hot reload backend in v1, beyond enough data for callers to check files.
6. No animation, particle, entity, tile, or gameplay-specific schemas.
7. No global asset manager singleton.
8. No Lua or scripting.
9. No backwards-compatibility layer for old manifests.

## Relationship To Other Libraries

1. `gcontent` provides ordered content roots. `gassets` consumes roots.
2. `ganim` can load animation assets by querying `gassets` for type `animation`.
3. `gparticles` can load particle assets by querying type `particle`.
4. `gaudioassets` can load audio metadata by querying type `audio`.
5. Games can use `gassets` directly with one base root and no `gcontent`.

## Initial File Shape

Example sketch:

```lisp
(assets
  (asset
    (key "player.run")
    (type "animation")
    (source "anims/player_run.lisp")
    (deps "texture.player")
    (tags "player" "movement"))
  (asset
    (key "texture.player")
    (type "texture")
    (source "graphics/player.png")))
```

## Decisions

1. Use sexpr manifests only.
2. Use both stable numeric ids and string keys. Runtime code should use ids;
   tools, diagnostics, and manifests keep strings.
3. Asset type ids should be dynamic hashed/string-backed ids, not C++ enums.
   Mods and hotloaded packages must be able to introduce new asset categories.
4. Dependency cycles are errors.
5. Unknown fields are not allowed in core manifests. If custom data is needed,
   it must live in an explicit typed metadata form owned by the relevant typed
   library or game.
6. `gassets` should read and write manifests. Humans edit files externally, but
   tools will need save/write paths.
7. Keep the interrupted empty `~/Coding/Gamedev/gassets` scaffold and reuse it
   when implementation starts.

## Implementation Steps

1. Create repo scaffold.
2. Define `AssetId`, `AssetTypeId`, `AssetRecord`, `AssetRoot`, `Registry`,
   `Diagnostic`, and `ResolveReport`.
3. Implement key/type hashing and string lookup.
4. Implement manifest load/save through `gsexp`.
5. Implement ordered root merge and override resolution.
6. Implement dependency validation.
7. Add tests for duplicate keys, overrides, missing deps, cycles, and type
   queries.
8. Add demo with base game plus one override pack.
9. Write dry README and integration docs.
10. Publish repo.

## Ambiguities

1. Should override records replace whole records, or should they be able to patch
   individual fields?
2. Should `source` be required for every asset, or can records be purely virtual?
