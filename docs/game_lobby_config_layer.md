# Game Lobby Config Layer

Gubsy should provide the reusable lobby shell. The game should provide the game-specific lobby configuration that sits inside that shell.

This keeps Gubsy useful across games without forcing every game into the same campaign/versus/settings model.

## Responsibilities

Gubsy owns:

1. Local player roster.
2. User profile selection.
3. Binds/input settings profile selection.
4. Input device assignment.
5. Host/join/browse room flow.
6. Session contract transport.
7. Generic start validation.

The game owns:

1. Available game modes.
2. Mode-specific settings.
3. Per-player game choices.
4. Serialization into `SessionContract::game_config`.
5. Validation of remote lobby config.
6. Actual game start behavior.

## API Shape

The game should register a lobby config provider with Gubsy.

The provider has two jobs:

1. Describe boring editable fields that Gubsy can render with default widgets.
2. Register custom game-authored panels/screens for rich UI that descriptors
   cannot express well.

This matches the normal engine shape: the framework owns screen routing, stack
behavior, input handoff, and session lifecycle; the game authors the actual
rich UI where game-specific presentation matters.

Required descriptor data:

1. Game mode id.
2. Game mode label.
3. Game mode description.
4. Mode-specific setting descriptors.
5. Per-player choice descriptors.
6. Default config.

Optional custom UI data:

1. A game mode panel hook for rich mode-specific configuration.
2. A per-player panel hook for rich character/loadout/team selection.
3. A summary panel hook for showing game-specific lobby state.
4. A help/description hook for selected game-specific fields.

Required callbacks:

1. Build default config.
2. Validate config.
3. Serialize config to JSON or sexpr-backed data.
4. Deserialize config from a remote session.
5. Apply local per-player choices.
6. Start the game from validated lobby state.

The initial implementation should stay low abstraction. A small struct of
function pointers is enough.

## UI Driving Model

Gubsy should not try to model every possible game-specific lobby widget as
data. That becomes restrictive quickly for character select screens, animated
previews, sprite panels, custom descriptions, loadout previews, and similar
game-specific presentation.

Use a hybrid model instead:

1. Gubsy owns the reusable shell.
2. Gubsy owns navigation between lobby sections.
3. Gubsy owns local player/profile/input/session lifecycle.
4. Gubsy can render default descriptor-backed fields for simple settings.
5. The game can provide custom panels for anything rich or highly specific.
6. Custom panels read and mutate the game-owned lobby config through the
   provider.
7. Custom panels must not own transport, room membership, local player roster,
   or profile/input assignment.

The practical shape should be boring:

```cpp
struct LobbyPanelContext {
    Rect area;
    int player_index;
    LobbyState& lobby;
    GameLobbyConfig& config;
    MenuInput input;
};

using LobbyPanelFn = void (*)(LobbyPanelContext&);
```

The exact types can change during implementation, but the boundary should stay
the same: Gubsy gives the game a slot and context; the game draws/updates its
own rich game-specific panel inside that slot.

## Engine Comparison

This should follow the common engine pattern rather than trying to make a
universal data-only UI.

1. Unreal Common UI has framework-owned activatable widget stacks, while games
   still author and push their own widgets.
2. Godot commonly keeps lobby/session/network state separate from custom
   `Control` scenes and signals.
3. Unity Lobby is a service/data layer; the game authors the lobby UI with
   uGUI/UI Toolkit or its own screens.

The useful pattern is not "Gubsy owns all lobby UI." The useful pattern is
"Gubsy owns the reusable shell and lifecycle; games own rich game-specific
panels."

## Mode-Specific Settings

Different game modes can expose different settings.

Examples:

1. Campaign:
   - Campaign.
   - Starting level.
   - Difficulty.
   - Respawn policy.

2. Versus:
   - Map.
   - Team size.
   - Score limit.
   - Round count.
   - Friendly fire.

3. Survival:
   - Arena.
   - Wave preset.
   - Item density.

Gubsy should not know what these fields mean. It only needs descriptors that let it render and edit them.

For richer mode setup, the game should register a custom mode panel instead of
forcing all controls through descriptors.

## Per-Player Choices

Per-player game choices are separate from user profile and input settings.

Examples:

1. Character.
2. Team.
3. Color.
4. Loadout.

These choices belong to the active lobby session. They should not be stored as generic user profile data unless the game explicitly wants preference persistence.

Simple choices can use descriptors. Rich choices, such as animated character
select cards or loadout previews, should use a game-authored per-player panel.

## Host And Client Rules

The provider should mark fields by authority:

1. Host-only session fields.
2. Client-editable local player fields.
3. View-only replicated fields.

When joining another player’s lobby, Gubsy displays host-owned fields as read-only unless the game/provider says otherwise.

## Splonks Starting Point

Splonks starts simple:

1. One mode: `campaign`.
2. Session setting: respawn policy.
3. Per-player choice: character.

Respawn policy options:

1. Respawn friends instantly at level start.
2. Respawn friends at next level.
3. No respawn.

Character options:

1. Player.
2. Bee.
3. Meatboy-like character.

Splonks can add campaign/level selectors later without changing Gubsy’s generic lobby shell.

## Open Questions

1. Should game lobby config descriptors reuse the existing settings schema, or should they be separate lightweight descriptors?
2. Should `game_config` serialization be JSON only because `SessionContract` already uses JSON, or should Gubsy also support sexpr-backed local persistence?
3. How much per-player choice validation should happen live versus only on start?
