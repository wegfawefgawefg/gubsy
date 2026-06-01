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

## Wire Format

Use a compact binary UDP format. Keep it versioned and small enough to fit
comfortably under common MTUs.

All packets:

```text
magic          4 bytes  "GUBR"
version        u8       1
kind           u8
flags          u16
payload_len    u16
payload        payload_len bytes
mac            16 or 32 bytes
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

Encoding can be either hand-packed binary or length-prefixed JSON for the first
implementation. The production target is binary, but JSON is acceptable for the
first smoke if the parser enforces maximum packet sizes.

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

## Host Flow

When a player hosts a public room:

1. Start the game UDP socket as today.
2. Publish room with `lan_direct` and `nat_punch` candidates.
3. Send periodic `host_hello` packets to the roomd UDP endpoint.
4. Include room code, host member id, host update token, and local UDP port.
5. Keep sending `host_hello` while the room is alive.
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
joiner_hello_interval_ms = 100
endpoint_hint_interval_ms = 100
punch_probe_interval_ms = 50
punch_window_ms = 3000
max_udp_packet_bytes = 1200
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
implementation can still return a verified endpoint to Splonks' existing UDP
transport, but the boundary should be kept narrow.

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

## Security Limits

First implementation should include:

1. Join attempt TTL.
2. Per-IP UDP packet rate limit.
3. Per-room active join attempt cap.
4. Maximum packet size.
5. HMAC verification.
6. No gameplay relay in roomd.
7. No acceptance of unauthenticated game traffic as a punch success.

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

## Decisions Needed

These are the ambiguities that should be resolved before starting code:

1. Should the first UDP packet format be binary immediately, or JSON with strict
   packet-size limits for speed of implementation?
2. Should `gubsy-roomd` derive UDP port from HTTP port by default, or require
   explicit advertised UDP rendezvous capabilities?
3. Should host `host_hello` run continuously while public hosting, or only when
   there are active join attempts?
4. Should NAT punch be attempted after LAN direct fails, or in parallel with
   direct candidates to reduce join time?
5. Should the first implementation return a verified endpoint to the existing
   game UDP transport, or should it introduce the full `PacketTransport`
   abstraction now?
6. Should the host be able to explicitly approve/reject a join attempt before
   roomd releases endpoint hints?
7. How long should punch attempts last before falling through to relay/failure?
8. Do we need IPv6 support in the first NAT punch implementation?
9. Should `nat_punch` be enabled by default for public rooms, or require a game
   config flag?
10. What is the minimum VPS validation target: two LANs, mobile hotspot, or a
    controlled NAT lab?

