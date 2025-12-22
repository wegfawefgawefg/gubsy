# Layout Editor

The runtime layout editor now ships with the engine and is the canonical way to author and tweak `UILayout` data. This document describes its behaviour and the current “nice to have” backlog.

## Overview
- Modal editor toggled with `Ctrl+L`. While active, all mouse/keyboard/gamepad input is captured and gameplay/menu logic pauses.
- Overlay shows the active layout (ID, resolution, form factor) plus instructions (`Ctrl+S` save, `Ctrl+Z/Y` undo/redo, etc.).
- Working copy is the `UILayout` instance from `es->ui_layouts_pool`. Edits mutate it in place and can be persisted with `save_ui_layout`.
- Resolution preview can be overridden via the debug “Video & Resolution” window so layouts are authored at arbitrary aspect ratios without resizing the OS window.
- Undo/redo is snapshot-based (layout + object list) so every drag/rename/copy/paste is reversible.

## Interaction
- Grid + rulers are always visible with numeric normalized ticks. `=` / `-` change spacing, `G` toggles snap-to-grid (holding `Shift` / `Ctrl` temporarily overrides).
- Objects draw translucent rectangles with labels/IDs and `x/y/w/h` readouts. Handles appear on corners/edges/center.
- Mouse drag handles move or resize, clamping to 0–1 normalized space. Both single objects and multi-selection bounding boxes snap to grid lines and to neighbouring object edges.
- Multi-selection: click (or `Ctrl`/`Shift`-click) toggles members, and dragging any selected object moves/scales the whole set. Bounding box shows shared handles, and the ImGui panel lists each selected entry with a “Solo” button.
- Keyboard nudges (`arrow keys`) move the selection (`Shift` = bigger steps). `Ctrl+C/V` copy/paste (group-aware, new IDs), `Delete` removes selection, `Ctrl+S` saves.
- ImGui panel exposes numeric editing for IDs, labels, positions, sizes (with clamping). Layout metadata (resolution + form factor) is editable, and buttons exist to add objects, duplicate layouts, or create fresh ones.
- Clipboard contents and snap/grid state persist until the editor deactivates.

## Persistence
- `Ctrl+S` rewrites the matching `(layout_id, width, height)` entry in `data/ui_layouts/layouts.sxp`. The in-memory pool already reflects edits, so gameplay picks them up immediately after a save or hot reload.
- Undo/redo data is per-layout; switching layouts resets the stack.

## Future Ideas
These are optional niceties we’ve discussed but not scheduled:
- Designer-defined guidelines (persistent vertical/horizontal guides that objects can snap to without needing “dummy” objects).
- Alignment helpers (align/distribute buttons for current selection, spacing adjustments, etc.).
- Richer search/filter for the object list, especially on large layouts.
- Integration hooks for a future navigation/menu state editor so selection states can drive menu focus graphs.
