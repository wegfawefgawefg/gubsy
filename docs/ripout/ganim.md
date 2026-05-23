# ganim Plan

`ganim` owns generic animation data and animator state. It does not own
game-specific collision boxes, entity semantics, renderer handles, or texture
loading.

`ganim` should use asset ids and sit naturally on top of `gassets`/`gmods`.
Animation loading should be able to resolve animation/image records through
`gassets`, while still keeping renderer and game metadata outside core.

## Responsibilities

1. Define animation ids, clip ids, frame ids, and image/asset references.
2. Store clips, frames, frame durations, source rectangles, draw offsets, pivot
   or origin data, loop mode, and tags.
3. Provide animator state: current clip, frame index, time, speed, pause, reset,
   randomize, and completion state.
4. Load generic animation files.
5. Load animation records from `gassets`.
6. Validate duplicate frames, bad durations, missing image refs, and empty clips.
7. Support tools like Span by preserving authored frame order.
8. Avoid runtime string lookups in animation stepping.
9. Store atlas/sprite-sheet frame rectangles or equivalent source rect data
   needed to draw a frame.

## Not Responsible

1. No texture decoding.
2. No renderer draw calls.
3. No Splonks `pbox`, `cbox`, tile behavior, entity mappings, or gameplay
   metadata semantics.
4. No particle simulation.
5. No state machine for gameplay animation selection.
6. No global animation manager singleton.

## Game Metadata Strategy

Game-specific metadata should be compiled into typed sidecar arrays.

Example:

```cpp
struct SplonksClipMeta {
    ganim::AnimId anim_id;
    std::vector<SplonksFrameMeta> frames;
};
```

The load step validates that sidecar frame order and count match the `ganim`
clip. Runtime code reads by frame index, not by string or generic metadata map.

Pivot/origin decision: keep generic draw offset, pivot/origin, center, frame
duration, source rect, loop mode, and frame tags in `ganim`. Keep collision
boxes, tile flags, entity mappings, and gameplay-specific emit points in
game-owned sidecar metadata.

## Relationship To Other Libraries

1. `gassets` provides records of type `animation`.
2. `ganim` consumes asset ids for animation and image references.
3. `gparticles` may depend on `ganim` and reference animations by id.
4. Span/editor output can feed `ganim` plus game-owned metadata sidecars.

## Implementation Steps

1. Extract minimal clip/frame/animator structs.
2. Add deterministic id hashing.
3. Add stepping and looping tests.
4. Add loader for a sexpr animation format.
5. Add sidecar validation examples using Splonks-shaped metadata in the demo.
6. Add `gassets` integration after `gassets` exists.
7. Add a one-way converter tool for old Splonks/Span YAML-ish annotations if it
   helps migration. Do not support the old format as a runtime load path.
8. Publish repo.

## Decisions

1. `ganim` is coupled to the asset stack for normal use. This is acceptable:
   animation assets should be easy to consume through `gassets`/`gmods`, not
   artificially decoupled.
2. Include atlas/source rectangle data in `ganim`.
3. Include pivot/origin/draw offset/center as generic animation drawing data.
4. Keep game-specific frame metadata in sidecar arrays.
5. Use both stable ids and dense frame indices. Runtime animation stepping uses
   dense indices; cross-system references and tools use ids.
6. Support fixed-fps clip defaults and per-frame duration overrides.
7. Do not support old Splonks YAML-ish annotations as a runtime format.

## Ambiguities

1. Should tags stay in runtime clip/frame structs or be stripped into
   editor/load-time metadata after validation?
