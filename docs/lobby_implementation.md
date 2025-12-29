# Lobby Implementation Plan

This document captures the agreed lobby flow and the UI screens to build in slices.

## Goals
- Keep the lobby UI generic and game-agnostic.
- Use the new menu system with small, focused screens (3–500 LOC each).
- Support local multiplayer with per-player profiles, input devices, and binds.

## Core Lobby Behavior
- The lobby always hosts a local session (even solo).
- Privacy options: **Solo / Couch / Friends / Invite Only / Anyone**.
- Max Players is hidden when Privacy is Solo or Couch.
- Local players count always includes player 0; never allow fewer than 1.
- Players panel in header:
  - Solo: `1 / 1`
  - Couch: `local_count / 4`
  - Other: `local_count / max_players`
- Game Settings button pushes to game-owned screens.
- Manage Mods opens Lobby Mods (session mod set).
- Browse Servers opens Server Browser (join list).

## Screen Map

### 1) Lobby (Session)
**Widgets**
- Lobby Name (TextInput)
- Privacy (OptionCycle)
- Max Players (OptionCycle, only when Privacy != Solo/Couch)
- Local Players (Button)
- Game Settings (Button)
- Manage Mods (Button)
- Start Game (Button)
- Back (Button)
- Browse Servers (Button)
- Players panel (header)

**Notes**
- Lobby name default: `"<ProfileName>'s Lobby"` with a fallback (e.g., "Local Lobby").
- Game Settings goes to game-owned screens (separate UI, not defined here).

### 2) Local Players List
**Widgets**
- More (Button) → `add_player()`
- Less (Button) → `remove_player(last)` but never below player 0
- Paged player slots (left/right paging)
- Slot button → Player Settings (for that player)
- Back

### 3) Player Settings (Per Player)
**Widgets**
- Profile (Button) → Profile Picker
- Inputs (Button) → Input Devices
- Binds (Button) → Binds Menu
- Back

### 4) Profile Picker
**Widgets**
- Search (TextInput)
- Paged list of profiles (select applies immediately)
- "Manage Profiles..." (Button) → existing Profiles UI (if present)
- Back

### 5) Input Devices
**Widgets**
- Paged list of **physical devices** (KBM + each connected controller)
- Toggle per device (enable/disable for that player)
- Back

### 6) Binds Menu
**Widgets**
- Binds Profile (Button) → Binds Profile Picker
- Edit Binds (Button) → existing binds editor flow
- Back

### 7) Binds Profile Picker
**Widgets**
- Search (TextInput)
- Paged list of binds profiles (single selection, immediate apply)
- "New" (Button, stub)
- Back

## Implementation Slices
1) Lobby Session page refactor + layout updates.
2) Local Players List screen + paging + add/remove.
3) Player Settings + Profile Picker + Input Devices.
4) Binds Menu + Binds Profile Picker + Edit Binds wiring.

## Open Wiring Questions
- Where the "Manage Profiles..." button should point (Settings → Profiles or dedicated screen).
- How to persist per-player device enablement in profile or session state.
