# Realnet Relayd Plan

This document specifies the next Realnet transport phase: `gubsy-relayd`.
Relayd is the guaranteed fallback for rooms where direct UDP and NAT punch do
not produce a usable path.

Realnet has already proven:

```text
roomd room -> join attempt -> direct candidate or punch_udp -> game UDP packets
```

The relay phase extends that cascade:

```text
loopback -> LAN direct -> public direct -> punch_udp -> relay_udp
```

The relay must be reusable for future Gubsy games. It must not know about
Splonks lockstep, rollback, entities, players, stages, or packet schemas. It
only authorizes endpoints and forwards opaque datagrams.

## Target Outcome

After this phase:

1. `gubsy-roomd` advertises relay availability through `realnet.relay_udp`.
2. A room join attempt can allocate a relay session when direct and NAT punch
   are unavailable or fail.
3. Host and joiner authenticate to relay using room/join-attempt credentials.
4. Relayd forwards opaque UDP datagrams between authorized endpoints.
5. The game sees the same packet transport shape regardless of selected
   transport.
6. Gubsy diagnostics show why relay was selected and how it performed.
7. Splonks can join a public room with no port forwarding even when NAT punch
   fails.

## Non-Goals

This phase does not:

1. Add Steam transport.
2. Add peer-to-peer mesh.
3. Add TCP, WebSocket, WebRTC, or browser-hosted transports.
4. Make `gubsy-roomd` run game simulations.
5. Parse or transform game payloads.
6. Implement a dedicated Splonks game server.
7. Replace the existing direct IP developer path.

## Service Responsibilities

The responsibilities should stay distinct even if development starts with one
deployed process.

```text
gubsy-roomd
  HTTP room directory
  room lifecycle
  join attempts
  service capability discovery
  relay allocation authority

gubsy-punchd
  UDP endpoint observation
  NAT punch endpoint hints
  punch diagnostics

gubsy-relayd
  UDP relay allocations
  endpoint authorization
  opaque datagram forwarding
  bandwidth/rate/accounting limits
  relay diagnostics
```

Development can initially ship relay code inside `gubsy-roomd` or as a sibling
tool target. The code should still be structured as a relay service module so
it can become a separate `gubsy-relayd` binary without changing the game-facing
API.

## Capability Discovery

Roomd should advertise relay availability in the existing health/capability
shape:

```json
{
  "ok": true,
  "realnet": {
    "room_directory": {
      "enabled": true,
      "protocol": "gubsy-roomd-v1"
    },
    "punch_udp": {
      "enabled": true,
      "host": "example.com",
      "port": 8789,
      "protocol": "gubsy-punch-v1"
    },
    "relay_udp": {
      "enabled": true,
      "host": "example.com",
      "port": 8790,
      "protocol": "gubsy-relay-v1"
    }
  }
}
```

Config names should describe service roles:

```text
GUBSY_RELAYD_UDP_HOST=0.0.0.0
GUBSY_RELAYD_UDP_PORT=8790
GUBSY_RELAYD_PUBLIC_HOST=example.com
GUBSY_RELAYD_REGION=tokyo
GUBSY_RELAYD_MAX_PACKET_BYTES=1400
GUBSY_RELAYD_ALLOCATION_TTL_MS=30000
GUBSY_RELAYD_IDLE_TIMEOUT_MS=10000
```

The HTTP response may use the room server host as a fallback public host when a
dedicated relay public host is not configured, but the advertised capability
must still be `relay_udp`.

## Connection Cascade

Relay is not a separate game mode. It is one candidate in the same ordered
Realnet plan.

Candidate order:

1. Loopback direct.
2. LAN direct.
3. Public direct.
4. NAT punch through `punch_udp`.
5. Relay through `relay_udp`.
6. Steam later when a Steam backend is configured or required.

Relay should be tried when:

1. Relay capability is enabled.
2. The room or server policy permits relay.
3. The join attempt is valid and not expired.
4. Direct candidates were skipped or failed.
5. NAT punch was skipped, unavailable, timed out, or failed to establish a game
   transport.
6. The game supplied a packet socket/adapter compatible with relay routing.

Relay should be skipped when:

1. Relay capability is disabled.
2. The room explicitly disallows relay.
3. The game or build disallows relay.
4. Relay allocation fails authorization.
5. The relay is full or rate-limited.
6. The relay region is unavailable and no alternate relay candidate exists.

## Room And Join Model

Relay allocation is tied to a room join attempt. Room listing alone must never
authorize packet relay.

Required identifiers:

```text
room_code
join_attempt_id
host_member_id
joiner_member_id
relay_allocation_id
host_relay_token
joiner_relay_token
relay_secret
expires_at_ms
```

Roomd owns allocation authority:

1. Client requests or receives a join attempt.
2. Roomd decides whether relay is allowed for that room and joiner.
3. Roomd creates a relay allocation when relay may be needed.
4. Roomd returns relay credentials as part of the connection plan or join
   attempt response.
5. Relayd validates host/joiner packets against the allocation data.

The first implementation can create relay allocations eagerly with the join
attempt. A later optimization can allocate relay only after direct and punch
fail.

## Relay Wire Protocol

Use a small versioned UDP control envelope for relay setup. Game payloads remain
opaque bytes.

Control packets:

```text
relay_hello
relay_ready
relay_keepalive
relay_close
relay_error
```

Data packets:

```text
relay_data
```

A control packet should include:

```text
magic
version
kind
allocation_id
room_code
join_attempt_id
role: host | joiner
member_id
seq
timestamp_ms
nonce
mac
```

A data packet should include:

```text
magic
version
kind=relay_data
allocation_id
sender_role
seq
payload_size
payload_bytes
mac or packet-auth tag
```

The relay must treat `payload_bytes` as opaque. It should not inspect, log, or
decode game data.

### JSON Versus Binary

Punch control packets are bounded JSON because they are low-volume diagnostics
metadata. Relay data packets can be frequent, so relay data should be binary
from the first implementation.

Recommended split:

1. Binary relay packet header and payload for `relay_data`.
2. Binary or bounded JSON control packets are both acceptable for first pass,
   but binary control keeps the protocol consistent.
3. If JSON control is used temporarily, it must be bounded and authenticated,
   and `relay_data` must still be binary.

## Authorization

Relay must only forward packets for authorized room join attempts.

Required checks:

1. Allocation exists.
2. Allocation is not expired.
3. Packet role is allowed for the allocation.
4. MAC verifies against the role token or relay secret.
5. Packet source endpoint is bound to the authenticated role after hello.
6. Packet size is within configured bounds.
7. Sequence/replay checks reject obvious stale duplicate control packets.

Endpoint binding:

1. Host sends `relay_hello` from its game UDP socket.
2. Joiner sends `relay_hello` from its game UDP socket.
3. Relayd records observed UDP source endpoints.
4. After both sides authenticate, relayd sends `relay_ready`.
5. Relayd forwards `relay_data` only between the bound endpoints.

The relay must trust the UDP source endpoint it observes, not an endpoint
claimed in a payload.

## Datagram Forwarding

Forwarding rules:

1. A host packet forwards only to the joiner endpoint for this allocation.
2. A joiner packet forwards only to the host endpoint for this allocation.
3. Packets from unknown endpoints are rejected.
4. Packets over the maximum relay payload size are rejected.
5. Closed or expired allocations reject further data.
6. Relayd may drop packets under rate pressure; it should not queue
   unboundedly.

For Splonks player-hosted lockstep, one allocation is host-to-one-client. If a
future game needs one host to relay to many clients, model each client join as a
separate allocation first. Shared multi-recipient relay rooms can come later if
real usage needs them.

## Timing Defaults

Initial defaults:

```text
relay hello interval: 250 ms
relay setup timeout: 3000 ms
relay allocation TTL: 30000 ms before authenticated setup
relay idle timeout: 10000 ms after setup
relay keepalive interval: 1000 ms
max relay packet bytes: 1400
```

These values should live in Realnet config, not hardcoded in Splonks. The
connection timeline should record the configured values used for each attempt.

## Game-Facing Transport

Games should not special-case relay payloads.

Target adapter behavior:

```text
game sends opaque datagram to peer
Realnet selected transport is direct:
  send datagram to peer endpoint
Realnet selected transport is punch:
  send datagram to punched peer endpoint
Realnet selected transport is relay:
  wrap datagram in relay_data and send to relay endpoint
```

Receiving is the inverse:

```text
direct/punch:
  receive game datagram from peer endpoint
relay:
  receive relay_data from relay endpoint
  verify relay envelope
  expose payload as game datagram from logical peer
```

Splonks should continue to run its lockstep session above this transport. The
only Splonks-specific change should be teaching its UDP transport adapter to
send/receive through a relay endpoint when the selected Realnet candidate is
relay.

## Diagnostics

Relay diagnostics must be first-class because relay is the fallback players hit
when the network is difficult.

Connection timeline events:

```text
relay_candidate_generated
relay_candidate_try
relay_allocation_created
relay_host_hello
relay_joiner_hello
relay_ready
relay_data_first_seen_host
relay_data_first_seen_joiner
relay_selected
relay_closed
relay_failed
```

Failure reasons:

```text
relay_disabled
relay_not_permitted
relay_capability_missing
relay_allocation_failed
relay_allocation_expired
relay_auth_failed
relay_host_timeout
relay_joiner_timeout
relay_idle_timeout
relay_rate_limited
relay_packet_too_large
relay_endpoint_changed
relay_server_full
```

Runtime counters:

```text
active_allocations
allocations_created
allocations_expired
allocations_closed
auth_failures
packets_from_host
packets_from_joiner
bytes_from_host
bytes_from_joiner
packets_dropped
rate_limited_packets
```

Developer/debug endpoints should expose:

```text
/debug/realnet/relays
/debug/realnet/relays/:allocation_id
```

These endpoints should be local/admin-only like existing Realnet debug
endpoints.

## Abuse Controls

Relay moves game traffic, so it needs stronger controls than punch metadata.

Minimum first pass:

1. Per-source packet rate limit.
2. Per-allocation packet and byte rate limits.
3. Maximum active allocations.
4. Maximum packet size.
5. Idle timeout.
6. Allocation TTL.
7. Short-lived relay tokens.
8. Auth failure counters and temporary source bans.
9. Structured logs for drops and auth failures.

Relayd should never become an open UDP reflector. It must not forward payloads
until both endpoints have authenticated and are bound to an allocation.

## Region And Multi-Relay Shape

The first implementation can run one relay on the existing VPS. The protocol
should still leave room for multiple relay regions.

Roomd capability shape can later become:

```json
{
  "relay_udp": {
    "enabled": true,
    "protocol": "gubsy-relay-v1",
    "regions": [
      {"id": "tokyo", "host": "tokyo.example.com", "port": 8790},
      {"id": "us-west", "host": "us-west.example.com", "port": 8790}
    ]
  }
}
```

First pass may advertise a single `host` and `port`. Keep the code structured
so a relay candidate can carry `region_id` later.

## Host Migration And Late Success

If relay is selected, the game should stay on relay for that join attempt. Do
not switch an active Splonks connection from relay back to punch mid-session in
the first implementation.

Reasons:

1. Transport switching during lockstep adds risk.
2. Relay fallback should be stable and easy to reason about.
3. Late punch success can be logged for diagnostics without changing the live
   path.

Future optimization:

1. Try punch in the background while relay is active.
2. If both sides agree and the game transport supports migration, switch after
   a synchronized transport-change handshake.
3. This is not required for first relayd.

## Implementation Order

### Phase 1: Data Model And Config

1. Add `RelayTimingConfig` and `RelayServiceConfig` to Realnet config.
2. Add relay allocation structs to Gubsy.
3. Add relay capability parsing/advertising for `relay_udp`.
4. Add relay candidate fields to the connection plan timeline.
5. Document env/config names.

### Phase 2: Relayd Core

1. Add a relay service module with allocation registry.
2. Add UDP socket bind/start/stop.
3. Implement authenticated `relay_hello`.
4. Bind host and joiner observed endpoints.
5. Send `relay_ready` after both roles authenticate.
6. Implement allocation expiry and idle cleanup.

### Phase 3: Opaque Datagram Forwarding

1. Implement binary `relay_data` packet framing.
2. Forward host payloads to joiner and joiner payloads to host.
3. Add packet size limits.
4. Add per-allocation and per-source rate limits.
5. Add counters and structured logs.

### Phase 4: Gubsy Connection Cascade

1. Add relay candidate generation when `relay_udp` is enabled.
2. Select relay after direct/punch skip or failure.
3. Record relay selection and failure reasons in the attempt timeline.
4. Expose relay setup status to games.

### Phase 5: Splonks Consumer

1. Add relay mode to Splonks Realnet transport runtime.
2. Route outgoing lockstep packets through `relay_data` when relay is selected.
3. Unwrap incoming `relay_data` into the existing lockstep packet path.
4. Display `Connected via relay` in developer status.
5. Keep direct IP and existing direct/punch paths working.

### Phase 6: Validation

1. Unit/smoke test relay packet encode/decode and auth failure.
2. Local two-client relay smoke with direct and punch disabled.
3. Local roomd/relayd live validation using `relay_udp`.
4. Desktop/laptop/VPS validation forcing relay.
5. Network validation where direct/punch are intentionally blocked or forced to
   fail.
6. Splonks gameplay validation:
   - start game
   - join in progress
   - leave/rejoin
   - restart run
   - stage transition
   - no hash mismatch or fatal desync

## Validation Commands To Add

Gubsy:

```bash
./scripts/relay_smoke.sh
ctest --test-dir build --output-on-failure -R "relay"
```

Splonks:

```bash
SPLONKS_REALNET_FORCE_RELAY=1 ./scripts/validate_gubsy_roomd_live.sh
SPLONKS_REALNET_DISABLE_DIRECT=1 \
SPLONKS_REALNET_DISABLE_PUNCH=1 \
./scripts/validate_gubsy_roomd_live.sh
```

Two-machine validation:

```text
desktop host -> roomd/relayd on VPS
laptop client -> same room via relay_udp
expected host marker: REALNET_LAN_HOST_OK
expected client marker: REALNET_LAN_CLIENT_OK
expected connection mode: relay
```

## Completion Criteria

Relayd is ready for the next phase when:

1. `gubsy-roomd /health` advertises enabled `relay_udp`.
2. Relay allocation requires a valid join attempt.
3. Both host and joiner authenticate before any payload forwarding.
4. Relayd forwards opaque datagrams without parsing game payloads.
5. Relay failure reasons appear in the Realnet attempt timeline.
6. Local forced-relay smoke passes.
7. Desktop/laptop/VPS forced-relay validation passes.
8. Direct and NAT punch validations still pass.
9. Splonks lockstep gameplay works over relay without changing lockstep packet
   schemas.
10. Abuse controls prevent unauthenticated reflection and unbounded traffic.

## Open Decisions

1. **Deployable shape:** start as a distinct `gubsy-relayd` binary, or colocate
   inside `gubsy-roomd` while keeping separate modules?
2. **Control packet format:** binary for all relay packets, or bounded JSON for
   setup control plus binary for payload data?
3. **Allocation timing:** allocate relay credentials eagerly with the join
   attempt, or allocate only after direct/punch failure?
4. **Relay region:** single configured relay for first pass, or support a small
   region list immediately?
5. **Game setting:** should games expose "allow relay" to players, or should it
   be a server/build policy?

## Recommended Defaults

1. Build a separate `gubsy-relayd` tool target, with an option for roomd to run
   a colocated relay service in developer mode.
2. Use binary relay packets for both control and data.
3. Allocate relay credentials eagerly with the join attempt for the first
   implementation; optimize later if this becomes wasteful.
4. Support one relay endpoint first, but represent `region_id` internally.
5. Enable relay by default for public player-hosted rooms when the server
   advertises `relay_udp`.
6. Keep relay selection automatic in the normal player UI and visible in
   developer diagnostics.
