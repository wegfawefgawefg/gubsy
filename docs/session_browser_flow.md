# Session Browser Flow

This documents the intended behavior for session discovery, joining, and pre-game lobbies.

## Core Requirement

Joining from the game browser should not always mean the same thing.

A discovered session can be in one of multiple phases:

- `lobby`: players are gathered before the run starts
- `in_game`: the session is already running

Later we may add more phases such as `loading`, `paused`, or `post_game`, but `lobby` and `in_game` are the minimum useful set.

## Join Semantics

When a player joins a discovered session:

- if the session is in `lobby`, the join should land them in the active session lobby
- if the session is in `in_game`, the join should land them in the running session and trigger whatever game-specific join/snapshot flow is required

The browser should surface which phase a session is in before the user joins.

## Lobby Behavior

A pre-game lobby is live shared state, not just a local menu.

That means:

- the member roster is shared
- the session name is shared
- the host-controlled game settings are shared
- clients in the lobby should see host edits show up without leaving/rejoining

What counts as "game settings" is game-defined.

The engine should not assume a universal set of settings beyond generic session metadata.

## Engine / Game Split

The engine should provide:

- browser/session discovery
- room/session membership
- session phase
- generic replicated metadata channels
- generic runtime sync/session orchestration

The game should provide:

- the game-specific lobby config schema
- serialization/deserialization of that config
- the lobby UI for editing or viewing that config
- the rules for whether clients can edit, observe, or join mid-game

## Current Demo-Game Interpretation

For the bundled demo game, the lobby config currently means simple run-setup state like:

- scenario selection
- seed mode
- fixed seed text

Those are just example game-defined fields. Other games built on Gubsy should be able to define different lobby config without rewriting the engine browser/session lifecycle.
