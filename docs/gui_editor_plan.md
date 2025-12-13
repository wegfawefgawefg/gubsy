# GUI Editor Tooling Plan

## Overall Flow
- `Ctrl+E` toggles GUI edit mode. Entering shows the Screens view; exiting returns to the live menu/game.
- While in edit mode, the editor owns keyboard/mouse input. Escape unwinds layers:
  1. If inside a per-screen sub-mode (layout/nav/objects/warp/labels), `Esc` cancels that mode and returns to the neutral per-screen view.
  2. From the neutral per-screen view, `Esc` returns to the Screens overview.
  3. From the Screens overview, `Esc` exits edit mode entirely.
- `Ctrl+H` opens a simple help overlay listing the current context’s hotkeys; `Ctrl+H` (or `Esc`) closes it.

## Screens Overview
- Displays every registered screen/page as a card in a grid. Grid dimensions = `ceil(sqrt(screen_count))`.
- Each card renders a miniature representation (rect outlines + labels).
- Controls:
  - `Enter` / double-click card: dive into that screen.
  - `N` (while in overview) or a “New” button: create a placeholder screen entry.
  - `Delete`: delete a placeholder screen (only allowed if the screen has no hardcoded implementation).
  - `B`: enter “back-target” mode – click a source screen then its destination; stored per screen so runtime Esc/back follows it.
  - `W`: enter “warp overview” – allows connecting buttons across screens (see Warp Mode below).

## Per-Screen Neutral View
- Shows the current screen at full size plus an overlay toolbar (current resolution, page name, active mode indicator).
- Hotkeys (single letters, left-hand friendly):
  - `l`: Layout mode (drag/resize). `Ctrl+R` resets layout; `Esc` exits layout mode.
  - `n`: Navigation mode (see below). `Esc` exits.
  - `o`: Object mode (create/delete placeholders). `Esc` exits.
  - `w`: Warp mode (define cross-screen nav). `Esc` exits.
  - `f` or `Enter`: label edit mode on the widget under cursor; captures typing until `Enter` (accept) or `Esc` (cancel).
  - `h`: show help overlay (same as `Ctrl+H`).
  - `s`: toggle snap-to-grid when applicable (layout/object modes share the same snap flag).

## Layout Mode (`l`)
- Reuses existing layout tooling (Ctrl+L previously). Drag handles remain; reset moved to `Ctrl+R`.
- Grid snap toggle stored per screen/resolution.
- Labels remain visible for context.

## Object Mode (`o`)
- Purpose: create placeholder rectangles even if no real button exists yet.
- Controls:
  - `Enter` (while mode active): spawn a new placeholder centered at cursor.
  - Drag edges/corners like layout mode.
  - `Delete`: remove selected placeholder.
  - `d`: duplicate selected placeholder.
  - `f`/`Enter`: rename the placeholder (same 32-char inline editor).
- Stored in a new INI section (e.g., `config/menu_objects.ini`) keyed by screen name + resolution.

## Label Editing
- Neutral screen view: hover widget, press `f` or `Enter` to edit its label.
- Inline textbox appears atop the widget; typing is captured (letters, numbers, dash, underscore, space? TBD).
- `Enter` saves; `Esc` cancels.
- Labels live inside layout/object data so they persist per resolution.

## Navigation Mode (`n`)
- While active:
  - `W`, `A`, `S`, `D` or arrow keys arm the corresponding direction from the currently selected widget.
  - `Space`/`Enter` confirm the currently hovered widget as the target (mouse hover). Alternatively, click to set.
  - `Delete` clears the armed direction.
  - `Tab` cycles selection through widgets; mouse click also selects.
  - Lines are drawn from edges (right edge to left edge, etc.) with slight offsets to avoid overlap; text label floats near the middle showing the direction name.
- Overrides are per screen + resolution (`[screen 1280x720] id=U:...`).

## Warp Mode (`w`)
- Lets you connect widgets across screens (e.g., Main Menu “Options” button → Options screen).
- Flow:
  1. Select source widget (click or Tab).
  2. Press `Space`/`Enter` (or click) to arm.
  3. In warp mode, the Screens grid appears in a sidebar; clicking a target screen zooms it momentarily so you can click its widget.
  4. Once a target widget is chosen, a warp entry is recorded (source screen/button → target screen/button).
- Warp entries live alongside navgraph data but tagged with destination screen names.
- At runtime, activating a widget with a warp entry switches to the destination screen before running any widget-specific callback.

## Back Targets
- Defined from the Screens overview (`B` hotkey).
- Click source screen, click destination screen. Stored as `back=screen_name` in a simple config file.
- Runtime behavior: pressing Esc/back while on that screen triggers the linked screen if the current page doesn’t override it in code.

## Help Overlay (`Ctrl+H` or `h`)
- Simple text panel listing:
  - How to exit editor (Esc layers).
  - Mode hotkeys (`l`, `n`, `o`, `w`, `f`, `Ctrl+R`, `h`).
  - Navigation mode controls (WASD/Space/Delete).
  - Object mode controls (Enter/Delete/D/f).
  - Warp/back instructions.
  - Reminder that edits are per resolution.

## File Structure
- `src/main_menu/editor/` folder containing:
  - `editor_state.hpp/cpp`: shared structs, mode flags, entry/exit logic.
  - `editor_screens.cpp`: grid view + screen management/back targets.
  - `editor_layout.cpp`: wraps layout mode (existing code can move here).
  - `editor_nav.cpp`: navgraph editor (existing code adapted).
  - `editor_objects.cpp`: placeholder creation + storage.
  - `editor_help.cpp`: renders help overlay.
- Existing menu files (`menu_step.cpp`, `menu_render.cpp`, etc.) call into the editor module when `Ctrl+E` is toggled, but the heavy logic stays isolated.

## Migration Strategy
- Keep current menu callbacks and behavior logic intact. The editor only supplies layout/nav metadata.
- Adopt screen-by-screen. Start with the Lobby/Game Options page as the canonical demo to prove the workflow.
- For each screen we migrate:
  1. Author default layout/nav/object definitions via the editor (ini files).
  2. Update that screen’s button-building/rendering path to query editor data (`maybe_apply_editor_layout`, `maybe_use_editor_nav`, etc.).
  3. Verify behavior matches existing UI (callbacks untouched).
  4. Once stable, remove hardcoded rects/nav for that page.
- When seeding a screen, we can pre-populate the INI files with current hardcoded positions so the editor starts with the existing layout.
- Existing screens that haven’t been migrated continue using hardcoded rectangles/nav graphs until their turn.

## Persistence
- Layout rectangles: continue using `config/menu_layouts.ini`.
- Nav graph edges (including warps): `config/menu_nav.ini`.
- Objects + labels: new `config/menu_objects.ini`.
- Screens/back targets: `config/menu_screens.ini`.
- All files scoped by `screen_name WIDTHxHEIGHT` sections to keep per-resolution variants.
