# ginput Spec

`ginput` is a small input binding/profile library. It is not a full input
runtime.

The first source of truth is Gubsy's current bind system:

- `engine/binds_profiles.*`
- `engine/input_binding_utils.*`
- `engine/input_sources.*`
- `engine/input_system.hpp`
- `engine/input.hpp`

The strongest extraction target is the profile/schema/reconciliation model from
`binds_profiles.*`. The backend sampling code in `input_binding_utils.*` is
useful, but it is currently coupled to SDL2, `EngineState`, ImGui capture, the
layout editor, GLM, and Gubsy's graphics projection rules. That code should
inform optional adapters, not the core.

## Name

Use `ginput`.

Do not call it `ginputm`. Mapping is the main feature, but the useful library is
broader than mapping:

- schemas
- profiles
- binding records
- reconciliation
- conflict checks
- serialization
- optional backend adapters

## Core Responsibilities

The core library owns:

- `ActionId`, `Axis1DId`, and `Axis2DId` integer ids.
- Schema entries for actions and axes.
- Labels/categories/order metadata for debug UI or menus.
- `InputProfile` records with id, name, and binding lists.
- Button bindings: device button id to action id.
- 1D analog bindings: device axis id to 1D axis id.
- 2D analog bindings: device stick id to 2D axis id.
- Duplicate bind prevention helpers.
- Reconciliation against a schema.
- Duplicate checks.
- Load/save for profiles.

The core should be deterministic, inspectable, and easy to debug. It should use
plain structs and vectors unless a measured need appears.

## Runtime Responsibilities

The optional backend-neutral runtime layer may own:

- `FrameState` for current/previous action and axis snapshots.
- Button edge helpers.
- Menu repeat/debounce helpers.
- Mouse wheel accumulation.
- Axis merge helpers.

Runtime helpers must be explicit-state. They should not use globals, poll
devices, or know about SDL/raylib.

## Non-Responsibilities

The core library must not own:

- SDL, raylib, GLFW, platform APIs, or device polling.
- ImGui capture checks.
- Layout editor state.
- Active input contexts or layers.
- Player profiles or user profiles.
- Save files.
- Game simulation input consumption.
- Deciding which action ids are relevant in a game mode.
- Menu screens or binding editor UI.
- Rendering overlays.
- Mouse projection into a render target.
- Hotplug policy.
- Cloud sync, Steam Input, or platform account policy.

Those concerns belong in adapters, examples, or the host game.

## Runtime Helpers

`ginput::runtime` provides small backend-neutral helpers for common input-frame
bookkeeping:

```cpp
ginput::FrameState frame;
frame.resize_actions(schema.actions().size());
frame.begin_frame();
frame.set_down(Jump, true);

if (frame.pressed(Jump)) {
    jump();
}
```

The host still owns frame timing, fixed-tick timing, device polling, backend
events, and text input.

## Relationship To gconfig

`gconfig` should store which input profile a user prefers:

```lisp
(user_profiles
  (profile
    (id 42)
    (name "GMan")
    (values
      (key "preferred_input_profile" 1))))
```

`ginput` should store the input profile itself:

```lisp
(input_profiles
  (profile
    (id 1)
    (name "Keyboard")
    (button_binds
      (bind (device_button 65) (action 0))
      (bind (device_button 66) (action 1)))
    (analog_1d_binds
      (bind (device_axis 0) (axis_1d 0)))
    (analog_2d_binds
      (bind (device_stick 0) (axis_2d 0)))))
```

`gconfig` owns user preference documents. `ginput` owns input profile documents.
Neither library should own the other concept.

## IDs Over Strings

Core bindings should use ints, matching Gubsy's current model.

Current Gubsy data is shaped like:

```lisp
(bind (device_button 65) (gubsy_action 0))
(bind (device_axis 0) (gubsy_analog 0))
(bind (device_stick 0) (gubsy_stick 0))
```

`ginput` should keep that basic idea:

```text
device control id -> game action id
```

Reasons:

- Simple runtime lookup.
- Matches Gubsy's existing disk data.
- Game code can use enums and cast to ints.
- Reconciliation against a schema is straightforward.
- Display names can live in schema metadata.

Names and labels are useful for UI, docs, and diagnostics, but ids should be the
authoritative binding values.

## Device Control IDs

The core should not expose backend key enums directly. It should represent a
control as a stable integer plus kind metadata.

Planned model:

```cpp
enum class DeviceKind {
    Keyboard,
    Mouse,
    Gamepad,
};

struct DeviceButton {
    DeviceKind kind;
    int device_id; // specific device or any-device sentinel
    int code;
};

struct DeviceAxis1D {
    DeviceKind kind;
    int device_id;
    int code;
};

struct DeviceAxis2D {
    DeviceKind kind;
    int device_id;
    int x_code;
    int y_code;
};
```

The core may provide encode/decode helpers to store these as ints for compact
files and fast comparisons. Backend adapters translate SDL3, raylib, or other
library codes into these values.

Stick components should be usable as 1D axes. A gamepad left stick can expose a
whole 2D control for `DeviceAxis2D`, and also expose its X and Y components as
separate `DeviceAxis1D` controls. That allows profiles such as:

```lisp
(analog_1d_binds
  (bind (device_axis 1000) (axis_1d 0)) ; left stick X -> rotate
  (bind (device_axis 1001) (axis_1d 1))) ; left stick Y -> shoot power
```

The backend adapter owns the control ids. Core only needs stable ids and clear
labels for tools.

## Schema

Schema data should be small and explicit:

```cpp
ginput::Schema schema;
schema.add_action(0, "Jump", "Gameplay");
schema.add_action(1, "Shoot", "Gameplay");
schema.add_axis_1d(0, "Move X", "Gameplay");
schema.add_axis_2d(0, "Aim", "Gameplay");
```

Schema entries should include:

- id
- display name
- category
- order
- optional flags later if needed

The schema validates profile bindings. If an action or axis id no longer exists,
the binding is removed or reported depending on policy.

## Profiles

An input profile is reusable and may be shared across user profiles.

```cpp
struct InputProfile {
    int id = -1;
    std::string name;

    const std::vector<ButtonBind>& button_binds() const;
    const std::vector<Axis1DBind>& axis_1d_binds() const;
    const std::vector<Axis2DBind>& axis_2d_binds() const;
};
```

The profile should not know which user owns it. A host game can make personal
profiles by convention, but that is not core library policy.

Profiles keep direct lookup indexes in both useful directions:

- control id to action/axis binds, for runtime event application
- action/axis id to bind records, for binding editors and settings screens

Callers mutate binds through helpers such as `add_button_bind` and
`remove_button_bind`; those helpers keep the indexes current. The bind vectors
are read-only from the public API.

Analog binds should allow small per-bind transforms with safe defaults:

```cpp
struct Axis1DBind {
    int device_axis = 0;
    int axis_1d = 0;
    float scale = 1.0f;
    float deadzone = 0.0f;
};

struct Axis2DBind {
    int device_stick = 0;
    int axis_2d = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float deadzone = 0.0f;
};
```

This covers normal profile-level needs like inverted Y, swapped-feeling
hardware, trigger sensitivity, and stick drift. It does not make `ginput` a
gameplay input runtime. Callers still decide when and how to consume the sampled
values.

## Actions, Modes, And Menus

`ginput` should not have contexts or layers in core.

Gubsy's current model is enough:

- Bindings are flat `control -> action id` records.
- Menu actions and gameplay actions are separate action ids.
- The same physical control may bind to several different actions.
- The host game decides which action ids to consume in each mode.

For example, Gubsy binds `W` to both `MENU_UP` and gameplay `UP`. The menu input
path scans binds and only reacts to `MENU_*` actions. Gameplay movement asks for
`UP`, `DOWN`, `LEFT`, and `RIGHT`. The in-game menu blocks gameplay by game
logic, not by changing an active input context in the binding system.

This keeps the binding library simple and leaves policy in the caller. If a game
later wants contexts, it can encode that in its action ids, categories, or a
small wrapper above `ginput`.

## Reconciliation

Reconciliation should handle:

- bindings that target missing action ids
- bindings that target missing axis ids
- duplicate exact bindings
- malformed or unknown forms while loading

Reports should be explicit enough for tools:

```cpp
struct ReconcileChange {
    ReconcileChangeKind kind;
    int profile_id;
    int source_id;
    int target_id;
    std::string message;
};
```

## Duplicate And Conflict Checks

The first conflict checks should be boring:

- exact duplicate button bind
- exact duplicate 1D axis bind
- exact duplicate 2D axis bind

These are exact duplicate records, not same-source collisions. A button bound to
multiple different actions is valid. A 1D axis bound to multiple different
targets is valid. A 2D stick bound to multiple different targets is valid.

Tools may still report same-source multi-binds as informational diagnostics if a
caller wants to show them, but core helpers should not treat them as errors.

Do not over-design chords, contexts, layers, or Steam Input-style action sets
until a real consumer needs them.

## Serialization

Use `gsexp`, not a new parser.

Initial format should stay close to Gubsy:

```lisp
(input_profiles
  (profile
    (id 1)
    (name "Default")
    (button_binds
      (bind (device_button 65) (action 0)))
    (analog_1d_binds
      (bind (device_axis 0) (axis_1d 0) (scale 1.0) (deadzone 0.05)))
    (analog_2d_binds
      (bind (device_stick 0) (axis_2d 0) (scale_x 1.0) (scale_y -1.0) (deadzone 0.1)))))
```

Differences from Gubsy:

- Use neutral names like `action`, `axis_1d`, and `axis_2d` instead of
  `gubsy_action`, `gubsy_analog`, and `gubsy_stick`.
- Keep integer ids.
- Omit transform fields when they equal defaults if compact files are preferred.
- Preserve enough structure to support old Gubsy import later.

## Optional Backend Adapters

Adapters should be separate targets:

- `ginput::sdl3`
- `ginput::raylib` if needed

An adapter can own:

- backend key/button enum translation
- device enumeration
- backend control labels

An adapter should not own:

- user profiles
- binding editor UI
- device polling
- ImGui capture policy
- game action dispatch
- app event loop

## First Implementation Plan

1. Scaffold C++20/CMake like `gconfig`, `gsexp`, and `glayout`.
2. Implement core structs and schema.
3. Implement add/remove binding helpers.
4. Implement reconciliation and conflict checks.
5. Implement sexpr load/save using `gsexp`.
6. Add tests for profile round trips, reconcile, and conflict checks.
7. Add examples based on Gubsy, shooter, flight sim, and adventure profiles.
8. Only then consider an SDL3 adapter.

See [examples.md](examples.md) for the intended profile shapes and consumption
style.

## Open Questions

- Should the core encode `DeviceButton` into one int, or store structured
  controls on disk from the start?
- Should v1 support button-to-axis binds, or should games model keyboard
  movement as button actions like Gubsy does?
- Should adapter targets live in this repo or separate repos?

Current recommendation:

- Encode controls as ints internally and on disk for v1.
- Allow same-source multi-binds; reject only exact duplicate records by default.
- Do not add contexts/layers to core.
- Do not add button-to-axis binds until a real consumer needs them.
- Keep adapters in the repo as optional targets if they stay small.
