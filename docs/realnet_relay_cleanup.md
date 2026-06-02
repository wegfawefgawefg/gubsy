# Realnet Relay Cleanup Plan

This plan is the cleanup and hardening pass after the first working Realnet
relay milestone.

The current relay path is real:

1. `gubsy-roomd` advertises `realnet.relay_udp`.
2. Room join attempts allocate relay credentials.
3. Host and joiner authenticate with signed relay packets.
4. `relay_data` forwards opaque binary payloads.
5. Splonks consumes relay as a transport wrapper without changing lockstep
   packet schemas.
6. Local direct, forced NAT punch, local forced relay, and
   desktop/laptop/VPS forced relay validation have passed.

This document is not a replacement for `realnet_relayd_plan.md`. It is the
next pass: remove rough edges, make operating the relay safer, and make the
implementation cleaner for future Gubsy games.

## Current Shape

Relay currently runs inside `gubsy-roomd`. That is acceptable for the first
working milestone because room state and relay allocations live together, but
the code should keep service responsibilities explicit:

```text
roomd:
  room directory
  join attempts
  relay allocation authority

relayd:
  authenticated UDP relay endpoint
  endpoint binding
  opaque datagram forwarding
  rate/size/idle enforcement
  relay diagnostics
```

The first implementation is clean enough to use for Splonks development and
real internet validation. It is not yet production-hardened relay
infrastructure.

## Cleanup Goals

1. Keep relay game-general.
   - Relay must never parse Splonks packets.
   - Relay must not know about players, entities, lockstep, rollback, stages,
     or game-specific schemas.
   - The game-facing contract stays: send opaque bytes to a selected Realnet
     transport.

2. Make relay service boundaries explicit.
   - Extract relay registry/service code out of `tools/room_server/main.cpp`
     into Gubsy-owned Realnet modules.
   - Keep the initial deployed binary as `gubsy-roomd` if convenient.
   - Make a future `gubsy-relayd` binary possible without changing the
     game-facing API.
   - Keep HTTP room APIs and UDP relay handling from becoming one tangled
     implementation.

3. Finish abuse controls.
   - Add maximum active allocations.
   - Add per-room active allocation limits.
   - Add per-source auth failure counters.
   - Add short temporary source bans after repeated auth failures.
   - Add counters for packets rejected by auth, size, allocation lookup,
     endpoint mismatch, timeout, and rate limit.
   - Make all limits configurable through Realnet config and roomd/relayd CLI.

4. Add replay and duplicate protection.
   - Track recent control packet sequences per role/allocation.
   - Reject stale duplicate `relay_hello`, `relay_keepalive`, `relay_close`,
     and `relay_error` packets.
   - Decide whether `relay_data` duplicate rejection is useful or harmful for
     game UDP traffic.
   - Do not impose reliable ordering on game payloads. Relay remains a UDP
     datagram forwarder.

5. Tighten endpoint binding.
   - Continue trusting only observed UDP source endpoints.
   - Reject relay data from an endpoint that changes after binding.
   - Log `relay_endpoint_changed` with role, room, allocation, old endpoint,
     and new endpoint.
   - Decide whether a controlled rebinding path is needed for mobile networks.
     If it is added, it must be explicit and authenticated.

6. Make allocation lifecycle exact.
   - Allocation TTL applies before both sides authenticate.
   - Idle timeout applies after both sides are ready.
   - `relay_close` should close the allocation and increment close counters.
   - Room deletion should close associated allocations and log why.
   - Host leave should close relay allocations for that room.
   - Joiner leave should close only that joiner's allocation.

7. Improve config and capability reporting.
   - Advertise active relay limits in `/health`.
   - Add CLI/env support for all relay limits:
     - UDP bind host and port
     - public advertised host and port
     - region id
     - max packet bytes
     - allocation TTL
     - idle timeout
     - max active allocations
     - per-IP packet rate and burst
     - per-room packet rate and burst
     - auth failure ban threshold and duration
   - Keep capability naming service-oriented: `relay_udp`, not game-specific
     labels.

8. Improve diagnostics.
   - Keep `/debug/realnet/relays`.
   - Keep `/debug/realnet/relays/:allocation_id`.
   - Add per-room relay summaries to `/debug/realnet/rooms/:room_code`.
   - Add structured timeline fields for:
     - relay candidate generated
     - relay candidate skipped
     - relay candidate tried
     - relay allocation created
     - host hello
     - joiner hello
     - relay ready
     - first data from host
     - first data from joiner
     - close
     - idle timeout
     - auth failure
     - rate limit
   - Make debug output useful without dumping game payloads.

9. Make Splonks relay adapter less magic.
   - Keep synthetic endpoints internal to Splonks transport bookkeeping.
   - Document why `realnet-relay:<port>` exists.
   - Consider replacing the string sentinel with an explicit endpoint kind in
     Splonks transport state.
   - Move the relay peer timeout from a hardcoded Splonks constant to a
     Realnet-derived or configurable value.
   - Keep direct/NAT behavior unchanged.

10. Make validation precise.
    - Keep local forced-relay validation.
    - Keep local direct and forced NAT punch validation.
    - Keep desktop/laptop/VPS forced-relay validation.
    - Add a smaller relay transport smoke that validates allocation, ready,
      bidirectional relay data, and clean close without requiring full gameplay.
    - Keep full Splonks gameplay smoke for end-to-end confidence.
    - Add a relay idle-expiry smoke.
    - Add a relay auth-failure smoke.
    - Add a relay packet-too-large smoke.
    - Add a relay endpoint-change smoke.
    - Add a max-allocation smoke once limits exist.

## Implementation Order

### Phase 1: Service Extraction

1. Move relay packet handling, allocation lookup, endpoint binding, and
   counters into a Gubsy Realnet relay service module.
2. Keep roomd as the owner of allocation creation.
3. Pass allocation data to the relay service through a small internal API.
4. Keep the public room/join HTTP response shape unchanged.

Success criteria:

```text
./scripts/build.sh
ctest --test-dir build --output-on-failure -R "room_relay|realnet_connection"
```

### Phase 2: Limits And Abuse Controls

1. Add max active allocation limits.
2. Add auth failure counters.
3. Add temporary source bans.
4. Add endpoint-change rejection.
5. Add structured reject counters by reason.
6. Expose all values in health/debug output.

Success criteria:

```text
ctest --test-dir build --output-on-failure -R "room_relay"
./scripts/room_relay_smoke.sh
```

### Phase 3: Lifecycle Cleanup

1. Close allocations on `relay_close`.
2. Close allocations on host leave.
3. Close allocations on room deletion.
4. Expire authenticated but idle allocations.
5. Log close reasons.

Success criteria:

```text
ctest --test-dir build --output-on-failure -R "relay"
```

### Phase 4: Splonks Adapter Cleanup

1. Document or replace synthetic relay endpoints.
2. Make relay timeout policy explicit and transport-aware.
3. Verify direct and NAT punch behavior stays unchanged.
4. Keep Splonks lockstep packet schemas untouched.

Success criteria:

```text
./scripts/build.sh
./scripts/validate_gubsy_roomd_live.sh
SPLONKS_REALNET_FORCE_NAT_PUNCH=1 ./scripts/validate_gubsy_roomd_live.sh
./scripts/validate_realnet_relay_live.sh
```

### Phase 5: Remote Validation

1. Deploy updated Gubsy roomd/relay service to the VPS.
2. Rebuild Splonks on desktop and laptop.
3. Run forced relay from desktop host to laptop client through the VPS.
4. Check relay debug output for ready allocation, bidirectional bytes, no active
   auth failures, and no active-path rate-limit drops.

Success criteria:

```text
REALNET_LAN_HOST_OK
REALNET_LAN_CLIENT_OK
```

## What Not To Do

1. Do not make relay parse game packets.
2. Do not make relay reliable or ordered by default.
3. Do not add Splonks-specific branches to Gubsy relay code.
4. Do not make Steam required for non-Steam public play.
5. Do not hide relay failures behind generic "connection failed" messages.
6. Do not remove direct IP or direct LAN developer paths.

## Open Questions

1. Should endpoint rebinding be supported for mobile networks, or should a
   changed endpoint always require reconnect?
2. Should relay allocations be created eagerly with every join attempt, or only
   after direct/NAT candidates fail?
3. What should the default max active allocation count be for a small VPS?
4. Should per-room byte rate limits be separate from packet rate limits?
5. Should relay debug endpoints remain localhost-only forever, or should a
   signed admin token be added for remote operations?

## Recommended Next Goal

```text
Implement the Realnet relay cleanup pass: extract relay service code from roomd
into Gubsy Realnet modules, add allocation caps, auth-failure bans, endpoint
change rejection, replay checks for control packets, precise close/idle
lifecycle handling, richer relay diagnostics, and Splonks relay adapter cleanup
while keeping direct, NAT punch, local relay, and desktop/laptop/VPS forced
relay validation passing.
```
