# gcore Plan

`gcore` is the likely tiny common-types library for the next wave. It should be
small and boring: ids, hashing, stable handles, small pools, and maybe common
diagnostics. It must not become a utility junk drawer.

## Possible Responsibilities

1. Stable id/hash helpers.
2. Small handle/pool helpers.
3. Tiny math types if several repos need the same vectors/rects. Prefer
   generalized local types over reusing `glayout` rects outside layout.
4. Direction/side enums if repeated.
5. Common diagnostic structs if every library converges on the same shape.

## Not Responsible

1. No engine framework.
2. No large utility grab bag.
3. No filesystem policy.
4. No logging framework.
5. No dependency on SDL, ImGui, Lua, or renderer code.

## Creation Rule

Create `gcore` when implementing the next repos if `gassets`, `gmods`, `ganim`,
or `gparticles` need the same id/hash/handle/pool helpers.

## Decisions

1. `gcore` should probably exist for hashing, ids, vids/stable handles, handle
   pools, diagnostics, and small math if the next repos share them.
2. Generalize tiny vector/rect types instead of using `glayout` types outside
   layout.
3. Keep the anti-junk-drawer rule: a helper enters `gcore` only when at least
   two repos need the same concrete type or function.

## Ambiguities

1. Should diagnostics be standardized across all g-libs immediately, or only
   after the first two new repos show the same shape?
