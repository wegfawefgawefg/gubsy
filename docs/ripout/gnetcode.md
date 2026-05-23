# gnetcode Plan

`gnetcode` is the proposed lockstep/rollback/session protocol library. The
current best source is Splonks' input lockstep experiment. The goal is reusable
netcode building blocks, not a full game networking framework.

This should be a real game-agnostic form of Splonks' working netcode, including
UDP-capable transport at the edge. Do not make this a transport-less placeholder
that cannot run a game session.

## Responsibilities

1. Define peer ids, player ids, session ids, frame ids, and stage/epoch ids.
2. Store input records by player/frame.
3. Support fixed input delay.
4. Support prediction and late-input mismatch detection.
5. Support rollback scheduling and snapshot retention policy.
6. Encode/decode transport-neutral packet payloads.
7. Provide UDP transport helpers/adapters.
8. Track ping, jitter, suggested delay, and scheduled deterministic setting
   changes.
9. Provide session/join/leave state machines if they can remain game-neutral.
10. Provide fuzzer/headless tests for delay, jitter, reorder, and packet loss.
11. Keep gameplay unaware of networking.
12. Support generic barriers/epochs for joins, level changes, shops,
    between-level screens, or other synchronization pauses.
13. Support content compatibility hashes as first-class session metadata.

## Not Responsible

1. No game entity replication.
2. No Splonks stage-transition rules in core.
3. No matchmaking server.
4. No rendering, audio, or presentation smoothing in core.
5. No serialization of arbitrary game state beyond caller-provided snapshots.
6. No Splonks-specific loading-screen/stage-transition hacks.

## Relationship To Other Libraries

1. `ginput` may define input profiles, but `gnetcode` should use caller-provided
   packed input records.
2. `gmods` may help ensure peers have compatible content, but `gnetcode` does
   not install content.
3. `gassets`/`gmods` hashes can be included in session compatibility checks.
4. Games provide their own snapshot structs plus save/restore and deterministic
   step callbacks.

## Implementation Steps

1. Extract Splonks lockstep input buffer into generic structs.
2. Extract packet codec.
3. Extract scheduled settings changes.
4. Define game-authored snapshot interface.
5. Add rollback ring policy without knowing game state shape.
6. Add headless deterministic smoke tests.
7. Add UDP transport helpers/adapters.
8. Decide whether lobby/session state belongs in this repo or a companion layer.
9. Publish repo.

## Decisions

1. Repo name is `gnetcode`.
2. Include UDP-capable transport at the edge.
3. Join/lobby/session state belongs in `gnetcode` if it can stay generic.
4. Snapshot data is game-authored. Splonks currently stores rollback snapshots
   as `shared_ptr<GameplaySnapshot>` and serializes gameplay snapshots to bytes
   for resync. The library should generalize that into caller-owned snapshot
   capture/restore/serialize hooks rather than owning game state.
5. Content compatibility hashes should be first-class, but cached and explicit
   so runtime hashing cost is controlled.
6. Barriers should be generic epochs/barrier state, not Splonks stage-specific
   hacks.
7. Input prediction belongs in core. Presentation/render prediction is
   caller-provided and game-specific.
8. Built-in protocol packets should use fixed binary structs with explicit
   versioning, sizes, and endian policy. Splonks already uses fixed binary packet
   structs for join, ping, input frames, hashes, snapshot chunks, barriers, and
   resume messages. Keep that model, but make game payload bytes caller-owned.
9. Input records should carry caller-provided packed input bytes or fixed small
   words. `gnetcode` should not know Splonks button flags.
10. Snapshot storage is caller-owned through hooks. Core owns retention policy
    and when to capture/restore/serialize, not the game snapshot type.
11. Generic lobby/session state belongs in `gnetcode`: peers, assigned players,
    room/session id, role, session contract, content hash, latency stats, join
    barriers, and connection state. Game-specific screen flow, scenario choices,
    menus, and stage semantics stay caller-side.

## Packet Encoding

Splonks currently uses fixed binary packet structs copied into packet payloads.
That is a good fit for `gnetcode` because lockstep packets are small, stable,
and performance-sensitive.

1. Use explicit packet headers: magic, protocol version, packet type, and payload
   byte count.
2. Use fixed binary structs for built-in protocol packets.
3. Use caller-owned byte spans for game input payloads and snapshot chunks.
4. Validate every decode with payload size, protocol version, and range checks.
5. Keep schema systems out of the core unless a real cross-language requirement
   appears.

## Snapshot Hook Shape

The game owns the snapshot type. `gnetcode` owns when snapshots are captured,
retained, restored, serialized, and pruned.

Required hook shape:

1. `capture(frame, user) -> SnapshotHandle`
2. `restore(frame, handle, user) -> bool`
3. `destroy(handle, user)`
4. `serialize(handle, output_bytes, user) -> bool`
5. `deserialize(input_bytes, user) -> SnapshotHandle`
6. `step(frame, input_batch, replay_mode, user) -> bool`
7. `hash(frame, user) -> uint64_t`

The handle can wrap a caller pointer, index, or `shared_ptr`-like object, but the
core should not depend on the concrete snapshot struct.

## Generic Session State

Gubsy already has session contracts with game version, net protocol, mod hash,
required mod ids, content revision, and optional realtime endpoint data. Splonks
has peer roles, lockstep settings, hash history, rollback snapshots, join
barriers, snapshot resync chunks, ping/jitter, and packet transport state.

The reusable state should include:

1. Session id and protocol version.
2. Peer ids and player assignments.
3. Host/client role.
4. Session compatibility contract.
5. Content hash and required package/mod ids.
6. Lockstep delay and scheduled deterministic settings.
7. Ping, jitter, and timeout tracking.
8. Join/leave state.
9. Generic barriers for level loads, shops, lobby ready gates, or other sync
   pauses.
10. Snapshot resync transfer state.

It should not include Splonks entities, tools, effects, stage transition hacks,
or Gubsy menu state.

## Remaining Questions

1. Should the caller-provided input payload be a fixed maximum byte array, a
   `uint64_t` fast path plus optional bytes, or always a byte span?
2. Should UDP transport live in the main target or an optional `gnetcode_sdl3`
   or `gnetcode_udp` edge target?
