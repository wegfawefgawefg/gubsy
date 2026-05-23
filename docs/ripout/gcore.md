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
4. Diagnostics are worth standardizing if the next wave shares parser/manifest
   behavior. `gassets`, `gmods`, `ganim`, and `gparticles` will all report
   source-location errors, warnings, unknown fields, duplicate ids, missing
   dependencies, and invalid references.

## Diagnostics Shape

If `gcore` owns diagnostics, keep the type tiny:

1. Severity: info, warning, error.
2. Stable diagnostic code.
3. Human-readable message.
4. Optional source name/path.
5. Optional line and column.
6. Optional related id/key.

Do not make this a logging framework. A library should return diagnostics to the
caller; the caller decides whether to print, store, display in ImGui, or fail.

## Creation Guidance

1. If only one repo needs a type, keep it local.
2. If two repos copy the same id/hash/diagnostic/pool code, move the small shared
   piece into `gcore`.
3. Do not move broad helpers into `gcore` just because they are convenient.

## Remaining Questions

1. Should `gcore` be created before `gassets`, or should `gassets` start local
   and promote shared code once `gmods` needs the same pieces?
