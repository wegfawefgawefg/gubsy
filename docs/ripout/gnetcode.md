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
2. `gcontent` may help ensure peers have compatible content, but `gnetcode` does
   not install content.
3. `gassets`/`gcontent` hashes can be included in session compatibility checks.
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

## Ambiguities

1. Should packet encoding be fixed binary structs, schema-driven, or caller-owned?
2. What exact callback shape should game-authored snapshots use?
3. What is the smallest generic lobby/session state that still avoids every game
   rewriting join/leave/barrier/session code?
