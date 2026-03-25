# Code-First Engine Intent

This document describes what Gubsy is supposed to be.

It is not meant to be a highly opinionated game framework.

It is meant to be a code-first engine/runtime that removes infrastructure work
so game code can stay focused on game rules.

## Primary Goal

Using Gubsy should feel like writing a normal game in C++.

The engine should help with:

- windowing
- input
- audio
- menu flow
- settings and profile persistence
- saving/loading infrastructure
- session/lobby/bootstrap flow
- networking transport and backend plumbing
- Steam/backend integration
- mod loading, mod browsing, and mod install/runtime boundaries
- debug tooling and diagnostics

The engine should not force one gameplay architecture.

## What The Engine Should Not Own

The engine should not own:

- entity model
- world state layout
- gameplay rules
- AI model
- authoritative conflict policy
- save schema meaning
- sync message schema
- asset metadata meaning

Those belong to the game.

## Rendering Boundary

Rendering infrastructure can live in the engine.

That includes things like:

- renderer/device setup
- texture loading
- font loading
- sprite/image decode
- draw helpers
- hot reload hooks

But what gets drawn, when, and why is mostly game-side.

That means:

- camera rules are game-side
- sprite layering is game-side
- animation rules are game-side
- gameplay-facing render semantics are game-side

## Asset Boundary

The engine can provide asset transport and loading primitives.

Examples:

- image decode
- audio decode
- asset cache
- file watching
- sidecar metadata file loading
- generic JSON/blob loading helpers

But the game should own what metadata means.

For example, sprite metadata may contain:

- hurtboxes
- hitboxes
- mouth points
- grab sockets
- centers
- effect anchors

Or none of those.

That meaning should stay in game code.

## Networking Boundary

The engine should provide:

- session membership
- lobby/bootstrap flow
- transport channels
- packet send/receive
- sequencing and reliability helpers over time
- backend integration like Steam or custom room services

The game should provide:

- its own message schema
- its own serialization
- its own authority model
- its own reconciliation policy

The engine may ship optional helper layers, but those should be helpers, not
the required architecture.

## Mods Boundary

The engine should provide:

- discovery
- install/update
- activation/deactivation
- runtime API registration hooks
- mod session/content contract plumbing

The game should provide:

- the actual mod API surface it exposes
- the content types it understands
- any gameplay-side migration/resync rules

## Reflection And Dynamic Types

Gubsy does not need a giant universal reflection system by default.

Most boundaries can stay explicit through:

- normal C++ calls
- registries
- explicit serializers
- opaque bytes/handles
- game-owned parsing code

Only add heavier runtime type/reflection systems when they solve a concrete
problem, such as native hot reload, generic editor inspection, or true binary
plugin ABI requirements.

## Repo Structure Intent

The repo should reflect the ownership boundary.

That means:

- engine code should build as an engine library target
- bundled game code should build as a separate executable target on top of that
- tools should stay separate tools
- sample-game assets and data should eventually live under a sample/app area,
  not implicitly under the engine root

The first restructuring step is the build split.

The later restructuring step is moving sample-owned runtime data, mods, and
assets into a clearer sample/app layout.
