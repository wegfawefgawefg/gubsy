# Layout Editor Plan

## Goals
- Ship an **engine-side** layout editor (similar to the debug overlays) so every game can author/adjust `UILayout` data at runtime.
- Make the editor modal: when it is active, all keyboard/mouse/gamepad input is captured by the editor so menus/gameplay logic are paused.
- Provide intuitive mouse-driven manipulation plus discoverable hotkeys for selection, duplication, copy/paste, rename, undo (later), and saving.
- Persist edits back to `data/ui_layouts/layouts.sxp` (or a temp file) with `Ctrl+S`.
- Support multi-selection (drag box, Ctrl+click) with group move/scale and copy/paste.
- Provide undo/redo (snapshot-based) so experiments are reversible.
- Allow virtual-resolution preview: render the UI/layout into an offscreen target with any width/height/aspect and scale it to the actual window so designers can inspect 2560×1080, 4:3, etc. without resizing the monitor.

## Toggle & Lifecycle
- Hotkey: `Ctrl+L` toggles layout edit mode (engine-wide). Future: hook into the ImGui debug HUD with a checkbox.
- When active:
  - Display an overlay banner: current layout ID, resolution, snap state, instructions (`Ctrl+S Save`, `Ctrl+Z Undo`, `Esc Exit`, etc.).
  - Consume all input until the user presses `Esc` (or toggles `Ctrl+L` again). `Esc` steps through sub-modes (dragging → neutral → exit).
  - Pause menu navigation/game use actions.
- When inactive: editor state persists (selected object, snap setting) but no overlays are rendered.

## Interaction Basics
- **Viewport Overlay**
  - Draw the current `UILayout` objects as translucent rectangles with labels, IDs, and normalized rect text.
  - Each object shows drag handles: corners (resize both axes), edges (resize one axis), center (move). Active handle highlights.
  - Show pixel coordinates while dragging; hold `Shift` for coarse snap, `Ctrl` for fine increments (configurable grid).
  - Multi-select: click-drag marquee, `Ctrl`+click toggles selection, all transforms apply to the selection set.
- **Selection**
  - Click to select. `Tab` cycles through objects. Selected object shows bounding box + info panel (label, ID, normalized xywh, pixel xywh).
  - `Enter` / double-click opens inline label rename.
- **Manipulation**
  - Drag handles as described. While dragging, clamp to [0,1] normalized range and optionally snap to other objects (magnetic guides).
  - Keyboard nudging (`Arrow keys`) moves the selected object; add `Shift` modifiers for 10x steps.
  - `Ctrl+C` / `Ctrl+V` copy/paste the selected object within the same layout/res (new object gets a unique ID).
  - `Delete` removes the object (with undo once implemented).
- **Saving**
  - `Ctrl+S` writes the edited layout back to `data/ui_layouts/layouts.sxp` by calling `save_ui_layout`. For safety, also mirror to `data/ui_layouts/layouts.sxp.bak`.
  - Show toast in overlay (“PlayScreen 1920x1080 saved @ 12:34:56”).
  - Future: auto-save checkpoint + undo stack.
- **Grid & Rulers**
  - Always draw a faint normalized grid with numeric tick labels along X/Y (0.00–1.00). Provide `-`/`=` hotkeys to shrink/grow spacing (step 0.01, with key-repeat).
  - Snap-to-grid toggle (`G` or toolbar button). Modifier keys override snap temporarily (`Shift` = coarse, `Ctrl` = fine/no snap).

## Architecture
- New module `src/engine/layout_editor/` containing:
  - `layout_editor.hpp/cpp`: entry points (`begin_frame`, `handle_input`, `render_overlay`, `shutdown`), activation state, and per-layout editing state.
  - `layout_editor_gizmos.cpp`: hit-testing, dragging, snapping logic.
  - `layout_editor_ui.cpp`: ImGui panels for label editing, property inspector, instructions.
  - Keep each file ≤ ~400 lines. Shared structs (SelectedObject, DragState) go into `layout_editor_state.hpp`.
- Integration points (engine-owned):
  1. After `imgui_new_frame`, call `layout_editor_begin_frame`.
  2. If editor is active, skip `mode->process_inputs_fn`/menu input (or have the editor tell the main loop to early-out).
  3. During render, draw the layout overlay before ImGui debug/render calls.
  4. On shutdown, clear editor state.
- Editor uses current `UILayout` from `get_ui_layout_for_resolution` (the one used by the active mode). Later we can add a picker to swap layouts/res without changing the main view.

## Data Flow
- Editor operates on the `UILayout` instance currently in `es->ui_layouts_pool`. Mutations edit this in-place.
- Saving rewrites the corresponding `(id, width, height)` entry in `data/ui_layouts/layouts.sxp`.
- Clipboard stores one or multiple `UIObject` structs (IDs remapped via `generate_ui_object_id()` when pasted).
- Snap settings, per-res preferences stored in `config/layout_editor.ini` (per layout ID + resolution).
- Undo/redo: keep a rolling buffer (e.g., 20 entries) of serialized layout snapshots (JSON or direct structs). Each edit commits a new snapshot; undo restores the previous one.

## Future Enhancements
- Undo/redo stack (timeline of edits with `Ctrl+Z` / `Ctrl+Y`).
- Multi-selection with align/distribute tools.
- Visual grid toggle (configurable spacing).
- Highlight object when hovered during gameplay even outside editor (helpful for debugging).
- Integration with navigation editor (`n` mode) from `docs/gui_editor_plan.md` to provide a unified workflow.
