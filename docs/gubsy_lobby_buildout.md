# Gubsy Lobby Buildout

This document describes the reusable default lobby Gubsy should provide. The goal is functionality first, not a custom visual rebuild of older lobby screens.

The lobby should work as a game-agnostic session hub:

1. Add/remove local players without backing out.
2. Assign user profiles, binds profiles, input settings profiles, and devices per local player.
3. Host or join online sessions while preserving the local party.
4. Let the game provide game-specific mode/settings/character choices.
5. Start the game only after the lobby state is valid.

## Default Lobby Responsibilities

Gubsy owns generic session and player setup:

1. Lobby session state:
   - Lobby name.
   - Privacy/visibility.
   - Max players.
   - Online room state.
   - Selected local player index.
   - Local player readiness/validity.

2. Local player roster:
   - Always keep at least one local player.
   - Add local players from the lobby.
   - Remove local players from the lobby.
   - Support more than four local players with paging/scrolling.
   - Do not hardcode a practical local-player limit beyond sanity limits.

3. Per-player user profile:
   - Let each local player choose a user profile.
   - Pull remembered preferred binds/input settings from the selected user profile.
   - Persist changed preferences back to the user profile.

4. Per-player binds profile:
   - Let each local player choose a binds profile.
   - Keep the existing binds profile editor reachable.
   - Persist the selected binds profile id to `UserProfile::last_binds_profile_id`.

5. Per-player input settings profile:
   - Let each local player choose an input settings profile.
   - Persist the selected input settings profile id to `UserProfile::last_input_settings_profile_id`.

6. Per-player device assignment:
   - List keyboard/mouse aggregate plus each connected controller.
   - Toggle devices on/off per player.
   - Allow multiple devices per player.
   - Allow the same physical device to be assigned to multiple players.
   - Refresh safely on hotplug.

7. Start gating:
   - Require at least one local player.
   - Require joined local players to have valid user profiles.
   - Warn rather than crash on missing binds/input settings.
   - Call the host game start callback only after validation.

8. Online hosting:
   - Create/host a room from the lobby.
   - Support public/private-ish visibility when the backend supports it.
   - Show room code/status.
   - Keep all local players attached to the hosted session.

9. Online browsing/joining:
   - Browse available rooms.
   - Show lobby/in-game phase.
   - Join a room from the browser.
   - If the room is in lobby phase, land in the session lobby.
   - Preserve local players as the joining party where supported.

10. Session compatibility:
    - Use `SessionContract`.
    - Carry game version, net protocol, session phase, content/mod hash, and game config.
    - Games with no mods can leave mod/content fields empty/default.

## Default Menu Shape

Use the current boring stacked-card menu style. Do not require a custom lobby layout before functionality is correct.

Main lobby rows should be roughly:

1. Local Players
2. Game Mode / Game Settings
3. Host Game
4. Browse Servers
5. Start Game
6. Back

Subscreens can be simple:

1. Local players list.
2. Player settings.
3. Profile picker.
4. Binds profile picker.
5. Input settings profile picker.
6. Device picker.
7. Server browser.
8. Host-room setup.

## Game-Owned Lobby Layer

Gubsy should not hardcode game modes or game settings. The game owns this layer.

The game should provide a small lobby configuration API:

1. List available game modes.
2. Provide a label/description for each mode.
3. Provide mode-specific settings.
4. Provide per-player choices such as character selection.
5. Serialize the selected config into `SessionContract::game_config`.
6. Deserialize/validate a remote config when joining.
7. Tell Gubsy which fields are host-editable and which are client/local-only.

Examples:

1. A campaign mode can expose campaign/level/respawn settings.
2. A versus mode can expose team rules, score limit, rounds, and map pool.
3. A character-select field can be per-player and may be constrained by the host.

## Host And Client Authority

For online sessions:

1. Host controls session-wide game mode and game settings.
2. Clients can view host-controlled settings.
3. Clients control their own local party where allowed.
4. Per-player choices may be host-constrained.
5. Character selection should be represented as per-player lobby config, not as generic Gubsy profile data.

If a client joins a room with restrictions, the game layer validates its local choices and reports any required changes through the lobby UI.

## Splonks Interpretation

Splonks can use the default Gubsy lobby with a small game-owned config layer.

Initial Splonks game mode:

1. Campaign.

Likely Splonks settings:

1. Respawn policy:
   - Instant at level start.
   - At next level.
   - No respawn.
2. Character per local player:
   - Player.
   - Bee.
   - Meatboy-like character.

Splonks does not need mod management in the lobby right now. Mod/content fields in the generic session contract should remain available for other games and future Gubsy use.

## Build Order

1. Add reusable lobby state for local players and session metadata.
2. Add local player roster screen.
3. Add player settings screen.
4. Add profile picker.
5. Add binds profile picker.
6. Add input settings profile picker.
7. Add device picker.
8. Wire start validation.
9. Wire game-owned lobby config API.
10. Add Splonks campaign config.
11. Reconnect host/join/server browser flow.
12. Polish copy/navigation/status messages.

## Current Implementation Notes

The reusable local-player path is implemented through `GubsyLobbyState`.

Implemented now:

1. Local player roster state.
2. Per-player user profile selection.
3. Per-player binds profile selection.
4. Per-player input settings profile selection.
5. Per-player device toggles.
6. Start validation before game start callbacks.
7. Direct host/join callbacks through `GubsyLobbyCommands`.

The direct host/join callback path lets a game wire Gubsy's lobby UI to its own
tested transport code without putting game networking details in Gubsy. Splonks
uses this to call its existing `StartHostSession` and `JoinHostSession` entry
points.

Still remaining:

1. Real room-server browser UI backed by `RoomServerMatchmaking`.
2. Room-code join flow.
3. Periodic room heartbeat/refresh while hosted or joined.
4. Game-owned lobby config provider from `game_lobby_config_layer.md`.
5. Richer status copy for online failures and compatibility checks.

## Non-Goals For This Pass

1. No visual redesign requirement.
2. No mod management UI for Splonks.
3. No engine-specific campaign/versus assumptions.
4. No mandatory unique-device enforcement.
5. No deep menu system rewrite.
