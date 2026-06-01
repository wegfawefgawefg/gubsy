# Multiplayer Backend Plan

This is the target plan for Gubsy multiplayer discovery, hosting, joining, and
Steam integration.

The next concrete phase of this work is Realnet. See `realnet_plan.md` for the
internet connectivity plan covering room candidates, UDP rendezvous/NAT
punch-through, relay fallback, authority modes, and Steam as a future backend.

The goal is one user-facing lobby flow that can use multiple backends:

1. Direct IP and port.
2. Server browser through a small directory service.
3. Steam lobby and invite flow.

Games should not need to rewrite their lobby UI for each backend. A game should
provide its session config and networking callbacks once, then Gubsy should make
that session reachable through the enabled backends.

## Core Direction

Gubsy owns the generic multiplayer shell:

1. Lobby UI.
2. Local player setup.
3. Session discovery.
4. Session metadata.
5. Host/join/leave lifecycle.
6. Backend selection.
7. Steam/non-Steam integration points.

The game owns game-specific networking:

1. Simulation model.
2. Packet payloads.
3. Determinism, rollback, prediction, or snapshot policy.
4. Game-specific lobby config.
5. Actual game start/load behavior after a session is accepted.

For Splonks, the active game networking direction is input lockstep/rollback.
Gubsy should not force host-authoritative gameplay replication onto Splonks.
Gubsy should only provide the session and transport plumbing needed to reach the
Splonks lockstep entry points.

## Discovery Versus Transport

Discovery and transport are separate responsibilities.

Discovery answers:

1. What sessions exist?
2. Can this client join them?
3. What session contract is the host advertising?
4. What endpoint or token should the client use to connect?

Transport answers:

1. How do packets move between peers?
2. Is traffic direct UDP, relay, Steam networking, or something else?
3. What peers are connected?
4. How do we send and receive opaque payload bytes?

Do not merge these into one design. A Steam lobby can point to Steam transport.
A directory listing can point to direct UDP. A future directory listing can point
to a relay token.

## Required Join Methods

### 1. Direct IP Join

Direct IP join is the baseline.

Required behavior:

1. Host screen shows the listen port.
2. Host screen shows useful local/LAN addresses when available.
3. Join screen accepts `host:port`.
4. Join screen accepts separate host and port fields if that is easier in the
   menu.
5. Direct join works without a room server, Steam, or internet service.

Limitations:

1. Same-LAN direct join should work normally.
2. Internet direct join usually requires port forwarding.
3. Direct join does not solve NAT traversal by itself.
4. Direct join does not create a public server browser entry by itself.

### 2. Server Browser Join

Server browser join is preferred over room-code join when a directory service is
available.

Required behavior:

1. Host can publish a session to a configured directory service.
2. Browser can list public sessions from that directory service.
3. Browser shows session phase: lobby or in-game.
4. Browser shows compatibility status before joining.
5. Browser shows ping/age/status when available.
6. Join from browser uses the same session contract path as direct join.

The directory service is not the gameplay server. It is a server server:

1. It stores advertised sessions.
2. It returns session lists.
3. It returns a selected session contract and endpoint.
4. It accepts heartbeat/refresh from hosts.
5. It expires dead rooms.
6. It does not carry gameplay traffic in the first implementation.

### 3. Steam Lobby And Invite Join

Steam is a first-class backend.

Required behavior:

1. Steam build can create a Steam lobby for a hosted session.
2. Steam build can list/join Steam lobbies.
3. Steam build can accept Steam friend invites.
4. Steam lobby metadata carries the same session contract fields as the
   directory service.
5. Steam transport can be used when enabled.

Steam should not replace the generic Gubsy session model. It should implement
the same matchmaking/discovery and transport interfaces as the non-Steam path.

## Room Codes

Room codes are optional convenience, not the primary model.

Server browser is better when browsing is available because the user can see
sessions instead of typing a code.

Keep room-code support only if it remains cheap:

1. A room code can be a short alias for a directory-service session.
2. It is useful for private games without exposing the full server list.
3. It is useful when a player wants to tell a friend one small token.
4. It should not be the only way to join.

If room codes create extra complexity, they can be treated as a directory
service feature rather than a separate backend.

## Directory Service

Gubsy should ship a small standalone directory service binary.

Suggested name:

1. `gubsy-roomd`

Responsibilities:

1. Listen on HTTP or HTTP plus WebSocket.
2. Accept room creation from a host.
3. Accept host heartbeat/update.
4. List public rooms.
5. Fetch one room by ID/code.
6. Expire rooms when heartbeat stops.
7. Report simple health/version info.

Non-responsibilities:

1. No gameplay packet relay in the first implementation.
2. No account system in the first implementation.
3. No authoritative game simulation.
4. No game-specific session rules.
5. No mod hosting unless the mod/content service is explicitly added later.

Data stored per room:

1. Stable room ID.
2. Optional short room code.
3. Display name.
4. Visibility: public or unlisted.
5. Session phase.
6. Player counts.
7. Host age/last heartbeat.
8. Advertised connect endpoint.
9. Session contract.
10. Optional backend-specific metadata.

## VPS Tooling

Gubsy should include boring tooling for running the directory service on a VPS.

Target first host:

1. Debian stable.

Required files:

1. Build instructions for `gubsy-roomd`.
2. A systemd service file.
3. An install script.
4. A config file example.
5. A firewall note for the required port.
6. A reverse-proxy note if using nginx/caddy later.
7. A smoke command that verifies the service is reachable.

User goal:

1. Rent a VPS.
2. Install Debian.
3. Clone or copy the built server package.
4. Run one setup script.
5. Point the game at `https://rooms.example.com` or `http://ip:port`.
6. Browse hosted sessions from the game.

The first implementation can use HTTP without accounts. Production public
service should use HTTPS.

## Steam And Non-Steam Cross-Play

Cross-play should be a target, but the transport backend matters.

Cases:

1. Steam player hosts through direct UDP and publishes to the directory service.
   Non-Steam players can join if they can reach the direct endpoint.
2. Non-Steam player hosts through direct UDP and publishes to the directory
   service. Steam players can join if they can reach the direct endpoint.
3. Steam player hosts with SteamNetworkingSockets/Steam relay only. Non-Steam
   players probably cannot join unless we provide a non-Steam endpoint or relay.
4. Steam lobby discovery is only visible to Steam users unless mirrored into the
   directory service.

Target policy:

1. Steam builds should still be able to publish a non-Steam direct endpoint when
   the host enables it.
2. Steam lobbies and the Gubsy directory service should both carry the same
   session contract.
3. Steam-only relay sessions may be Steam-only.
4. Cross-play should be explicit in the browser UI: joinable, incompatible, or
   requires Steam.

## Backend Shape

The engine should expose a small backend boundary.

Discovery backend:

1. Create session advertisement.
2. Update session advertisement.
3. Delete/leave session advertisement.
4. List sessions.
5. Fetch session by ID/code.

Transport backend:

1. Listen.
2. Connect.
3. Disconnect.
4. Send opaque bytes.
5. Poll received opaque bytes.
6. Report peer connection state.

Expected implementations:

1. `DirectIpTransport`.
2. `DirectoryMatchmaking`.
3. `SteamMatchmaking`.
4. `SteamTransport`.
5. `NullMatchmaking` for offline/no-service builds.

Avoid a large abstract framework. Keep the interfaces narrow and concrete.

## Menu Flow

Host screen:

1. Local game.
2. Host public listed game.
3. Host unlisted game.
4. Listen port.
5. Directory service URL.
6. Steam lobby enable/disable when Steam is available.
7. Advertised endpoint preview.
8. Host button.

Join screen:

1. Browse servers.
2. Direct IP join.
3. Room code join if enabled by the directory service.
4. Steam friends/invites when Steam is available.

Browser rows:

1. Session name.
2. Host/backend.
3. Player count.
4. Phase.
5. Compatibility.
6. Ping/age.
7. Join action.

## Validation Gates

The implementation is not complete until these pass:

1. Two normal Splonks windows can host and join by direct IP through the menu.
2. Two normal Splonks windows can host and join through the server browser using
   a local `gubsy-roomd`.
3. The server browser shows lobby and in-game phases correctly.
4. Incompatible session contracts are visible and blocked before transport join.
5. The room service can be killed and restarted without crashing clients.
6. Dead hosted rooms expire from the browser.
7. Direct IP still works when no directory service is configured.
8. Steam-disabled builds compile and run without Steam SDK files.
9. Steam-enabled builds can create/list/join Steam lobbies once Steamworks is
   available.
10. Steam invites enter the same join path as browser/direct joins.
11. Cross-play behavior is explicit in UI and docs.

## Build Order

1. Normalize Gubsy session backend interfaces around direct IP, directory
   discovery, and Steam discovery.
2. Make the Gubsy menu expose direct IP host/join clearly.
3. Validate two-window Splonks direct IP host/join through the menu.
4. Build `gubsy-roomd` as a standalone binary.
5. Add local directory-service smoke tests.
6. Wire Gubsy host publishing and browser listing to `gubsy-roomd`.
7. Validate two-window Splonks browser host/join through the menu.
8. Add VPS packaging: config, install script, systemd unit, and README.
9. Add room-code alias support only if it stays small.
10. Add Steam compile gates and null backend.
11. Add Steam lobby metadata mapping.
12. Add Steam invite/join flow.
13. Add Steam transport.
14. Validate Steam-only and Steam/direct cross-play cases.

## Resolved Design Decisions

1. Directory service protocol:
   - Use HTTP first.
   - Discovery is low-frequency metadata traffic, not gameplay traffic.
   - Browser refresh can poll HTTP.
   - Add WebSocket/SSE later only if live browser updates or hosted lobby
     metadata pushes become worth it.

2. Directory service implementation:
   - Implement `gubsy-roomd` in C++ inside the Gubsy repo.
   - Keep it a standalone binary.
   - Keep it boring: request parsing, in-memory room table, expiry, and simple
     JSON responses.

3. Public directory auth:
   - Directory auth is for publishing/updating/deleting advertised rooms, not
     for friend invites.
   - Joining a passworded game is a session/game policy, not directory admin
     auth.
   - First implementation should support:
     1. Public listed rooms.
     2. Unlisted rooms hidden from browse results.
     3. Optional per-room join password flag/metadata.
     4. A host update token returned at room creation and required for
        heartbeat/update/delete.
   - A shared directory admin token can be added for private deployments if
     needed, but normal public browsing should not require user accounts.

4. Relay metadata:
   - The browser UI should stay game-generic and backend-generic.
   - A browser row should not care whether the endpoint is direct UDP, Steam, or
     a later relay.
   - Session endpoint metadata should therefore carry a transport kind plus
     opaque connection data.
   - This keeps relay support possible without making every game customize its
     server browser.

5. Room codes:
   - Do not make room codes a required path.
   - Server browser and direct IP are more useful.
   - If room codes remain, treat them as optional aliases for unlisted directory
     rooms.
   - Do not let room-code support block direct IP, browser join, or Steam join.

6. Steam lobby mirroring:
   - Steam-hosted public games should be able to publish to the Gubsy directory
     when they also expose a non-Steam-reachable endpoint.
   - Steam-only relay sessions can stay Steam-only.
   - The browser should clearly label whether a session is joinable by this
     build, requires Steam, or is incompatible.

7. Response format:
   - Use JSON for the `gubsy-roomd` HTTP protocol.
   - JSON is better for curl, browser inspection, VPS debugging, and service
     tooling.
   - Gubsy can still use S-expressions for local data files.

8. HTTP implementation:
   - Vendor a small C++ HTTP implementation instead of inventing a custom HTTP
     server.
   - Keep the vendored code isolated and document its source/version.
   - Do not make normal Gubsy game consumers think about the HTTP dependency
     unless they build `gubsy-roomd` or the directory backend.

9. Directory persistence:
   - Start with an in-memory room table.
   - Add structured request/room lifecycle logs for debugging.
   - Do not persist room state for correctness; rooms are live presence data and
     should expire naturally after heartbeat loss.
