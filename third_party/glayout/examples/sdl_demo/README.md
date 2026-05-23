# glayout SDL Demo

This demo is a normal SDL3 consumer of `glayout::core`.
It loads `data/layouts.lisp`, picks the closest layout for the active page and
preview form factor, and draws the resulting rectangles with SDL.

## Controls

- `1`, `2`, `3`: switch to Title, Settings, Credits.
- `Tab`: cycle pages.
- `P`: cycle desktop/tablet/phone preview.
- Arrow keys: move demo focus.
- `Enter`: activate focused demo item.
- `E`: toggle layout edit mode.
- Mouse drag in edit mode: move or resize rectangles.
- `S`: save layouts.
- `Z`, `Y`: undo, redo.
- `C`, `V`: copy, paste selected object.
- `Delete`/`Backspace`: delete selected object.
- `Esc`: quit.

The demo intentionally treats buttons, panels, and labels as demo-only concepts.
`glayout` itself only knows about rectangles.

The default `dev` preset builds the demo with Dear ImGui by fetching ImGui into
the build directory if the caller did not provide an ImGui target/source tree.
Press `E` to enter edit mode; the ImGui layout browser/editor appears while edit
mode is on.

The demo requests SDL's utility-window type so tiling window managers can float
it instead of treating it like a normal application window. On i3, add this if
your config does not already float utility windows:

```i3
for_window [window_type="utility"] floating enable
```

The CMake path is intended to work on Linux, macOS, and Windows when SDL3 is
available through `find_package(SDL3 CONFIG)`. The fallback `pkg-config` path is
mainly for Unix-like local development.

If configured without ImGui or without SDL3 ImGui backends, the SDL rectangle
editor still works.
