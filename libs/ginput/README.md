# ginput

<p>
  <img src="assets/logo.svg" alt="ginput logo" width="96" height="96">
</p>

`ginput` is a small C++20 library for input action schemas, input profiles, and
integer-id input bindings.

It is intended to extract the reusable binding/profile parts of Gubsy's input
system without taking ownership of a game's event loop, player model, renderer,
or UI.

`ginput` saves input profile files: named collections of button, 1D axis, and
2D axis bindings. It does not save user profiles, global settings, or save-game
data. A game can use `gconfig` to store which input profile a user selected.

## Core

- Action schemas for button actions, 1D axes, and 2D axes/sticks.
- Named/id input profiles.
- Integer-id bindings from device controls to game actions.
- Reconciliation against schemas.
- Duplicate checks.
- S-expression load/save through `gsexp`.

## Runtime Helpers

- `FrameState` for current/previous action and axis snapshots.
- `ButtonState` and edge queries: down, pressed, released.
- Repeat/debounce helper for menu navigation.
- Mouse wheel accumulator.
- Axis merge helpers.

These helpers do not poll devices or own an event loop.

## Add To A Project

The intended integration path is vendored source with CMake `add_subdirectory`.
Put `gsexp` and `ginput` somewhere under your project, for example:

```text
third_party/
  gsexp/
  ginput/
```

Then wire them into your CMake project:

```cmake
add_subdirectory(third_party/gsexp)

set(GINPUT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GINPUT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/ginput)

target_link_libraries(my_game PRIVATE ginput::ginput)
```

`ginput::runtime` is an interface target for the backend-neutral runtime
headers. It links `ginput::ginput`, so consumers may use either target:

```cmake
target_link_libraries(my_game PRIVATE ginput::runtime)
```

For sibling development checkouts, `ginput` defaults `GINPUT_GSEXP_SOURCE_DIR`
to `../gsexp`. If your layout is different, set it before adding `ginput`.

## Basic Use

```cpp
enum Action {
    MenuUp = 0,
    MoveUp = 1,
    Use = 2,
};

ginput::Schema schema;
schema.add_action(MenuUp, "Menu Up", "Menu");
schema.add_action(MoveUp, "Move Up", "Gameplay");
schema.add_action(Use, "Use", "Gameplay");

ginput::InputProfile profile;
profile.id = 100;
profile.name = "Keyboard";
ginput::add_button_bind(profile, ginput::ButtonBind{26, MenuUp});
ginput::add_button_bind(profile, ginput::ButtonBind{26, MoveUp});
ginput::add_button_bind(profile, ginput::ButtonBind{8, Use});

std::vector<ginput::InputProfile> profiles{profile};
std::string text;
ginput::save_profiles_string(profiles, text);

ginput::LoadProfilesResult loaded = ginput::load_profiles_string(text, schema);

const std::vector<ginput::ActionId>& actions = ginput::actions_for_button(
    loaded.profiles[0],
    26);

ginput::FrameState frame;
frame.resize_actions(schema.actions().size());
frame.begin_frame();
frame.set_down(MoveUp, true);

if (frame.pressed(MoveUp)) {
    // First frame MoveUp is down.
}
```

The host game still owns device polling and action consumption. `ginput` stores
the profile, validates it against the schema, keeps direct lookup tables on
loaded profiles, and offers small backend-neutral runtime helpers.

## Optional Adapters

- SDL3 adapter for translating SDL keyboard, mouse, and gamepad inputs.
- Raylib adapter if a real consumer needs it.

Adapters should translate backend input into `ginput` device/control ids. The
core/runtime should not include SDL, raylib, ImGui, GLM, or engine state.

See [docs/spec.md](docs/spec.md) for scope and design boundaries.
See [docs/examples.md](docs/examples.md) for profile and consumption examples.

## Build

```sh
./scripts/build.sh
```

The build script configures the default development preset and runs tests
through CTest.

## Run Demo

```sh
./scripts/run.sh
```
