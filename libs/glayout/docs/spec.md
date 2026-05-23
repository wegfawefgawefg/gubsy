# glayout Specification

## Purpose

`glayout` is a lightweight C++ layout authoring tool for games and custom UI.
It stores rectangular UI objects in normalized coordinates, picks the closest
layout variant for a runtime resolution, and can optionally provide an in-game
editor for dragging, resizing, inspecting, and saving those rectangles.

The library should stay boring:

- Plain structs over class hierarchies.
- Explicit function calls over callback-heavy frameworks.
- No required renderer, windowing, engine, or ImGui dependency in the core.
- The renderer-free editor should stay independent of SDL, ImGui, and engines.
- Optional ImGui helpers should sit beside the core, not inside it.
- File format should remain small, readable, and hand-editable.

## Responsibilities

`glayout` is responsible for:

- Authoring flat, named/id'd rectangles in normalized space.
- Storing multiple layout variants for the same screen/page id.
- Finding the best layout variant for a requested resolution and form factor.
- Parsing and writing a small layout file format.
- Editing rectangles at runtime when the optional editor module is used.
- Providing optional Dear ImGui editor/debug panels when the host already uses
  Dear ImGui.
- Providing small rectangle coordinate helpers that make composition easy.

The core model is:

```text
Layout = one flat set of normalized object rectangles for one page/screen
Object = id + label + normalized rectangle
Variant = same layout id, different resolution/form factor
```

## Non-Responsibilities

`glayout` is not responsible for:

- Full UI framework behavior.
- Widget library behavior.
- Flexbox, grid, constraint, or web-style automatic layout.
- Retained-mode rendering.
- Generic plugin architecture.
- Dependency management for Dear ImGui, SDL, GLFW, or any engine.
- Menu/navigation behavior.
- Scroll-view behavior.
- Clipping behavior.
- Input routing.
- Styling or theming.
- Game behavior or command binding.
- Widget metadata such as button, slider, image, or panel.
- Game-facing navigation links such as up/down/left/right.
- Ownership of the app loop, renderer, window, asset paths, or save policy.

Host applications decide what a rectangle means. A `glayout` object can become a
button, text label, sprite slot, scroll viewport, child layout region, or
anything else, but that interpretation lives outside this library.

## Composition and Scrolling

`glayout` should support composition through plain rectangle math, not through a
stored hierarchy.

The useful primitive is mapping one normalized rectangle into another rectangle:

```cpp
struct Rect {
    float x;
    float y;
    float w;
    float h;
};

Rect map_rect(Rect parent, Rect child_normalized);
```

Expected behavior:

```cpp
Rect map_rect(Rect parent, Rect child) {
    return {
        parent.x + child.x * parent.w,
        parent.y + child.y * parent.h,
        child.w * parent.w,
        child.h * parent.h,
    };
}
```

This allows a caller to embed one layout inside a rectangle from another layout
without `glayout` storing parent/child relationships.

Scrolling can also be built by the host with the same primitives:

- Use one authored rectangle as the viewport.
- Map child rectangles into that viewport or into a shifted virtual content
  rectangle.
- Let the host own scroll offset, clipping, wheel/controller input, and
  visibility rules.

`glayout` may provide simple helpers like `intersects` and `intersection`, but
it should not store scroll offsets or implement scroll containers.

## Coordinate Semantics

Rectangles are normalized by convention.

Expected normal authored range:

```text
x >= 0
y >= 0
w >= 0
h >= 0
x + w <= 1
y + h <= 1
```

Core should preserve rectangle values as given:

- Parsing should not clamp.
- Writing should not clamp.
- `map_rect` should not clamp.
- Layout lookup should not validate or reject layouts based on rectangle bounds.

Out-of-bounds rectangles can be useful for oversized backgrounds, decorative
bleed, offscreen animation positions, scroll/content layouts, and deliberate
negative margins.

The editor should default to keeping directly manipulated rectangles inside the
unit rectangle. Later editor options may allow out-of-bounds editing, but the
first version should favor safe direct manipulation.

Validation helpers may report suspicious rectangles, but validation should be
explicit and nonfatal.

## Modules

### `glayout::core`

Required, dependency-light module.

Responsibilities:

- Define layout data types.
- Parse layout files.
- Write layout files.
- Find layout variants by id, resolution, aspect ratio, and form factor.
- Find objects by id or label.
- Generate simple ids when callers want runtime-created layouts/objects.

Allowed dependencies:

- C++ standard library.
- `gsexp` for S-expression parsing/writing.

Not allowed:

- Engine state.
- SDL types.
- ImGui types.
- Filesystem policy beyond helper functions that accept explicit paths.
- Global current form factor hidden inside lookup.

Expected core API shape:

```cpp
namespace glayout {

enum class FormFactor {
    Desktop,
    Tablet,
    Phone,
};

struct Object {
    int id;
    std::string label;
    Rect rect;
};

struct Layout {
    int id;
    std::string label;
    int width;
    int height;
    FormFactor form_factor;
    std::vector<Object> objects;
};

const Layout* find_best_layout(const std::vector<Layout>& layouts,
                               int layout_id,
                               int target_width,
                               int target_height,
                               FormFactor preferred_form_factor);

const Object* find_object(const Layout& layout, int object_id);
const Object* find_object(const Layout& layout, std::string_view label);

} // namespace glayout
```

Identity rules:

- `Object::id` is the stable identity.
- `Object::label` is a human/debug name.
- Duplicate labels are allowed.
- `find_object(layout, label)` returns the first matching object in layout
  order.
- `add_or_replace_object` replaces by id, not by label.

Core should provide string conversion helpers for form factors:

```cpp
std::string_view to_string(FormFactor form_factor);
FormFactor form_factor_from_string(std::string_view text);
```

### Renderer-Free Editor

Runtime editing API included with `glayout::core`.

Responsibilities:

- Track active layout selection.
- Track selected object or object group.
- Hit-test objects and resize handles.
- Drag and resize selected objects.
- Support multi-select.
- Support snapping.
- Support copy, paste, delete.
- Support snapshot undo/redo.
- Report save requests.

The editor should not poll platform input itself. The host should pass a small
input snapshot each frame.

Expected input shape:

```cpp
struct EditorInput {
    float mouse_x;
    float mouse_y;
    bool left_down;
    bool ctrl;
    bool shift;
    bool key_save;
    bool key_undo;
    bool key_redo;
    bool key_delete;
    bool key_copy;
    bool key_paste;
};
```

The editor should not require a renderer. It should expose enough state for the
host or optional ImGui helper to draw overlays. If we keep a concrete renderer
helper later, it should be a separate adapter file.

`EditorState` is intentionally a plain, inspectable struct. Caller-visible
state and configuration should remain near the top of the struct. Bookkeeping
for drag state, undo/redo, and per-frame mouse transitions can remain public for
debugging, but normal callers should treat it as owned by the editor functions.

### `glayout::imgui`

Optional Dear ImGui helper module.

Responsibilities:

- Layout browser/debug window.
- Editor panel for selecting layouts and objects.
- Numeric editing for layout metadata and object rectangles.
- Buttons for add object, duplicate layout, new layout, save, undo, redo.
- Display editor status and dirty state.

Rules:

- Compile only when `GLAYOUT_WITH_IMGUI=ON`.
- Do not commit ImGui sources into this repo.
- Do not make ImGui a dependency of `glayout::core`.
- ImGui helpers should manipulate the same public structs and editor state as
  non-ImGui users.

## File Format

Use a tiny Lisp-style S-expression format compatible with the current layout
files.

Example:

```lisp
(ui_layouts
  (layout
    (id 100)
    (label "TitleScreen")
    (resolution (width 1280) (height 720))
    (form_factor desktop)
    (objects
      (object (id 1) (label "play_button") (x 0.4) (y 0.4) (w 0.2) (h 0.08))
    )
  )
)
```

Parser requirements:

- Support symbols, strings, numbers, and nested lists.
- Ignore whitespace.
- Preserve enough diagnostics to report parse failure location later.
- Be small enough to audit in one sitting.

Parser result shape:

```cpp
struct ParseResult {
    bool ok;
    std::vector<Layout> layouts;
    std::vector<Diagnostic> diagnostics;
};
```

Duplicate layout variants in input are nonfatal. A duplicate variant means the
same `(layout id, width, height, form_factor)` appears more than once. The parser
should preserve input order and report a warning/diagnostic when practical.
Update/save helpers should use the latest duplicate as the effective variant
when replacing a layout.

Writer requirements:

- Deterministic output.
- Replace/update variants by `(layout id, width, height, form_factor)`.
- Quote labels safely.
- Avoid reordering objects unless the caller does it.

String parse/write should be the primary persistence API:

```cpp
ParseResult parse_layouts(std::string_view text);
std::string write_layouts(const std::vector<Layout>& layouts);
```

File helpers can exist for convenience, but they should be thin wrappers around
the string APIs:

```cpp
ParseResult load_layout_file(const std::filesystem::path& path);
bool save_layout_file(const std::filesystem::path& path, const std::vector<Layout>& layouts);
```

## Layout Matching

When several layouts share an id:

1. Prefer the requested form factor if any matching variant exists.
2. Score candidates by aspect-ratio distance first.
3. Use raw resolution distance as the secondary signal.
4. Break exact score ties by layout vector/file order.
5. Return `nullptr` when no layout has the requested id.

If no layout exists for the requested form factor, lookup should fall back to all
form factors. This fallback is nonfatal. Debug/demo tooling may show what
happened, but core lookup should remain safe and quiet unless the caller uses a
diagnostic API.

The default score should be:

```text
score = abs(target_aspect - layout_aspect) * 1000 + euclidean_resolution_distance
```

This keeps behavior predictable while still favoring matching aspect ratios.

## Integration Model

A host game should be able to do this:

```cpp
glayout::ParseResult parsed = glayout::load_layout_file(path);
std::vector<glayout::Layout> layouts = std::move(parsed.layouts);

const glayout::Layout* layout =
    glayout::find_best_layout(layouts, title_layout_id, render_w, render_h, form_factor);

const glayout::Object* play_button = glayout::find_object(*layout, play_button_id);
```

For editor users:

```cpp
glayout::EditorState editor;
glayout::EditorInput input;
glayout::Viewport viewport{0.0f, 0.0f, float(render_w), float(render_h)};
glayout::Layout& active_layout = layouts[active_layout_index];

glayout::editor_begin_frame(editor, active_layout, input, viewport);
glayout::imgui::render_integrated_editor(editor, layouts, selected_layout_index); // optional

if (editor.save_requested) {
    glayout::save_layout_file(path, layouts);
}
```

The exact names can change, but the dependency direction should not.

Saving rules:

- Core should provide explicit save helpers.
- The editor should not secretly write files during frame update.
- UI code may expose a Save button, but the actual save call should still be an
  explicit host/demo action.
- Save requests can be reported through editor state or a frame result, but the
  caller decides when and where data is written.

Dirty rules:

- A layout becomes dirty when the editor mutates it.
- Drag, resize, numeric edits, add, delete, paste, duplicate, and metadata edits
  are mutations.
- Dirty state clears when the caller reports a successful save or reloads data.
- Undo/redo stacks are per layout.

## SDL Demo

The repo should include a real demo app that proves the library is usable in a
normal game loop.

Target example:

```text
examples/sdl_demo
```

Demo requirements:

- Uses SDL3 for the window and simple rectangle rendering.
- Shows three dummy menu pages: `Title`, `Settings`, and `Credits`.
- Has visible dummy controls such as buttons, labels, sliders-as-rectangles,
  and panels, but these remain demo concepts, not `glayout` core concepts.
- Has nine authored layout variants:
  - `Title` desktop/tablet/phone
  - `Settings` desktop/tablet/phone
  - `Credits` desktop/tablet/phone
- Uses concrete authoring resolutions for the variants:
  - Desktop: `1920x1080`
  - Tablet: `1536x2048`
  - Phone: `1080x1920`
- Lets the user switch pages.
- Lets the user resize or pick preview resolutions/form factors and see layout
  matching change.
- Draws focused/selected demo buttons so layout consumption is clear.
- Uses public `glayout` APIs only.
- Saves and reloads a sample layout file from a demo-owned data path.
- When `GLAYOUT_WITH_IMGUI=ON`, shows the optional ImGui layout browser/editor.
- When ImGui is off, still demonstrates core layout lookup and rectangle
  rendering.

CMake options:

```cmake
GLAYOUT_BUILD_EXAMPLES=ON
GLAYOUT_BUILD_SDL_DEMO=ON
GLAYOUT_WITH_IMGUI=ON
```

The SDL demo is a consumer of the library. It should not reach into private
implementation details or become a hidden engine layer.

Dependency rules:

- `glayout::core` depends on `gsexp::gsexp` instead of carrying its own Lisp
  parser.
- Consumers should link `glayout::core` when using CMake `add_subdirectory`.
- Consumers should link `glayout::imgui` only when the optional ImGui helpers
  are enabled.
- Local development may use a sibling `../gsexp` checkout.
- Consumers may provide `gsexp::gsexp` themselves or point CMake at a gsexp
  source tree.
- `GLAYOUT_WITH_IMGUI=ON` expects Dear ImGui to be provided by the caller/build,
  or fetched into the build tree for the SDL demo.
- The CMake hook may be a provided target or a caller-provided
  `GLAYOUT_IMGUI_SOURCE_DIR`. Committing ImGui sources into this repo is out of
  scope.
- `GLAYOUT_BUILD_SDL_DEMO=ON` may use `find_package(SDL3)`.
- If SDL3 is unavailable, the SDL demo should be disabled by default unless the
  user explicitly requested it.

## First Milestone

1. Standalone repo builds with strict warnings.
2. `glayout::core` has the public data structs and layout matching.
3. Tiny Lisp parser/writer can round-trip simple layout files.
4. Tests or examples verify matching and parsing.
5. No SDL, ImGui, or engine dependencies in core.

## Second Milestone

1. Add editor state, selection, hit-testing, drag, resize, snap, copy/paste, and
   undo/redo.
2. Replace `EngineState` coupling with explicit `EditorInput`, `Viewport`, and
   caller-owned layout vectors.
3. Keep behavior direct enough that an existing tool can switch to the library
   without a workflow regression.

## Third Milestone

1. Add optional ImGui panels.
2. Keep ImGui source files isolated behind `GLAYOUT_WITH_IMGUI`.
3. Add debug browser and editor panel features.
4. Add the SDL3 demo app with three pages and nine layout variants.
