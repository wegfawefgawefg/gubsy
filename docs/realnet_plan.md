# Realnet Plan

Realnet is the next networking phase for Gubsy: reusable internet connectivity
for games that cannot assume LAN play or manual router configuration.

The goal is not a Splonks-only prototype. The goal is a general Gubsy layer that
lets any game publish a room, join a room, and exchange opaque packets through
the best available transport.

## Goals

Realnet should support:

1. Player-hosted rooms where one player's game process owns the session.
2. Dedicated authoritative servers where a headless game server owns the
   session.
3. Direct LAN and same-machine connections.
4. Public internet joins without requiring players to understand port
   forwarding.
5. UDP NAT traversal for non-Steam builds.
6. Relay fallback later for networks where NAT traversal fails.
7. Steam lobby and SteamNetworkingSockets support later without changing the
   game-facing model.

Realnet should not require games to use one particular simulation model.
Lockstep, rollback, host-authoritative snapshots, dedicated authoritative
servers, and turn-based protocols should all fit above the same connection
layer.

## Non-Goals

Realnet should not:

1. Simulate game state.
2. Define game packet schemas.
3. Require deterministic lockstep.
4. Require host-authoritative snapshots.
5. Implement peer-to-peer mesh as a primary design target.
6. Make Gubsy's directory daemon run game simulations.
7. Treat Steam as the only internet path.

Peer-to-peer mesh is intentionally out of scope for now. Most likely Gubsy games
will use either player-hosted rooms or dedicated authoritative servers. Carrying
mesh support would add complexity to identity, NAT traversal, relay routing, and
debugging before we have a game that needs it.

## Architecture

Use one deployable server binary initially:

```text
gubsy-roomd
  room directory
  join authorization
  UDP rendezvous / NAT punch coordination
  relay coordination later
```

Keep the internals separated even if the deployable is one process. The current
room directory should not become a pile of special cases.

Suggested internal modules:

1. `room_directory`: create/update/list/fetch rooms.
2. `join_authority`: issue and validate short-lived join attempts.
3. `rendezvous`: observe UDP public endpoints and broker punch attempts.
4. `relay_registry`: advertise relay availability later.
5. `diagnostics`: health, stats, endpoint attempt logs, and admin/debug views.

This gives us one server to run during development while leaving room to split
relay traffic into a separate service later if bandwidth or scaling requires it.

## Discovery Versus Transport

Discovery answers:

1. What rooms exist?
2. Is this client allowed to join?
3. What version/protocol/session contract is advertised?
4. What connection candidates are available?

Transport answers:

1. How do packets move after a room is selected?
2. Which peer or server is connected?
3. Is this direct UDP, NAT-punched UDP, relayed UDP, or Steam?
4. How does the game send and receive opaque bytes?

The room browser should never mean "connect to this single IP and hope." A room
entry means "ask Gubsy to establish a session using the available candidates."

## Authority Modes

Realnet should model the owner of the session separately from the transport
backend.

```cpp
enum class RoomAuthorityMode {
    PlayerHost,
    DedicatedServer,
};
```

### PlayerHost

A normal game client publishes a room and owns the session. Other clients join
that host.

Examples:

1. Splonks co-op hosted by a player.
2. A casual game where a friend starts a lobby.
3. A Steam lobby whose owner is also the game host.

### DedicatedServer

A headless game process publishes a room and owns the session. Players connect
to that server.

Examples:

1. `splonks-server` running on a VPS.
2. A persistent survival/world server.
3. A tournament server with no player-host UI.

`gubsy-roomd` should not be the dedicated game server. It helps clients discover
and connect to the game server.

## Transport Backends

Transport backend is separate from authority mode.

```cpp
enum class TransportKind {
    Loopback,
    LanDirect,
    PublicDirect,
    NatPunch,
    Relay,
    Steam,
};
```

The same player-hosted room could be reached over LAN direct, public direct,
NAT punch, relay, or Steam. The same is true for a dedicated server.

## Connection Cascade

Room joins should use a cascade. Gubsy tries the cheapest/direct path first and
falls back to more robust paths when needed.

Initial cascade:

1. Resolve the room and validate compatibility.
2. Try loopback when joining a local same-machine endpoint.
3. Try LAN direct candidates.
4. Try observed public direct candidates.
5. Try UDP NAT punch-through through `gubsy-roomd` rendezvous.
6. Later, fall back to Gubsy relay.
7. Later, use Steam transport when the selected room/backend requires it.

Game UI should receive connection phases:

```cpp
enum class ConnectPhase {
    ResolvingRoom,
    CheckingCompatibility,
    TryingLoopback,
    TryingLanDirect,
    TryingPublicDirect,
    TryingNatPunch,
    TryingRelay,
    TryingSteam,
    Connected,
    Failed,
};
```

The game should be able to display this without knowing the NAT traversal
details.

## Room Record

Rooms need connection candidates, not a single endpoint.

Suggested fields:

```text
room_id
room_code optional
display_name
game_id
game_version
protocol_version
authority_mode: player_host | dedicated_server
visibility: public | unlisted | private
phase: lobby | in_game | closed
player_count
max_players
host_client_id optional
server_id optional
session_contract
connection_candidates[]
join_policy
created_at
updated_at
expires_at
```

Connection candidate fields:

```text
candidate_id
transport_kind
priority
address_family
endpoint optional
lan_scope optional
observed_by optional
relay_region optional
steam_lobby_id optional
steam_identity optional
expires_at
```

The server should treat endpoint data as short-lived. NAT mappings expire, and
rooms must heartbeat.

## Join Authorization

Joining should be tokenized. Listing a public room should not grant unlimited
permission to inject UDP packets into a host.

Suggested flow:

```text
client -> roomd HTTP: request join room
roomd -> client HTTP: join_attempt_id, short-lived join_token, candidates
client -> roomd UDP: rendezvous hello(join_attempt_id, join_token)
host   -> roomd UDP: rendezvous hello(room_id, host_update_token)
roomd -> host/client UDP: signed peer endpoint hints
host/client -> each other UDP: punch probes with attempt MAC
```

Tokens should be short-lived. A host should be able to reject or expire a join
attempt. Room update tokens and join tokens must be separate.

## UDP Punch-Through

NAT traversal needs a UDP rendezvous path. HTTP room listing is not enough.

Rendezvous responsibilities:

1. Observe the public UDP endpoint used by the host.
2. Observe the public UDP endpoint used by the joiner.
3. Validate both sides with room/update/join tokens.
4. Send each side the other's observed public endpoint.
5. Tell both sides to send punch probes for a bounded time window.
6. Report success/failure and endpoint details to diagnostics.

Punch probes should be authenticated:

```text
punch_probe
  protocol_magic
  protocol_version
  room_id
  join_attempt_id
  source_client_id
  nonce
  mac
```

Peers should not accept gameplay traffic from an endpoint until a verified
punch/handshake packet succeeds.

## Relay Fallback

UDP punch-through will not always work. Symmetric NAT, strict firewalls, mobile
networks, school/work networks, and ISP configurations can all break it.

Relay is the durable fallback:

1. Both peers connect outbound to a relay.
2. Relay forwards opaque packets between authorized peers.
3. The game sees the same transport interface.
4. UI can show "Using relay" for debugging and expectations.

Relay can be added after NAT punch-through if the APIs already have a
`TransportKind::Relay` and connection phase. Do not design NAT punch as a
dead-end special case.

## Steam Integration

Steam fits both authority modes.

For player-hosted rooms:

1. Steam lobby discovery can replace or mirror room directory discovery.
2. SteamNetworkingSockets can provide NAT traversal and relay fallback.
3. The game still sees "connected to host."

For dedicated servers:

1. A headless server can advertise through Steam server APIs, Gubsy roomd, or
   both.
2. Clients can connect through SteamNetworkingSockets or a non-Steam Gubsy path.
3. Steam auth can be used when available.

Steam should implement the same discovery/transport boundaries as non-Steam
Realnet. It should not become a separate game mode.

## Game-Facing API Shape

Games should ask Gubsy to connect and then exchange packets.

Sketch:

```cpp
struct ConnectOptions {
    std::string room_id;
    std::string player_name;
    std::string game_id;
    std::string protocol_version;
    bool allow_relay = true;
    bool allow_nat_punch = true;
    bool allow_steam = true;
};

struct PacketTransport {
    TransportKind kind;
    PeerId authority_peer;

    bool Send(PeerId peer, ByteSpan payload, SendMode mode);
    void Poll(std::vector<PacketEvent>& out_events);
    void Close();
};
```

The payload bytes are opaque to Gubsy. A game can run lockstep, rollback,
snapshots, commands, or an authoritative protocol above this layer.

## Diagnostics

Realnet needs first-class diagnostics from the start.

Required debug information:

1. Room ID and room code.
2. Authority mode.
3. Selected transport.
4. Every candidate tried.
5. Local endpoint.
6. Observed public endpoint.
7. Remote endpoint.
8. NAT punch result and timing.
9. Relay fallback reason.
10. Join failure reason.
11. Last heartbeat age.

This should be visible to games through callbacks and available in roomd logs.

## Security And Abuse Basics

First implementation can stay simple, but it should avoid obvious traps.

Required basics:

1. Short-lived join tokens.
2. Separate host update tokens.
3. Authenticated rendezvous and punch probes.
4. Room heartbeat expiry.
5. Rate limits for room creation, join attempts, and UDP rendezvous probes.
6. Maximum packet sizes.
7. Versioned wire formats.
8. No blind trust in client-advertised public IPs.

The rendezvous server should trust the UDP source endpoint it observes, not a
public endpoint claimed by the client.

## Milestones

### Milestone 1: Design And Data Model

1. Finalize room candidate schema.
2. Finalize join attempt/token lifecycle.
3. Finalize game-facing connection phases.
4. Document config keys and debug output.

### Milestone 2: Roomd Candidate API

1. Extend room publish/update to include authority mode and candidates.
2. Add join-attempt endpoint.
3. Keep existing browser/listing compatibility where possible.
4. Add CLI/curl smoke coverage for room creation, join attempt, and expiry.

### Milestone 3: UDP Rendezvous

1. Add UDP listener to `gubsy-roomd`.
2. Register host and joiner observed endpoints.
3. Exchange signed endpoint hints.
4. Add standalone punch smoke utility.
5. Log candidate attempt timelines.

### Milestone 4: Gubsy Client Cascade

1. Add reusable connection state machine.
2. Try loopback/LAN/public direct before punch-through.
3. Add NAT punch attempt window and verification.
4. Return a packet transport to the game.
5. Report clean failure reasons.

Foundation status: `include/gubsy/lobby/connection_cascade.hpp` now owns the
reusable direct-candidate ordering and selection path used by the browser join
flow. NAT punch, relay, and packet-transport return are later milestones.

### Milestone 5: Splonks Integration

1. Route Splonks browser joins through the Realnet cascade.
2. Keep direct IP join as a separate explicit tool.
3. Show selected transport and connection phase in lobby/debug UI.
4. Validate same-machine, LAN, hotspot, and cross-network attempts.

### Milestone 6: Relay

1. Add relay protocol and authorization.
2. Add relay candidate type.
3. Add relay fallback after direct/punch failure.
4. Add basic bandwidth/rate diagnostics.

### Milestone 7: Steam

1. Map Steam lobby metadata to the same room/session contract.
2. Add Steam transport backend.
3. Decide when Steam-only sessions can or cannot cross-play with non-Steam.

## Open Questions

1. Should relay be built into `gubsy-roomd` initially or split immediately when
   implemented?
2. How much NAT type detection do we need, or should we simply attempt and
   measure?
3. Do we need WebSocket/WebRTC transports for browser-hosted games later?
4. What is the minimum dedicated-server helper API?
5. How should games expose "relay allowed" to players, if at all?

## Immediate Next Step

Implement Milestone 1 as code-facing docs and schema sketches, then make the
smallest roomd/client changes that let Splonks attempt a real public
cross-network join without router port forwarding.
