# Networking Boundary

This document describes the intended engine/game split for multiplayer.

The short version:

- the engine should own session lifecycle, matchmaking, transport, and generic wire helpers
- the game should own its own message schema, authority rules, reconciliation policy, and serialization
- higher-level sync loops are allowed, but they should be optional helpers, not the only engine path

## Engine Responsibilities

The engine should provide the boring reusable parts:

- room/session discovery and join flow
- session contract and compatibility checks
- transport bootstrap and realtime endpoint plumbing
- raw packet send/receive
- reliable vs unreliable transport helpers over time
- sequencing, ack helpers, clocks, and latency simulation hooks
- optional debug overlays and diagnostics

These are engine problems because every game built on Gubsy needs some form of
them.

## Game Responsibilities

The game should decide the actual multiplayer model:

- what messages exist
- what bytes mean on the wire
- what state is authoritative
- what is predicted locally
- what is smoothed
- what conflicts are forgiven instead of hard-corrected
- what should be reliable, lossy, ordered, or best-effort

These are game design decisions, even when the engine can offer helper
utilities.

## Optional Helper Policy

Gubsy can still ship optional higher-level helpers.

Examples:

- host-authoritative snapshot sync
- command/event replication helpers
- interpolation buffers
- correction/smoothing utilities

But these should be framed as optional building blocks.

The engine should not force every game into:

- one authority model
- one snapshot format
- one command format
- one conflict-resolution policy

## Current Direction

The current bundled game still uses an optional host-authoritative sync helper.

That is acceptable as a sample and as a reusable utility, but it is not the
core engine boundary.

The important engine-facing change is:

- realtime transport packets are opaque byte payloads
- the sync helper driver now exchanges opaque byte payloads
- the demo game chooses its own payload encoding on top of that

Right now the bundled game still uses CBOR-wrapped JSON for its own sync
payloads. That is a game-layer choice, not an engine requirement.

## Practical Rule

When adding new multiplayer code:

- put transport/session/backend code in `src/`
- put game message definitions and serializers in `demo/`
- only add engine abstractions that are useful to more than one game
- prefer minimal explicit hooks over a large mandatory netcode framework

## What The Engine Should Not Assume

The engine should not assume:

- every game uses snapshots
- every game uses host authority for every action
- every game wants hard rollback or hard correction
- every replicated payload is structured JSON
- every game wants the same recovery policy when conflicts happen

Those are all game-level concerns.
