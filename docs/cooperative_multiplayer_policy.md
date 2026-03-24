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

## Current Implementation Direction

- The first playable online slice is host-authoritative snapshot sync.
- Clients should locally predict their own movement/input-facing state between host snapshots.
- The room service currently handles directory/bootstrap duties only.
- Realtime sync currently runs over a separate UDP transport path.
- The room/bootstrap service should stay replaceable without changing the higher-level session contract.

## Replay Semantics

- After a client receives an authoritative host snapshot, it prunes any local inputs the host has already acknowledged.
- The client then reapplies the remaining unacknowledged local inputs, in order, on top of the new authoritative state.
- That replay step is what keeps local controls responsive instead of waiting for the next round-trip.
- The engine owns the sequencing, ack tracking, and replay loop.
- The game owns the actual prediction rules and how corrected state should be presented.

## Correction Policy

- Authoritative state should replace simulation truth immediately.
- Presentation does not need to snap immediately.
- The engine should expose explicit reconciliation hooks around snapshot apply and replay.
- Games can use those hooks to smooth remote corrections, snap large teleports, or keep the local player unsmoothed.
- Smoothing is a view policy, not a simulation rule.

## Mod Boundary

- Mods are part of the network contract.
- Sessions should advertise game version plus a mod signature/hash.
- Sessions should carry a content revision so live host-side content changes are observable.
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
- Lobbies must expose a session contract, not just a bag of unrelated room fields.
- Networking should support snapshot/state replication, not just input relay.
- The room/join path should exist independently of Steam so the same model works on and off Steam.
- Steam integration should be a backend for invites, lobbies, and transport, not the entire multiplayer design.
- The first validation harness should be headless and compare client-applied state against host-authoritative state.
- The engine should own generic session orchestration, matchmaking/transport backends, compatibility checks, and reconciliation hooks.
- Individual games should provide their own sync driver for input capture, local prediction, snapshot capture, and snapshot application.

## Engine Boundary

Gubsy should not hard-code one game’s world state into the engine-level sync flow.

That means:

- The engine can define session/sync interfaces and lifecycle.
- The engine can provide reusable room/lobby/browser plumbing.
- The engine should not assume `DemoPlayer`, `BonkTarget`, or any other particular game state.
- A game should plug in code-first sync behavior rather than inherit a one-size-fits-all gameplay netcode layer.

## Current Limitation

- The current transport still uses JSON payloads for smoke-friendly validation.
- That is a backend detail, not the long-term transport goal.
- If the JSON payload boundary starts getting in the way, it should be replaced with an opaque packet/byte payload interface.
