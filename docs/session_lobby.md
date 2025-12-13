# Session Lobby Plan

Goal: After clicking `Play` from the title menu, drop players into a “Session Lobby” where they configure the upcoming run (game settings, mods, players). This replaces the current immediate jump into gameplay.

## Layout Overview

Think of Left 4 Dead’s lobby flow: a single panel with stacked sections. Ours should stay dense (more info per screen than the current menu grid) but still navigable via keyboard/controller.

### Sections
1. **Session Info**
   - Lobby name (defaults to player name or “Local Session”).
   - Privacy selector (Solo / Friends / Public). For now this only toggles a label.
   - Invite button (greyed out until networking arrives).
   - Connected player list with slots (1–64, but show the first few with scroll arrows).

2. **Game Settings**
   - Map/Scenario dropdown (demo: “Demo Pad Arena”).
   - Difficulty preset (Easy / Normal / Hard).
   - Player count spinner (min 1, max 64).
   - “Allow Join” or “Lock Lobby” toggle.
   - Optional Seed input (text box with “Randomize” button).

3. **Active Mods**
   - List of installed mods with checkboxes.
   - Required mods locked on.
   - Hover/selected description panel showing version, author, dependencies.
   - If a dependency is missing, show warning and offer “Install” (launch Mod browser pre-filtered).

4. **Footer Actions**
   - `Start Game` button (disabled until required mods + at least one player ready).
   - `Back` button to return to title.

## Menu Flow

1. Title → Play → push new `Page::LOBBY`.
2. Focus defaults to “Start Game” if everything is valid; otherwise first invalid field.
3. Up/Down navigates sections (Session Info → Game Settings → Mods → Footer). Within each section, Left/Right toggles per-field.
4. Pressing Start confirms, persists lobby settings (including selected mods) into state, and triggers the actual gameplay loading (load_demo_item_defs for enabled set, etc.).

## Implementation Notes

- Add `Page::LOBBY` to the menu state.
- Store lobby config in `State::LobbySettings` struct (map id, difficulty enum, max players, allow join flag, seed string, enabled mod ids, lobby name/privacy, pending invites placeholder).
- Reuse existing button rendering where possible but allow custom panel drawing for the dense layout.
- When toggling mods here, update a “enabled mods for next session” list; the global installer already handles on-disk copies.
- `Start Game` should error if a required dependency is disabled. Show alert/toast describing which mod is missing or not installed.
- Once networking/ui work arrives, the “Invite” button can integrate with actual services without redesigning the screen.

## Next Steps

1. Add the Lobby state structure and default values.
2. Build the new menu page (buttons, nav nodes, rendering).
3. Wire Play button to enter Lobby instead of directly switching modes.
4. Hook Start Game to load mods/settings and transition to PLAYING.
5. Persist lobby settings if needed (future saves).
