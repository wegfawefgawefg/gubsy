# Pre-Relay Cleanup

This is the cleanup pass before implementing `gubsy-relayd`. The NAT punch
path is working, but relay should not be built on top of temporary names,
game-specific policy, or incomplete platform support.

The goal is a clean Realnet foundation:

1. Gubsy owns connection planning.
2. Splonks and future games execute connection attempts through their packet
   transport.
3. Direct, NAT punch, relay, and Steam are connection candidates in one
   cascade.
4. Room directory, punch rendezvous, and relay responsibilities are explicit.
5. No game protocol details leak into `roomd`, `punchd`, or `relayd`.

## Decisions

### No Backward Compatibility Cleanup Debt

Splonks and Gubsy are unreleased internal G&K games/code. There are no old
external consumers to preserve.

Clean target:

1. Prefer correct names and clean APIs over compatibility aliases.
2. Remove the old `rendezvous_udp` health/capability alias.
3. Use `punch_udp` for NAT punch rendezvous.
4. Use `relay_udp` for relay availability.
5. Update Splonks and docs together instead of carrying migration shims.

### NAT Punch Implementation

The current NAT punch implementation is acceptable to keep building on.

Why:

1. It is low-volume rendezvous metadata, not game traffic.
2. Game packets stay opaque and continue through the game's UDP transport.
3. The service exchanges endpoint observations and credentials; it does not
   impose lockstep, host/client, authoritative-server, or peer-mesh rules.
4. It already validated the important public path with `gubsy-roomd` on a VPS
   and Splonks on separate machines.

What it does not solve:

1. Symmetric or restrictive NATs may still fail.
2. Firewalls can still block UDP.
3. Some users will need relay fallback.
4. Steam builds may eventually choose Steam Networking instead.

Real production shape:

```text
direct local/public -> NAT punch -> relay -> Steam transport where applicable
```

We do not need to replace the current punch code with a third-party library
right now. A library becomes interesting only if we decide to adopt a full ICE
stack, WebRTC data channels, or a platform service. For this game-general UDP
cascade, the right near-term work is completing diagnostics, service boundaries,
timeouts, and relay fallback.

### Windows Platform Support

Windows is not blocked from playing.

Current state:

1. Direct IP can still work on Windows.
2. NAT punch should still work on Windows.
3. The weak spot is local network interface detection.

The current Realnet local-interface detector returns no interfaces on Windows.
That means a private room-browser endpoint such as `192.168.x.x` may be judged
unreachable and skipped even when the Windows machine is actually on the same
LAN. The cascade can still fall through to NAT punch, but that is not the clean
desktop experience.

Clean target:

1. Implement Windows interface enumeration in Gubsy with `GetAdaptersAddresses`.
2. Fill IPv4 address, prefix length, loopback/private/public scope, and adapter
   usability.
3. Keep candidate classification in Gubsy.
4. Do not special-case this in Splonks.

### Relay Is Still The Next Transport

`relay_udp` is currently only a planned capability. That is fine for the
pre-relay phase, but the next transport implementation should be real packet
relay.

Relay target:

1. Allocate a relay session after direct and NAT punch fail.
2. Authorize both sides with room/join credentials.
3. Forward opaque UDP datagrams.
4. Keep bandwidth/accounting limits in relayd.
5. Expose diagnostics for allocation, active peers, bytes, packet loss, and
   shutdown reason.

### Latency Notes

The desktop-to-VPS ping measured during cleanup was:

```text
min/avg/max/mdev = 16.984/17.758/20.427/1.336 ms
```

So the tested VPS path was not meaningfully high latency from the desktop. The
earlier "high latency" wording was too loose. The remote smoke issue was not a
proven latency problem; it was a validation gate problem.

The NAT-punched game traffic is peer-to-peer after punch succeeds, not relayed
through the VPS. The VPS latency matters for room lookup and punch rendezvous,
but not necessarily for active gameplay packets after a successful punch.

## Hash Confirmation Gate

Splonks uses deterministic gameplay hashes during live lockstep. Those hashes
are still active and are the real desync detection mechanism.

Normal live behavior:

1. Peers periodically hash the deterministic gameplay state.
2. Hash packets include the lockstep frame and component hashes.
3. Matching hashes confirm a synchronized frame.
4. Mismatching hashes trigger rollback repair or snapshot catchup.
5. Missing hash packets do not automatically mean desync.

The "hash confirmation gate" was different. It was a smoke-test pass condition.

Old strict smoke condition:

```text
connected
in gameplay
past frame 60
join barrier inactive
no hash mismatch
no fatal desync
at least one confirmed matching hash exchanged
```

That last line, "at least one confirmed matching hash exchanged", made the
Realnet remote smoke wait for a sampled hash round-trip before declaring the
connection proof successful.

Why that can fail or be flaky without indicating gameplay is broken:

1. Hashes are sampled, not every frame.
2. Hash sending is intentionally suppressed during join barriers and snapshot
   catchup.
3. Fresh joins may ignore hashes through a settle window to avoid stale
   pre-barrier samples.
4. The smoke can prove transport, join barrier completion, active play, and
   movement before a confirmed hash has arrived.
5. A short remote validation timeout can expire before the first confirmed hash
   sample, even though lockstep is functioning.

Current Realnet smoke condition:

```text
connected
in gameplay
past frame 60
join barrier inactive
no hash mismatch
no fatal desync
movement proof on the joining client
```

This does not disable or weaken live desync checking. It only changes when the
Realnet remote smoke prints `REALNET_LAN_*_OK`.

Clean target:

1. Keep live hash exchange as the desync detector.
2. Keep hash mismatch as a smoke failure.
3. Do not require a confirmed matching hash for the basic Realnet connection
   smoke.
4. Add a separate longer lockstep-health validation when we specifically want
   to prove confirmed hash exchange on a network profile.
5. Name these validations differently so "connection proof" and "lockstep hash
   convergence proof" are not confused.

## Cleanup Checklist

1. Remove `rendezvous_udp` from roomd health/capabilities and clients.
2. Keep `punch_udp` as the only NAT punch capability name.
3. Implement Windows local-interface detection with `GetAdaptersAddresses`.
4. Add Realnet attempt timeline diagnostics:
   - room selected
   - candidates generated
   - direct candidate skipped/tried/failed
   - punch endpoint observed
   - punch hints exchanged
   - punch accepted by game transport
   - relay fallback selected
5. Move fixed punch timing constants into Realnet config.
6. Make punch timeout/failure reasons explicit.
7. Keep JSON rendezvous packets bounded and authenticated.
8. Keep game data opaque.
9. Add a dedicated long lockstep hash-convergence validation separate from the
   basic Realnet connection smoke.
10. Start relayd only after the above is clean enough that relay is just another
    candidate in the same cascade.

## Non-Goals

This pass does not:

1. Implement relay packet forwarding.
2. Replace the game UDP transport.
3. Add Steam transport.
4. Add peer-to-peer mesh.
5. Convert rendezvous control packets to binary.
6. Require complete IPv6 support before relay work begins.
