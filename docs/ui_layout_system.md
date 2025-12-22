# UI Layout System

This layer centralizes how HUD elements are authored, saved, and consumed at runtime. Layouts are stored on disk per resolution so designers can hand-tune placement while rendering code simply asks for normalized rectangles.

## Terminology
- **UIObject** – A normalized element (`x`, `y`, `w`, `h` are `0.0-1.0`) identified by an `id` plus a descriptive string label.
- **UILayout** – A single resolution-specific arrangement of UIObjects. Multiple layouts can share the same `id`/`label` to represent variants of the same screen (e.g. PlayScreen at 720p vs 1080p).
- **Layout Pool** – `es->ui_layouts_pool`: all layouts loaded from disk. Query helpers search this pool at runtime.

## File Format
Layouts live in `data/ui_layouts/layouts.lisp` as an S-expression tree:

```
(ui_layouts
  (layout
    (id 12345678)
    (label "PlayScreen")
    (resolution (width 1920) (height 1080))
    (objects
      (object (id 1) (label "healthbar") (x 0.02) (y 0.02) (w 0.15) (h 0.03))
      ...more objects...
    )
  )
  ...more layouts...
)
```

`save_ui_layout()` rewrites this file (creating the directory if necessary). Layout definitions should therefore be deterministic/idempotent – updating an existing `(layout id+resolution)` rewrites it in place rather than duplicating it.

## Core API
Declared in `engine/ui_layouts.hpp`:

```c++
struct UIObject { int id; std::string label; float x, y, w, h; };
struct UILayout {
    int id;
    std::string label;
    int resolution_width;
    int resolution_height;
    std::vector<UIObject> objects;

    void add_object(int obj_id, const std::string& label, float x, float y, float w, float h);
    bool remove_object(int obj_id);
    bool remove_object(const std::string& label);
};
```

- `create_ui_layout(id, label, width, height)` seeds a struct ready for authoring.
- `UILayout::add_object` is **idempotent**: reusing an `obj_id` overwrites the stored rect, which keeps scripting tools simple.
- `save_ui_layout(layout)` persists the layout, replacing an existing variant if the `(id, width, height)` tuple matches.
- `load_ui_layouts_pool()` populates `es->ui_layouts_pool`; `reload_ui_layouts_pool()` hot-reloads; `get_ui_layouts_pool()` exposes the vector for tooling.
- `get_ui_layout_for_resolution(layout_id, width, height)` picks the closest layout by matching `id`, weighting aspect-ratio distance heavily before Euclidean resolution distance.
- `get_ui_object(layout, id/label)` returns a pointer to the stored rect for rendering code.
- `generate_ui_layout_id()` / `generate_ui_object_id()` are helpers for runtime/builder tools that need unique identifiers but developers can also hardcode constants.

## Recommended Workflow
1. **Define IDs** – Add layout/object IDs in a shared header (see `game/ui_layout_ids.hpp`) so rendering and setup code agree on identifiers.
2. **Author Layouts** – During startup or an editor step, create layouts via `create_ui_layout`, add/remove objects, and call `save_ui_layout`.
3. **Load Pool** – After any saves, call `load_ui_layouts_pool()` so the runtime copy in `es->ui_layouts_pool` reflects the latest disk state.
4. **Render** – In the draw code, query `get_ui_layout_for_resolution(UILayoutID::PLAY_SCREEN, width, height)` and then fetch objects (by ID or label) to translate normalized coords into pixel rectangles.

### Resolution Matching
When multiple variants exist for a layout ID, `get_ui_layout_for_resolution`:
1. Filters down to layouts whose `id` matches.
2. Computes `aspect_distance = abs(target_aspect - layout_aspect)`.
3. Computes Euclidean resolution distance `sqrt(dx^2 + dy^2)`.
4. Combines them as `aspect_distance * 1000 + resolution_distance` to heavily favor aspect ratio before raw pixel size.

### Splitscreen
Because callers pass the viewport size they care about, splitscreen just queries per-viewport:

```c++
const UILayout* p1 = get_ui_layout_for_resolution(UILayoutID::PLAY_SCREEN, width / 2, height);
const UILayout* p2 = get_ui_layout_for_resolution(UILayoutID::PLAY_SCREEN, width / 2, height);
```

Each player can then fetch objects within their layout and render relative to their viewport dimensions.

## Example Usage
- See `src/game/main.cpp` for how we define and save the Play screen layouts for 1080p, 720p, and 2560x1080, using shared ID constants.
- See `src/game/playing.cpp` for how the Play mode now loads `UILayoutID::PLAY_SCREEN`, pulls out the `UIObjectID::BAR_HEIGHT_INDICATOR`, and renders that UI element using normalized rectangles.

This doc should stay in sync with the APIs in `engine/ui_layouts.*`. Update both whenever new capabilities (e.g., alignment hints, nested containers) are added so designers know how to author future HUDs.
