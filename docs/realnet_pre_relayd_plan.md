# Realnet Pre-Relayd Plan

This document describes the cleanup phase between the current NAT punch
foundation and a production relay service. The goal is to make Realnet reusable
for future Gubsy games before adding the larger relay fallback.

The current system proves the important technical path:

```text
roomd room -> join attempt -> UDP rendezvous -> NAT punch -> game UDP packets
```

Splonks validated this over real networks with a desktop host on home internet,
a laptop client on a phone hotspot, and a Tokyo VPS running `gubsy-roomd`.

That proof is valuable, but relayd should not be built on top of game-specific
cascade decisions or service boundaries that are only good enough for one
consumer. This phase turns the proof into a cleaner Realnet foundation.

## Target Outcome

After this phase, Gubsy should own the reusable Realnet connection planning
surface:

1. Games publish rooms with connection candidates and authority metadata.
2. Gubsy classifies and orders candidate attempts.
3. Gubsy exposes direct, NAT punch, and future relay choices through one
   game-facing plan.
4. Games provide a packet socket or transport adapter and keep their game
   protocol opaque.
5. Splonks becomes a normal Realnet consumer instead of carrying Realnet policy.

Relayd should then be a new candidate type in the same cascade, not another
one-off integration.

## Why This Comes Before Relayd

Relay is the guaranteed fallback for networks where direct UDP and NAT punch
fail. It will add more state, more bandwidth, more security constraints, and
more operational cost than rendezvous.

If relay is added before the Realnet policy is cleaned up, the likely result is:

1. Splonks-specific cascade rules duplicated in relay code.
2. Room metadata that cannot cleanly describe all connection paths.
3. Harder debugging because direct, punch, and relay use different mental
   models.
4. A relay service that is difficult for the next Gubsy game to consume.

The cleanup keeps relay small: relay only moves opaque datagrams between
authorized endpoints. Candidate choice, room identity, join authorization, and
diagnostics stay in the Realnet layer.

## Current Shortcuts To Fix

### 1. Splonks Owns Too Much Cascade Policy

Current state:

- Gubsy publishes room/join/rendezvous metadata.
- Splonks decides when to try direct UDP, when to skip private direct, and when
  to start NAT punch.

Why this is a problem:

- Other games would have to reimplement the same candidate logic.
- Fixes to candidate ordering would need to be copied per game.
- Relay fallback would likely become another game-specific branch.

Target:

- Gubsy exposes a Realnet connection plan API.
- Games ask Gubsy for ordered attempts for a room join.
- Splonks only executes the returned attempts through its UDP socket adapter.

### 2. Private Direct Detection Is Too Simple

Current state:

- Splonks skips private IPv4 direct endpoints unless the endpoint appears to be
  on the client's local `/24`.
- This solved the common home-LAN-advertised-to-phone-hotspot case.

Why this is a problem:

- `/24` is a heuristic, not a real subnet check.
- Some LANs use different masks.
- IPv6 is not covered.
- This logic belongs in reusable Realnet candidate classification, not Splonks.

Target:

- Gubsy has a `candidate_classifier` module.
- It understands candidate kinds such as `lan_direct`, `public_direct`,
  `nat_punch`, `relay`, and `steam`.
- It can classify local reachability using local interfaces, configured
  netmasks when available, and conservative defaults when unavailable.
- Unknown cases are explicit in diagnostics rather than hidden behind a boolean.

### 3. The Service Boundary Is Not Clear Enough

Current state:

- `gubsy-roomd` handles HTTP room directory and UDP rendezvous in one binary.
- The NAT punch code was written so it can move later, but the deployed process
  is still one service.

Why this is a problem:

- Room listing and UDP packet coordination have different scaling profiles.
- Relay traffic will have an even more different scaling profile.
- Naming matters for operations: `roomd`, `punchd`, and `relayd` communicate
  different responsibilities.

Target:

```text
gubsy-roomd
  HTTP room directory
  join attempts
  room metadata
  service capability discovery

gubsy-punchd
  UDP endpoint observation
  NAT punch rendezvous
  endpoint hint signing
  punch diagnostics

gubsy-relayd
  opaque UDP packet relay
  relay allocations
  bandwidth/accounting limits
  relay diagnostics
```

These can still be shipped as one process during development if needed, but the
code and configuration should make the boundary explicit. The API should not
care whether the services are colocated or split.

### 4. Rendezvous Timings Are Fixed Constants

Current state:

- Host/joiner hello interval is fixed.
- Punch probe interval is fixed.
- Punch window is fixed.

Why this is acceptable:

- Fixed retry timing is normal for a first implementation.
- NAT punch traffic is low volume.
- The current values worked in real validation.

Why it still belongs in this plan:

- Relayd fallback needs a clear timeout decision.
- Future diagnostics should show whether punch failed because no endpoint was
  observed, probes did not arrive, or the game never accepted the path.

Target:

- Keep sane defaults.
- Move constants into Realnet config.
- Record attempt timing in diagnostics.
- Make relay fallback consume a clear `NatPunchFailed` result.

### 5. Diagnostics Are Useful But Not Yet Unified

Current state:

- `gubsy-roomd` has structured logs and localhost-only debug endpoints for
  rooms and punch attempts.
- Splonks logs connection mode strings such as `Joining with Realnet NAT punch`.

Why this is a problem:

- Debugging requires correlating game logs, roomd logs, and roomd debug JSON.
- There is not yet a single connection attempt timeline.

Target:

- Gubsy records a connection attempt timeline:
  - room selected
  - candidates generated
  - direct candidate skipped/tried/failed
  - punch endpoint observed
  - endpoint hints sent
  - punch probes/acks observed
  - game transport accepted
  - relay fallback selected
- Splonks can show a concise status label, but Gubsy owns the canonical attempt
  details.

### 6. IPv6 Is Not A First-Class Path Yet

Current state:

- The practical validation was IPv4.
- Private direct classification is IPv4-only.

Why this matters:

- Some networks are IPv6-capable or IPv6-preferred.
- Some mature game networking stacks support IPv6.
- Ignoring IPv6 forever would make Realnet less general.

Target:

- Do not block relayd on complete IPv6 support.
- Do make the candidate model capable of representing IPv6 direct candidates.
- Keep IPv6 classification explicit as `unknown`, `public`, `link_local`, or
  `global` rather than pretending IPv4 rules apply.

### 7. JSON Rendezvous Packets Are Fine For Now

Current state:

- Rendezvous control packets are bounded JSON and HMAC-authenticated.
- Gameplay data is not JSON and does not flow through roomd/punchd.

Why this is acceptable:

- Rendezvous packets are low-volume metadata.
- JSON is easy to inspect while building and debugging the service.
- Packet size is bounded.

Target:

- Keep JSON for rendezvous control unless profiling or compatibility proves it
  is a problem.
- Relay game payloads remain opaque bytes and should not use JSON.

## Realnet Connection Plan API

Gubsy should expose a game-facing API that turns a join attempt into ordered
connection attempts.

Sketch:

```cpp
struct RealnetLocalNetworkInfo {
    std::vector<LocalInterfaceAddress> interfaces;
};

enum class RealnetCandidateKind {
    LanDirect,
    PublicDirect,
    NatPunch,
    Relay,
    Steam,
};

enum class RealnetCandidateDecision {
    Try,
    SkipUnreachablePrivate,
    SkipUnsupported,
    SkipDisabled,
};

struct RealnetConnectionCandidate {
    RealnetCandidateKind kind;
    RealnetCandidateDecision decision;
    int priority;
    std::string endpoint;
    std::string reason;
};

struct RealnetConnectionPlan {
    std::string room_code;
    std::string join_attempt_id;
    std::vector<RealnetConnectionCandidate> candidates;
};
```

The game should not need to know why a private endpoint is unreachable. It
should receive an ordered plan and status reasons it can display or log.

## Game Transport Adapter

Realnet should not own the game protocol. It only needs a way to send and
receive bytes through the same socket or transport mapping the game will use.

Sketch:

```cpp
struct RealnetDatagramEndpoint {
    std::string host;
    std::uint16_t port;
};

struct RealnetDatagramSocket {
    bool (*send)(void* user_data,
                 const RealnetDatagramEndpoint& endpoint,
                 const std::uint8_t* bytes,
                 std::size_t size);

    bool (*receive)(void* user_data,
                    RealnetDatagramEndpoint& endpoint,
                    std::uint8_t* bytes,
                    std::size_t capacity,
                    std::size_t& size);

    void* user_data;
};
```

Splonks can adapt its existing UDP transport to this shape. Other games can do
the same without copying Splonks-specific lockstep code.

## Service Split Plan

### Step 1: Internal Module Split

Keep one deployable binary if convenient, but split internals:

```text
tools/room_server/
  room_directory.*
  join_authority.*
  realnet_capabilities.*
  punch_rendezvous.*
  diagnostics.*
```

The room directory should not directly own UDP packet parsing logic.

### Step 2: Config Names

Support names that describe the future services:

```text
GUBSY_ROOMD_HTTP_HOST
GUBSY_ROOMD_HTTP_PORT
GUBSY_PUNCHD_UDP_HOST
GUBSY_PUNCHD_UDP_PORT
GUBSY_RELAYD_UDP_HOST
GUBSY_RELAYD_UDP_PORT
```

There are no external consumers yet, so compatibility aliases should not be
carried. Docs and code should prefer the explicit service names.

### Step 3: Capability Shape

Expose services as separate capabilities:

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
      "enabled": false,
      "host": "",
      "port": 0,
      "protocol": "gubsy-relay-v1"
    }
  }
}
```

The old `rendezvous_udp` health name should not be supported. The clean name is
`punch_udp` so the service responsibility is clear.

## Candidate Classification Rules

The first candidate classifier should be conservative.

### Direct Candidates

Try direct when:

1. Endpoint is loopback and client is local.
2. Endpoint is private IPv4 and appears local based on interface/subnet data.
3. Endpoint is public IPv4.
4. Endpoint is IPv6 global and the client has IPv6 support.

Skip direct when:

1. Endpoint is private IPv4 and no local interface appears compatible.
2. Endpoint is link-local IPv6 without a usable scope.
3. Endpoint kind is unsupported by the current platform.

### NAT Punch Candidates

Try NAT punch when:

1. A join attempt exists.
2. Punch credentials exist.
3. Punch service capability is enabled.
4. The game can provide a UDP socket adapter.

Fail NAT punch when:

1. No host endpoint is observed before timeout.
2. No joiner endpoint is observed before timeout.
3. Endpoint hints are not received.
4. Probes/acks do not establish a peer endpoint before timeout.
5. The game transport does not accept the peer endpoint.

### Relay Candidates

Relay is not implemented in this phase, but the plan should reserve the shape.

Try relay when:

1. Relay capability is enabled.
2. The room/join attempt permits relay.
3. Direct and NAT punch are skipped or fail.
4. The game has a packet adapter compatible with relay routing.

## Splonks Migration

Splonks currently proves the path and should be simplified after the Gubsy API
exists.

Move out of Splonks:

1. Private endpoint localness classification.
2. Realnet candidate ordering.
3. Direct-vs-punch status reason construction.

Keep in Splonks:

1. UDP socket ownership.
2. Game packet encoding/decoding.
3. Lockstep session acceptance and player topology.
4. UI labels that display the selected connection mode.

Splonks should end up with code shaped like:

```cpp
RealnetConnectionPlan plan = gubsy_realnet_build_connection_plan(...);
for (const RealnetConnectionCandidate& candidate : plan.candidates) {
    if (candidate.decision != RealnetCandidateDecision::Try)
        continue;
    if (TryCandidate(candidate))
        break;
}
```

## Validation

This phase is complete when the following are true:

1. Existing direct LAN validation still passes.
2. Existing forced NAT punch validation still passes.
3. Desktop home internet host plus laptop phone-hotspot client still passes
   through the VPS punch service.
4. The private direct skip rule is produced by Gubsy/Realnet, not Splonks.
5. Splonks logs the selected connection mode using Gubsy's connection attempt
   status.
6. `gubsy-roomd` or the future service split advertises room, punch, and relay
   capabilities distinctly.
7. Debug endpoints show an attempt timeline that identifies skipped direct,
   attempted punch, and future relay fallback points.
8. No game data is parsed by roomd or punchd.
9. The relay plan can be implemented as a new candidate type without changing
   Splonks lockstep packet schemas.

## Implementation Order

1. Add a Realnet candidate model and connection-plan API to Gubsy.
2. Move private/public/local endpoint classification into Gubsy.
3. Add local-interface collection helpers, including IPv4 and placeholder IPv6
   classification.
4. Update room/server capabilities to distinguish room directory, punch UDP,
   and relay UDP.
5. Refactor roomd internals so UDP rendezvous lives behind a punch-service
   module.
6. Update Splonks to consume Gubsy's connection plan.
7. Update Splonks status labels/logs to use Gubsy connection attempt reasons.
8. Re-run local direct, local forced punch, and VPS phone-hotspot validation.
9. Commit and push the pre-relayd cleanup.
10. Start relayd implementation.

The relay implementation plan now lives in `realnet_relayd_plan.md`.

## Non-Goals

This phase does not:

1. Implement relay packet forwarding.
2. Implement Steam transport.
3. Replace Splonks UDP gameplay transport.
4. Add peer-to-peer mesh.
5. Require binary rendezvous packets.
6. Require perfect IPv6 support before relayd.

## Decision Summary

1. Candidate/cascade policy should be Gubsy-owned.
2. Splonks should become a normal Realnet consumer.
3. `roomd`, `punchd`, and `relayd` should be distinct responsibilities.
4. They may be colocated temporarily, but service boundaries must be explicit.
5. Relay should be built only after the candidate model and punch service
   boundary are clean enough to reuse.
