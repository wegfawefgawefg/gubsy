# gparticles Plan

`gparticles` owns particle specs, emitters, simulation state, and renderer-neutral
draw commands. It may depend on `ganim`. It does not own textures, draw
batching, lighting backends, or game-world queries.

## Responsibilities

1. Define particle spec ids and emitter ids.
2. Store particle lifetime, spawn rate, velocity, acceleration, color, scale,
   rotation, sprite/animation refs, and simple curves.
3. Step particle systems deterministically when given `dt` and caller-provided
   random seeds.
4. Produce draw commands that a renderer can consume.
5. Support one-shot bursts and persistent emitters.
6. Support common sprite particles, ribbons, segmented sprites, and scripted/data
   variants if they can stay renderer-neutral.
7. Load particle specs from sexpr files.
8. Load records from `gassets`.
9. Provide optional renderer helpers at the edge if they make the library much
   easier to use, but keep core draw commands renderer-neutral.

## Not Responsible

1. No texture loading.
2. No SDL/OpenGL/Vulkan renderer.
3. No camera ownership.
4. No stage/world collision queries in core.
5. No lighting backend.
6. No gameplay event system.
7. No global particle singleton.

## Relationship To Other Libraries

1. `gassets` can provide particle spec records.
2. `gparticles` can depend on `ganim` for animation ids and frame data.
3. The game or Gubsy resolves sprite/animation refs to renderer handles.
4. `gnetcode` should not replicate particles by default; deterministic gameplay
   should spawn effects from deterministic events, while presentation-only
   particles stay local.

## Implementation Steps

1. Audit Splonks particle files for renderer/game dependencies.
2. Define renderer-neutral particle spec and draw command structs.
3. Port basic sprite particle stepping first.
4. Add deterministic random helpers if needed.
5. Add ribbons/segmented variants only after basic specs are clean.
6. Add demo that prints/dumps draw commands or uses a tiny SDL edge demo later.
7. Add `gassets` integration.
8. Publish repo.

## Decisions

1. Do not treat particles as gameplay state by default. Splonks uses them as
   presentation; netcode should not replicate them unless a game deliberately
   makes particles authoritative.
2. Leave lighting backend/rendering renderer-side. Particle draw commands can
   carry brightness/tint/light-intent fields, but the renderer decides what they
   mean.
3. World collision and camera/culling should be caller-side by default. Core does
   not know the world or camera.
4. Existing Splonks particle feature set includes sprite particles, scripted
   sequences, ribbons, segmented sprites, lighting mode, draw layer, animation
   refs, velocity/acceleration, alpha, rotation, scale, and color tint. The
   extracted library should cover that shape cleanly.

## Ambiguities

1. Should curves be built in, or should specs use simple start/end interpolation?
2. Should scripted particles be data-only scripts or
   caller callbacks?
3. Should optional renderer helpers target SDL3 first, or stay as examples only?
