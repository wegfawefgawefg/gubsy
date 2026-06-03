# Async Matchmaking Backend Plan

## Goal

Gubsy room-server and matchmaking operations must never block the game frame. Remote
HTTP latency from `gubsy-roomd`, Steam, EOS, or future service backends should affect
only matchmaking state and UI status, not input, simulation, rendering, or menu
responsiveness.

The immediate symptom is the once-per-second hitch while publicly hosting through
`gubsy-roomd`. The likely cause is synchronous room heartbeat HTTP running from the
main update path. The proper fix is an asynchronous matchmaking backend, not longer
timeouts or less frequent heartbeats.

## Design Principles

- Main thread owns `EngineState` and all lobby state mutation.
- Worker thread owns blocking service I/O.
- Main thread submits requests and consumes completed results.
- No worker thread may call menu, runtime, lobby, or game callbacks directly.
- Request completion should be explicit: pending, success, failure, timeout,
  cancelled, or superseded.
- The API should remain game-general. Splonks should not know whether matchmaking is
  roomd HTTP, Steam, relay metadata, or another backend.

## Operations To Move Async

All `IMatchmaking` operations that can touch a remote service should move off the
frame thread:

- `create_room`
- `heartbeat_room`
- `list_rooms`
- `fetch_room`
- `create_join_attempt`
- `join_room`
- `leave_room`
- `remove_member`

## Proposed Architecture

Add an async matchmaking layer in Gubsy:

- `AsyncMatchmakingClient`
  - Owns a worker thread.
  - Accepts request structs from the main thread.
  - Runs the current `RoomServerMatchmaking` implementation on the worker.
  - Pushes result structs to a completion queue.

- `MatchmakingRequest`
  - Has request id, operation kind, room code/member ids/body payload, and deadline.
  - Carries enough immutable data for the worker to run without reading `EngineState`.

- `MatchmakingResult`
  - Has request id, operation kind, status, error string, and operation-specific data.
  - Is consumed only by the main thread during runtime update.

- `GubsyLobbyState`
  - Tracks pending operation ids and pending UI state.
  - Does not store worker-owned pointers or references.

## Main Thread Flow

Each frame:

1. Drain completed matchmaking results.
2. Apply results to `EngineState` and lobby state.
3. Schedule due work if no conflicting request is already in flight.
4. Continue frame normally.

The frame must not wait for request completion.

## Heartbeat Behavior

Heartbeat is the first critical path to fix because it fires continuously while hosting.

- If no heartbeat is in flight and `next_heartbeat_at` has passed, enqueue one.
- If a heartbeat is already in flight, do not enqueue another.
- On success, update room members/status and schedule the next heartbeat.
- On failure, record `last_error`, schedule retry/backoff, and optionally alert after
  repeated failures.
- Never disconnect or stall the game because a single heartbeat request is late.
- Host room state should remain locally valid while heartbeat is pending.

## Browser Behavior

- `list_rooms` should enqueue a refresh request.
- Browser displays cached rooms while refresh is pending.
- Refresh button sets a pending refresh status rather than blocking.
- Search filters cached rooms immediately.
- Failed refresh updates status/alert but keeps old cached results.

## Publish And Join Behavior

Publishing:

- Host transport starts locally.
- `create_room` is enqueued.
- UI shows publishing state until room creation completes.
- On success, lobby becomes public and stores room code/host secret/member id.
- On failure, local transport is stopped and UI reports publish failure.

Joining:

- Browser selection enqueues `create_join_attempt`.
- UI shows joining/preparing connection state.
- On success, Gubsy stores join attempt credentials and asks the game transport to
  connect through the selected cascade path.
- Once the game transport reports accepted/pending-sync, UI must stop saying
  "server not found" and move to sync/catchup state.
- Sync/catchup timeout belongs to the game transport, not matchmaking HTTP.

Leaving:

- Local session teardown can happen immediately.
- `leave_room` is enqueued best-effort if a room/member exists.
- A failed leave request should not keep the player stuck in a session locally.

## Cancellation And Superseding

Requests should be cancellable at the state-machine level even if the underlying HTTP
call cannot be interrupted:

- Store generation/request ids in lobby state.
- When a result arrives, apply it only if it matches the current expected request id.
- Ignore stale results from older screens, old rooms, previous join attempts, or a
  session that has already been left.

## Thread Safety

- Worker input structs are value copies.
- Worker output structs are value copies.
- Completion queue is guarded by a mutex or lock-free queue.
- `RoomServerMatchmaking` should either be worker-local or protected so it is not
  shared unsafely.
- `httplib::Client` objects should be created per request or owned only by the
  worker thread.

## API Shape

Keep the existing synchronous `IMatchmaking` useful for tests and simple tools, but do
not call it directly from the runtime hot path.

Possible new interfaces:

```cpp
enum class MatchmakingRequestKind {
    CreateRoom,
    HeartbeatRoom,
    ListRooms,
    FetchRoom,
    CreateJoinAttempt,
    JoinRoom,
    LeaveRoom,
    RemoveMember,
};

struct MatchmakingRequest {
    std::uint64_t id;
    MatchmakingRequestKind kind;
    std::string server_url;
    // operation-specific copied fields
};

struct MatchmakingResult {
    std::uint64_t id;
    MatchmakingRequestKind kind;
    bool ok;
    std::string error;
    // operation-specific copied results
};
```

## Implementation Phases

1. Add async infrastructure.
   - Worker thread lifecycle.
   - Request queue.
   - Completion queue.
   - Clean shutdown that joins the worker.

2. Move heartbeat async.
   - This directly targets the once-per-second hitch.
   - Add pending heartbeat state.
   - Add stale-result protection.

3. Move browser refresh async.
   - Convert `gubsy_lobby_refresh_rooms` to enqueue/poll behavior.
   - Keep cached room list visible during refresh.

4. Move publish async.
   - Add pending publish state.
   - Ensure failed publish tears down local transport cleanly.

5. Move join preparation async.
   - Async `create_join_attempt`, `fetch_room`, and `join_room` where applicable.
   - Preserve connection cascade semantics.

6. Move leave/kick/remove async.
   - Local leave remains immediate.
   - Remote cleanup becomes best-effort with stale result handling.

7. Remove remaining runtime hot-path synchronous HTTP calls.
   - Audit all uses of `RoomServerMatchmaking` from runtime/menu update paths.
   - Tools and command-line smokes may keep synchronous calls.

## Validation

- Public hosting against a remote VPS should show no once-per-second frame spike.
- `perf` debug output should keep `frame_total_ms` stable across heartbeat ticks.
- Host heartbeat can be delayed by artificial server latency without menu/gameplay hitching.
- Browser refresh should not freeze the menu.
- Publish/join UI should show pending state rather than hanging.
- Existing roomd smokes should pass.
- Add a focused smoke/fake backend test proving runtime update does not block while a
  matchmaking request is in flight.

## Success Criteria

- No blocking HTTP from `gubsy_update_runtime`, menu update, or per-frame lobby tick.
- Public hosting through `gubsy-roomd` does not introduce visible periodic hitching.
- Room browser and lobby status remain responsive while remote requests are pending.
- Splonks code does not need roomd-specific hacks for heartbeat, browser, or join.
