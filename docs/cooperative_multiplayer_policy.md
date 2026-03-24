# Cooperative Multiplayer Policy

This project is aiming for cooperative multiplayer first, not rollback PvP netcode.

## Core Model

- Host-authoritative cooperative play.
- The host owns canonical world state.
- Clients should have no perceptible local control lag.
- Long-distance play should still feel viable for friendly co-op sessions.

## What We Are Not Optimizing For

- Deterministic lockstep.
- Rollback netcode.
- Fighting-game-style input reconciliation.
- Perfect competitive fairness under high latency.

## What We Are Optimizing For

- Immediate local controls.
- Stable join/host/lobby flow.
- State synchronization that tolerates latency.
- Graceful recovery when peers or content drift slightly.

## Mod Boundary

- Mods are part of the network contract.
- Sessions should advertise game version plus a mod signature/hash.
- The host decides the active gameplay-relevant mod set.
- Clients should sync to the host’s declared mod set before or during session entry.

## Live Mod Changes

Live mod changes during play are allowed as a design target.

That means:

- We accept that some swaps may require a resync or snapshot push.
- We accept that some removed mod content may leave orphaned entities or stale state behind.
- We prefer “recover and keep going” over rigid correctness if the alternative is blocking experimentation.

## Architectural Consequences

- Lobbies must expose version/mod identity, not just player counts.
- Networking should support snapshot/state replication, not just input relay.
- The room/join path should exist independently of Steam so the same model works on and off Steam.
- Steam integration should be a backend for invites, lobbies, and transport, not the entire multiplayer design.
