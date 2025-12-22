# ImGui Debug Windows Plan

## Goals
- Provide always-available developer diagnostics without affecting the main menu or gameplay UI.
- Use Dear ImGui to render lightweight overlay windows that can be toggled independently.
- Keep windows movable/dockable so developers can organize their workspace.

## Toggle Strategy
- Render engine-owned overlay controls (no game deps) from a new `engine/imgui_debug` module.
- Add a slim “Debug HUD” bar that can be hidden entirely (master toggle + hotkey).
- Provide hotkeys (default: `F1–F4`) to toggle individual windows even when the bar is hidden.
- Persist visibility flags and “bar hidden” state in `data/debug_overlays.lisp` (ImGui still handles window positions).

## Windows

### 1. Player / Devices Monitor
- One collapsing section per player index.
- Show:
  - Player ID and name.
  - Bound user profile ID, settings schema ID, binds profile ID.
  - List of devices contributing input (keyboard, mouse, controller IDs).
  - Live indicators (e.g. green dot) when input arrives from that device.
  - Current InputFrame bits/analogs decoded (menu + gameplay actions).

### 2. Binds Profiles Inspector
- List all available `BindsProfile` entries.
- Each entry expands to show button binds and analog binds.
- Provide quick filters (text box + category dropdown).
- Include “jump to player” buttons that show which players reference the profile.

### 3. UI Layout Browser
- List loaded `UILayout` entries grouped by label.
- For each layout:
  - Resolution metadata.
  - Table of `UIObject`s (id, label, normalized rect, world rect for current resolution).
  - Button to highlight the object in-world (draw temporary overlay rectangle).
  - Export button to dump layout JSON/S-expression for debugging.

### 4. Future Slots
- Leave room for additional panels (mod loader status, network sync, etc.). Add placeholder toggles but keep them disabled until implemented.

## Implementation Notes
- Create `engine/imgui_debug.hpp/.cpp` to own state + rendering. This module ships with the engine; game-specific overlays can live in game code without touching it.
- Provide an initialization hook from `engine/render.cpp` after ImGui layer setup.
- Keep module files small (< ~400 lines each) and group functionality under `src/engine/imgui_debug/`.
- Use ECS/global state accessors but keep drawing code read-only; no mutations inside ImGui except toggles and highlight requests.
- Guard all data access with `if (!es || !ss)` to avoid crashes when the engine is booting or shutting down.

## Next Steps
1. Add the new ImGui debug module with window toggles and hotkeys.
2. Implement the Player/Devices monitor first (highest debugging value).
3. Follow with Binds inspector and UI layout browser.
4. Iterate on styling (consistent colors/fonts) once functionality is stable.
