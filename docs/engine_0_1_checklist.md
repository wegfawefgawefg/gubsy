# Engine 0.1 Checklist

This is the practical checklist for calling Gubsy a credible `0.1` engine.

It is not a “someday everything” roadmap. It is the shorter list of what still
needs to exist, or needs to stop being fragile, before the engine feels solid
to build on.

## Current Baseline

Gubsy already has:

- engine/runtime code split from a bundled game layer
- rendering, input, audio, and menu framework
- profile/settings persistence
- Lua-based mod loading and mod menu groundwork
- lobby/session browser flow
- non-Steam multiplayer path
- host-authoritative sync with prediction/replay/correction hooks
- headless multiplayer smoke coverage

That means the project is already past “toy prototype.”

The missing work is mostly about:

- hardening
- clearer engine boundaries
- protocol maturity
- better tooling
- stronger content/session contracts

## Must Have

These are the things that should exist before calling the engine solid enough
for outside use or for serious internal game work.

### 1. Multiplayer Session Hardening

- clean disconnect / timeout handling
- reconnect behavior or explicit reconnect policy
- session error UX that does not quietly fall back into bad local behavior
- explicit host migration policy, even if the first answer is “host migration is not supported”
- better latency behavior under real RTT, not just local testing

Current implemented floor:

- snapshot timeout status exists
- room-service reconnect attempts exist
- room closure now exits the session cleanly
- host migration is explicitly unsupported

Still missing:

- stronger reconnect behavior for the realtime transport itself
- richer in-game diagnostics around who dropped and why

### 2. Stronger Engine/Game Sync Boundary

- keep engine ownership of transport, session lifecycle, sequencing, acking, replay, and correction hooks
- keep game ownership of input capture, prediction rules, snapshot capture, snapshot apply, and presentation policy
- reduce demo-shaped assumptions in the sync path over time
- make game-defined lobby config and runtime session metadata feel like a normal engine extension point

### 3. Version / Protocol / Mod Contract Enforcement

- gate sessions on `game_version`
- gate sessions on explicit `net_proto`
- gate sessions on mod signature / required content set
- refuse or delay session entry when the client is not actually compatible
- make the host-declared content contract explicit instead of implied

### 4. Transport / Protocol Cleanup

- keep the room server as directory/bootstrap only
- keep realtime sync on the separate transport path
- move from ad hoc JSON datagrams toward a more explicit packet/message schema
- support reliable and unreliable message classes cleanly
- make packet compatibility/versioning part of the engine contract

### 5. Tooling That Makes Development Less Blind

- better headless multiplayer scenarios
- scripted latency / packet loss simulation
- basic in-engine sync/debug overlay
- clear logging for session lifecycle and transport failures
- simple profiling hooks for update/render/network timing

## Should Have

These are not “ship is impossible without them,” but they matter a lot for the
engine feeling mature.

### 1. Mod Lifecycle Maturity

- explicit mod capability / compatibility checks
- better dependency handling
- clearer policy for live mod mutation during play
- better failure behavior when a mod reload only partially succeeds

### 2. Session UX Maturity

- better in-game session menu / leave flow
- clearer browser state and room diagnostics
- better host controls for session config, players, and content
- cleaner join-mid-lobby versus join-mid-game behavior

### 3. Persistence and Recovery

- save/load story for games that want it
- engine-level snapshot/debug save support
- config/session recovery after crashes or hard exits

### 4. Better Example Game Coverage

- keep the bundled game as a proof case, not as the engine’s center of gravity
- add just enough example behavior to prove the engine APIs are good
- avoid growing the demo in ways that hide engine API problems

## Nice To Have

These are valuable, but they should not block the engine from being called a
real first release.

- Steam backend for invites, lobbies, and transport
- achievements / cloud hooks
- richer debug visualizers and editor tools
- localization support
- stronger asset pipeline and packaging helpers
- additional example games or samples

## Working Definition Of “0.1”

Gubsy should count as a `0.1` engine when:

- a small game can define its own lobby/config/sync behavior without fighting engine assumptions
- local and online session flow are both reliable enough for normal use
- transport and session failures are understandable and recoverable
- mod/version/network compatibility is enforced instead of hand-waved
- the engine has enough tooling to debug sync, session, and content problems quickly

## Not The Goal

This checklist is not asking Gubsy to become:

- a big editor-first engine
- a fully opinionated ECS framework
- rollback PvP netcode
- a giant general-purpose middleware stack

The goal is a strong code-first engine with good multiplayer, modding, and
session foundations.
