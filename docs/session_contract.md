# Session Contract

This document describes the engine-level session contract used by matchmaking,
runtime sync, and future backends such as Steam.

## Purpose

The session contract is the shared description of a running or joinable session.

It exists so the engine can answer:

- can this client join this session at all?
- what transport endpoint should the client connect to?
- what gameplay-relevant content identity is the host currently running?
- has the host changed that content contract while the session is live?

## Current Fields

- `game_version`
- `net_protocol`
- `session_phase`
- `mod_hash`
- `content_revision`
- `allow_live_mod_reload`
- `realtime_endpoint`
- `game_config`

## Field Meaning

### `game_version`

The coarse game/content version gate.

If this differs, the session should generally be treated as incompatible.

### `net_protocol`

The network protocol compatibility version.

This should change when packet/schema/session assumptions change in a way that
older clients should not try to interpret.

### `session_phase`

The high-level lifecycle phase for the session.

Current expected values:

- `lobby`
- `in_game`

### `mod_hash`

The host-declared gameplay-relevant mod/content identity.

This is part of the multiplayer contract, not just a cosmetic label.

### `content_revision`

The monotonic revision for the current gameplay/content contract.

This is the field that makes live host-side mod changes tolerable.

If the host changes the gameplay-relevant mod set or other contract-shaping
game config during a live session, this revision should advance.

Clients do not need to be kicked immediately when this changes.

Instead, the engine can:

- notice that the contract changed
- reset or resync runtime sync state
- let the game decide how much stale/orphaned state is acceptable

### `allow_live_mod_reload`

Signals that the host is willing to let the session continue across live
content changes instead of treating them as a hard stop.

This does not guarantee perfect cleanup.

It means “prefer resync and keep going” rather than “disconnect everyone.”

### `realtime_endpoint`

The transport bootstrap endpoint for the real-time sync path.

The room/directory service can advertise this without carrying the actual
runtime traffic itself.

### `game_config`

Game-defined lobby/run configuration blob.

The engine treats this as opaque structured data.

## Engine Boundary

The engine should own:

- session contract definition
- compatibility checks
- matchmaking/backend plumbing
- transport/backend plumbing
- connection reset/resync when the contract changes

The game should own:

- what fields inside `game_config` mean
- how to present/edit those settings
- how to react when a new contract revision lands during play

## Current Backend Mapping

Right now:

- `IMatchmaking` is backed by the room server
- `INetTransport` is backed by the UDP realtime transport

That means Steam can later replace one or both backends without changing the
high-level session contract model.
