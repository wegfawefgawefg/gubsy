# Realnet NAT Punch Plan

This document turns the NAT punch-through part of `realnet_plan.md` into an
implementation plan. The goal is a reusable Gubsy path that lets normal
player-hosted rooms work over the public internet without manual router port
forwarding.

This is not a Splonks-specific protocol. Splonks should keep using its existing
game packets above the connection layer.

## Current Foundation

Already implemented:

1. `gubsy-roomd` room directory.
2. Public/private room visibility.
3. Room update token enforcement.
4. Join-attempt token flow.
5. `SessionContract::connection_candidates`.
6. Candidate kinds for `lan_direct`, `public_direct`, `nat_punch`, `relay`,
   and `steam`.
7. Connection phase identifiers including `trying_nat_punch`.
8. Splonks public browser join routed through Gubsy candidates.
9. Two-machine LAN validation proving `roomd -> join attempt -> LAN direct`.

Next target:

```text
roomd -> join attempt -> LAN direct fails -> NAT punch rendezvous -> direct UDP
```

Relay remains the fallback after this phase.

## Architecture Decision

Use `gubsy-roomd` as the initial deployable binary, but split the code internally:

```text
gubsy-roomd
  room_directory     HTTP room create/list/fetch/update
  join_authority     HTTP join attempts and short-lived tokens
  rendezvous_udp     UDP endpoint observation and punch coordination
  diagnostics        structured logs and attempt timelines
```

This avoids a second service while we are still learning the production shape.
The `rendezvous_udp` module should be written so it can move into a separate
`gubsy-punchd` binary later without changing the game-facing API.

## Core Model

A punch attempt is tied to a room join attempt.

```text
room_code
join_attempt_id
join_token
host_update_token
host_member_id
joiner_member_id
host_observed_udp_endpoint
joiner_observed_udp_endpoint
punch_secret
expires_at
```

The server must trust the UDP source endpoint it observes. It must not trust a
public endpoint claimed in a client payload.

## HTTP Flow

Existing room list/fetch/join-attempt endpoints stay the discovery path.

Add or extend join-attempt response data so a client knows whether NAT punch is
available:

```json
{
  "join_attempt_id": "ABC123",
  "join_token": "short-lived-token",
  "expires_at_ms": 1780000000000,
  "room": {
    "room_code": "ROOM42",
    "contract": {
      "connection_candidates": [
        {
          "kind": "lan_direct",
          "priority": 100,
          "endpoint": "192.168.11.7:35355",
          "label": "LAN direct"
        },
        {
          "kind": "nat_punch",
          "priority": 200,
          "token": "ABC123",
          "label": "NAT traversal"
        }
      ]
    }
  }
}
```

The `nat_punch` candidate should not contain a public endpoint. The public
endpoint is observed through UDP rendezvous.

## UDP Rendezvous Socket

`gubsy-roomd` should open a UDP socket in addition to its HTTP socket.

Default config:

```text
GUBSY_ROOMD_UDP_HOST=0.0.0.0
GUBSY_ROOMD_UDP_PORT=8789
```

The HTTP roomd URL can advertise the UDP rendezvous endpoint in a health/capability
response:

```json
{
  "ok": true,
  "capabilities": {
    "rendezvous_udp": {
      "host": "roomd.example.com",
      "port": 8789,
      "protocol": "gubsy-rendezvous-v1"
    }
  }
}
```

For local development, clients may derive UDP port `HTTP_PORT + 1` only as a
fallback. Production clients should prefer explicit capabilities.

## Chosen Defaults

These decisions are the working defaults for the first implementation.

1. Use JSON rendezvous control packets first, with strict packet-size limits.
   The game payload is still the game's own binary UDP stream; Realnet
   rendezvous only negotiates endpoints and verifies punch probes. Binary
   rendezvous packets can come later if profiling says this matters.
2. Advertise the UDP rendezvous endpoint explicitly through HTTP
   health/capabilities. `HTTP_PORT + 1` is allowed only as a local-development
   fallback.
3. Send host rendezvous hellos lightly while public hosting, with a slower idle
   interval and a faster interval while join attempts are active.
4. Try LAN/direct candidates quickly, but treat NAT punch as the normal public
   internet path for player-hosted rooms. Do not make players rely on port
   forwarding.
5. Build toward a real game-facing transport abstraction. A temporary adapter
   may bridge to an existing game UDP socket during migration, but the Gubsy API
   should not become a pile of verified-endpoint special cases.
6. Do not add manual host approval in the first NAT punch implementation.
   Existing room policy, join tokens, capacity, compatibility, and future
   kick/ban controls are enough for now.
7. Use a `30s` join-attempt TTL and `3s` punch window as first defaults, with
   config knobs and diagnostics so we can tune for high-latency regions.
8. Defer explicit IPv6 NAT-punch support. Direct IPv6 can be a candidate later,
   but IPv4 NAT traversal is the first target.
9. Enable NAT punch by default for public player-hosted rooms. Dedicated-server
   rooms can prefer direct public server endpoints and opt out.
10. Validate first with a VPS roomd plus desktop/laptop on different networks
    where possible. Local simulation and same-LAN smoke tests are necessary but
    not sufficient.
11. Use a small vendored SHA-256/HMAC-SHA256 implementation for first-pass
    rendezvous authentication instead of adding a system OpenSSL requirement.
    The dependency must be portable to Linux, macOS, Windows, Android, and iOS.
12. Expose read-only diagnostics through localhost/admin HTTP endpoints in
    developer deployments. Logs remain the durable evidence stream, but live
    endpoints make debugging active punch attempts much easier.

## Wire Format

Use JSON UDP control packets for the first implementation. Keep them versioned,
small, and bounded by `max_udp_packet_bytes`. These packets are only for
rendezvous and punch verification; game traffic remains opaque to Gubsy.

Logical packet envelope:

```json
{
  "magic": "GUBR",
  "version": 1,
  "kind": "joiner_hello",
  "room_code": "ROOM42",
  "join_attempt_id": "ABC123",
  "sender_role": "joiner",
  "sender_member_id": "MEMBER1",
  "nonce": "random-base64",
  "payload": {},
  "mac": "truncated-hmac-base64"
}
```

Packet kinds:

```text
1 host_hello
2 joiner_hello
3 endpoint_hint
4 punch_probe
5 punch_ack
6 punch_result
7 error
```

Binary can replace the envelope later without changing the room/cascade model.

## Authentication

Use separate secrets:

1. Host authenticates `host_hello` with the room host update token.
2. Joiner authenticates `joiner_hello` with the join token.
3. `roomd` creates a per-attempt `punch_secret`.
4. `endpoint_hint`, `punch_probe`, `punch_ack`, and `punch_result` use
   `punch_secret`.

MAC input should include:

```text
magic
version
kind
room_code
join_attempt_id
sender_role
sender_member_id
nonce
payload
```

Minimum first implementation:

1. Use HMAC-SHA256 truncated to 16 bytes.
2. Reject packets with invalid MACs.
3. Reject expired attempts.
4. Reject packet sizes above a small configured maximum.
5. Reject packets whose room/join attempt is unknown.
6. Keep the HMAC helper private to roomd/rendezvous internals until the public
   transport API needs cryptographic helpers.

## Host Flow

When a player hosts a public room:

1. Start the game UDP socket as today.
2. Publish room with `lan_direct` and `nat_punch` candidates.
3. Send periodic `host_hello` packets to the roomd UDP endpoint.
4. Include room code, host member id, host update token, and local UDP port.
5. Keep sending low-rate `host_hello` packets while the public room is alive.
   Increase to the active interval while roomd reports active join attempts.
6. When `endpoint_hint` arrives for a joiner, send `punch_probe` packets to
   the joiner's observed endpoint for the punch window.
7. When a verified probe/ack is received from the joiner, promote that endpoint
   to the game transport allowlist.

Host diagnostics should log:

```text
room_code
join_attempt_id
local_udp_endpoint
observed_udp_endpoint
remote_observed_endpoint
punch_probe_count
punch_success
selected_transport
```

## Joiner Flow

When joining a room:

1. Resolve room and create join attempt over HTTP.
2. Try loopback/LAN/public direct candidates first.
3. If direct candidates fail and `nat_punch` is allowed, enter
   `TryingNatPunch`.
4. Send periodic `joiner_hello` packets to roomd UDP.
5. Wait for `endpoint_hint`.
6. Send `punch_probe` packets to the host's observed endpoint for the punch
   window.
7. Accept only authenticated `punch_probe`/`punch_ack`.
8. On success, hand the verified remote endpoint to the game transport.
9. On timeout, report `NatTraversalFailed` and continue to relay if available.

Joiner diagnostics should log the same attempt timeline as the host.

## Roomd Rendezvous Flow

For each active join attempt:

1. Store the join attempt from HTTP.
2. Accept `host_hello` only from the current room host with a valid host token.
3. Accept `joiner_hello` only with a valid join token.
4. Record UDP source endpoints observed by roomd.
5. Once both endpoints are known, mint or reuse the attempt `punch_secret`.
6. Send signed `endpoint_hint` packets to both sides.
7. Continue hint retransmission for a bounded time.
8. Accept optional `punch_result` from both sides for diagnostics.
9. Expire the attempt.

The server does not proxy gameplay packets in this milestone.

## Timing Defaults

Initial defaults:

```text
join_attempt_ttl_sec = 30
host_hello_interval_ms = 500
host_hello_active_interval_ms = 100
joiner_hello_interval_ms = 100
endpoint_hint_interval_ms = 100
punch_probe_interval_ms = 50
punch_window_ms = 3000
max_udp_packet_bytes = 1200
udp_rate_limit_per_ip_per_sec = 100
udp_rate_limit_per_ip_burst = 200
udp_rate_limit_per_room_per_sec = 200
udp_rate_limit_per_room_burst = 400
active_join_attempts_per_room = 16
join_attempts_per_room_per_min = 64
join_attempts_per_ip_per_min = 120
```

These should be configurable for testing.

## Connection Cascade Integration

Current cascade helper selects direct candidates only. The next API should
become stateful:

```cpp
struct ConnectionAttemptOptions {
    std::string room_server_url;
    std::string room_code;
    std::string join_attempt_id;
    std::string join_token;
    bool allow_lan_direct = true;
    bool allow_public_direct = true;
    bool allow_nat_punch = true;
    bool allow_relay = true;
};

struct ConnectionAttemptEvent {
    ConnectPhase phase;
    std::string message;
    std::string candidate_kind;
    std::string local_endpoint;
    std::string observed_endpoint;
    std::string remote_endpoint;
};
```

The final game-facing API should return a packet transport. The intermediate
implementation may include an adapter that feeds verified endpoints into
Splonks' existing UDP transport, but that adapter should sit behind the
Realnet transport boundary and should not define the public Gubsy API.

## Splonks Integration

Splonks should not know how NAT punching works.

Splonks should provide:

1. Host UDP socket/local port.
2. Callbacks to attempt direct UDP endpoint joins.
3. Callback to allow/promote a verified remote endpoint.
4. UI/log sink for `ConnectPhase` and diagnostics.

Splonks should receive:

1. `TryingNatPunch`.
2. Candidate attempt logs.
3. `Connected` with selected transport `nat_punch`.
4. `Failed` with reason when punch fails.

## Smoke Tools

Build the smoke tools before wiring full UI:

1. `gubsy-roomd` UDP rendezvous unit/integration smoke.
2. A standalone `gubsy-punch-smoke` or `gubsy-roomd --punch-smoke` mode that
   runs two local UDP sockets through roomd and verifies endpoint hints and
   probe MACs.
3. A two-machine LAN smoke using roomd UDP even though LAN direct would work.
4. A cross-network smoke using a VPS roomd.
5. Splonks headless browser join with direct candidates disabled, forcing
   `nat_punch`.

The smoke should print stable markers:

```text
REALNET_PUNCH_HOST_HELLO
REALNET_PUNCH_JOINER_HELLO
REALNET_PUNCH_HINTS_EXCHANGED
REALNET_PUNCH_PROBE_VERIFIED
REALNET_PUNCH_OK
```

## Diagnostics And Logs

Roomd structured logs should include:

```text
roomd_udp_start
punch_attempt_create
punch_host_hello
punch_joiner_hello
punch_endpoint_hint
punch_probe_result
punch_attempt_expire
punch_attempt_fail
```

Avoid logging raw secrets or full MAC keys. Logging room code, attempt id,
member id, endpoint, phase, timing, and failure reason is useful.

Read-only diagnostics should also be available through admin/debug HTTP
endpoints when explicitly enabled:

```text
GET /debug/realnet
GET /debug/realnet/rooms/:room_code
GET /debug/realnet/attempts/:join_attempt_id
```

Default policy:

1. Bind diagnostics to `127.0.0.1`.
2. Disable public binding unless explicitly configured.
3. Never return raw tokens, HMAC keys, or full MAC secrets.
4. Include current phase, endpoint observations, selected candidate, counters,
   timings, and failure reasons.

## Security Limits

First implementation should include:

1. Join attempt TTL.
2. Per-IP UDP packet rate limit.
3. Per-room active join attempt cap.
4. Maximum packet size.
5. HMAC verification.
6. No gameplay relay in roomd.
7. No acceptance of unauthenticated game traffic as a punch success.
8. Read-only admin endpoints bound to localhost by default.
9. No secrets in logs or diagnostics.

## Implementation Checklist

1. Add rendezvous config to `gubsy-roomd`.
2. Add UDP socket lifecycle and polling loop.
3. Define packet structs, serializer, parser, and MAC helpers.
4. Persist active punch attempt state keyed by join attempt id.
5. Include `nat_punch` candidate in public player-hosted room metadata.
6. Add host `host_hello` sender.
7. Add joiner `joiner_hello` sender.
8. Add endpoint hint emission and validation.
9. Add authenticated punch probes.
10. Add success/failure result reporting.
11. Extend connection cascade API beyond direct-candidate selection.
12. Wire Splonks public browser join to attempt NAT punch after direct failure.
13. Add smoke tools and logs.
14. Validate with a VPS roomd across two networks.
15. Add localhost-only admin/debug endpoints for active rooms and punch attempts.

## Current Implementation Notes

The first roomd rendezvous foundation is implemented in `gubsy-roomd`:

- `--rendezvous-port=<udp-port>` binds a UDP rendezvous socket.
- `--no-rendezvous` disables the UDP socket for debugging.
- `/health` advertises `realnet.rendezvous_udp` with the configured endpoint
  and `gubsy-rendezvous-v1` protocol name.
- Join attempts now receive a per-attempt `punch_secret`.
- UDP `host_hello` packets are HMAC-authenticated with the room host secret.
- UDP `joiner_hello` packets are HMAC-authenticated with the join attempt
  punch secret.
- When both sides have sent valid hellos, roomd sends signed `endpoint_hint`
  packets to both sides with observed endpoints.
- `punch_result` packets are accepted for diagnostics.
- Per-source and per-room token-bucket rate limits protect the UDP path.
- Localhost-only `/debug/realnet`, `/debug/realnet/rooms/:room_code`, and
  `/debug/realnet/attempts/:join_attempt_id` expose current rendezvous state.
- `room_rendezvous_smoke` covers packet signing, tamper rejection, and rate
  limiter behavior.

Remaining work in this phase is to expose rendezvous data through the client
API, send host/joiner hellos from Gubsy, send peer-to-peer punch probes/acks
during the punch window, and wire Splonks browser joins into the cascade.

## Decisions Needed

Resolved for the first pass:

1. JSON rendezvous control packets, bounded and HMAC-authenticated.
2. Explicit advertised UDP rendezvous capability.
3. Low-rate host hello while public; faster with active attempts.
4. LAN/direct quick attempt, then NAT punch as normal public player-host path.
5. Real transport abstraction as the API target, with migration adapters allowed.
6. No manual host approval before endpoint hints in the first pass.
7. `30s` join-attempt TTL and `3s` punch window, both configurable.
8. IPv4 NAT punch first; IPv6 deferred.
9. NAT punch default-on for public player-hosted rooms.
10. VPS roomd plus two real client networks is the first meaningful public proof.

Still open:

1. Exact public validation setup. Desktop and laptop on the same home LAN are
   not enough for a real NAT punch proof, even with a VPS roomd, because they
   usually share the same public NAT. Use home internet plus laptop/phone
   hotspot, or two fixed networks, with the VPS running roomd.
2. Whether a later production relay should share the same process as roomd or
   split into a dedicated relay service immediately.
