# gparticles Plan

`gparticles` owns particle specs, emitters, simulation state, and renderer-neutral
draw commands. It may depend on `ganim`. It does not own textures, draw
batching, lighting backends, or game-world queries.

## Responsibilities

1. Define particle spec ids and emitter ids.
2. Store particle lifetime, spawn rate, velocity, acceleration, color, scale,
   rotation, sprite/animation refs, draw layer, and lighting mode.
3. Step particle systems deterministically when given `dt` and caller-provided
   random seeds.
4. Produce draw commands that a renderer can consume.
5. Support one-shot bursts and persistent emitters.
6. Support common sprite particles, ribbons, segmented sprites, and scripted/data
   animation-sequence variants.
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
5. Splonks does not currently use a general curve system. Its sprite particles
   use direct per-step velocity/acceleration for position, alpha, rotation, and
   scale. Start with that known shape instead of inventing a large curve editor.
6. Splonks scripted particles are data-driven animation sequences, not code
   callbacks. Keep `gparticles` scripted variants data-only unless a later game
   proves callback-driven particles are worth the extra policy.
7. SDL3 renderer helpers are acceptable at the edge. Core still emits
   renderer-neutral draw commands, but an SDL3 helper/demo can make the library
   easier to adopt because these projects are standardizing on SDL3.

## Splonks Feature Mapping

1. Sprite particles map to position, velocity, acceleration, alpha velocity,
   alpha acceleration, rotation velocity, rotation acceleration, scale velocity,
   and scale acceleration.
2. Scripted particles map to a list of animation sequence steps with animation
   id, playback mode, play count, and optional hold frames after the sequence.
3. Ribbon and segmented sprite particles should be ported after the basic draw
   command path is proven.
4. Draw layer and lighting mode are particle metadata. Rendering decides how to
   interpret them.
5. Game-world collision, camera culling, and world lighting queries stay outside
   core unless implemented as caller-provided data.

## Remaining Questions

1. Should the first implementation include ribbon and segmented sprite particles
   immediately, or should it land sprite/scripted first and then port the rest?
2. Should the SDL3 renderer helper live in the same repo as an optional target,
   or only in the demo/examples folder?
