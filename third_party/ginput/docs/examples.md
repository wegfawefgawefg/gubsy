# ginput Examples

This document shows the intended shape of `ginput` data and the expected way a
game consumes it.

The examples use symbolic constants in code, but profile files store integer
ids. Schema metadata gives tools names and categories for those ids.

## Consumption Model

`ginput` owns profiles and bindings. The game owns device polling and action
consumption.

Typical flow:

```cpp
enum Action {
    MenuUp = 0,
    MenuDown = 1,
    MenuSelect = 2,
    MenuBack = 3,
    MoveUp = 4,
    MoveDown = 5,
    MoveLeft = 6,
    MoveRight = 7,
    Use = 8,
};

enum Axis1D {
    Throttle = 0,
};

enum Axis2D {
    Aim = 0,
};

ginput::Schema schema;
schema.add_action(MenuUp, "Menu Up", "Menu");
schema.add_action(MenuDown, "Menu Down", "Menu");
schema.add_action(MenuSelect, "Select", "Menu");
schema.add_action(MenuBack, "Back", "Menu");
schema.add_action(MoveUp, "Move Up", "Gameplay");
schema.add_action(MoveDown, "Move Down", "Gameplay");
schema.add_action(MoveLeft, "Move Left", "Gameplay");
schema.add_action(MoveRight, "Move Right", "Gameplay");
schema.add_action(Use, "Use", "Gameplay");
schema.add_axis_1d(Throttle, "Throttle", "Gameplay");
schema.add_axis_2d(Aim, "Aim", "Gameplay");
```

The host game samples devices and asks the loaded profile what those controls
mean. Profiles keep a direct lookup index, so this path does not need to scan
every bind:

```cpp
ginput::FrameState frame;
frame.resize_actions(schema.actions().size());
frame.resize_axes_1d(schema.axes_1d().size());
frame.resize_axes_2d(schema.axes_2d().size());
frame.begin_frame();

for (ginput::EncodedControl pressed : host_pressed_buttons_this_frame) {
    for (ginput::ActionId action : ginput::actions_for_button(profile, pressed)) {
        frame.set_down(action, true);
    }
}

for (const SampledAxis1D& sampled : host_sampled_axes_1d) {
    for (const ginput::Axis1DBind& bind : ginput::axes_for_1d(profile, sampled.control)) {
        float value = ginput::apply_axis_transform(sampled.value, bind.scale, bind.deadzone);
        frame.merge_axis_1d(bind.axis_1d, value);
    }
}

for (const SampledAxis2D& sampled : host_sampled_axes_2d) {
    for (const ginput::Axis2DBind& bind : ginput::axes_for_2d(profile, sampled.control)) {
        ginput::Vec2 value =
            ginput::apply_stick_transform(sampled.value, bind.scale_x, bind.scale_y, bind.deadzone);
        frame.merge_axis_2d(bind.axis_2d, value);
    }
}
```

Loaded profiles have their lookup index ready immediately. The add/remove
helpers keep it in sync, and bind lists are exposed as read-only views for
debugging, saving, and editor display.

Menu code reads menu action ids. Gameplay code reads gameplay action ids. There
is no active context inside `ginput`.

`FrameState` keeps previous values when `begin_frame` is called, so edge queries
are direct:

```cpp
if (frame.pressed(Use)) {
    activate_item();
}
```

The host game still owns when frames begin and whether input is latched per
render frame or fixed tick.

## Gubsy-Style Menu And Gameplay

This mirrors Gubsy's current model: the same physical key can bind to menu and
gameplay actions.

```lisp
(input_profiles
  (profile
    (id 100)
    (name "Keyboard")
    (button_binds
      (bind (device_button 82) (action 0)) ; Up arrow -> MenuUp
      (bind (device_button 81) (action 1)) ; Down arrow -> MenuDown
      (bind (device_button 40) (action 2)) ; Enter -> MenuSelect
      (bind (device_button 41) (action 3)) ; Escape -> MenuBack
      (bind (device_button 26) (action 0)) ; W -> MenuUp
      (bind (device_button 22) (action 1)) ; S -> MenuDown
      (bind (device_button 26) (action 4)) ; W -> MoveUp
      (bind (device_button 22) (action 5)) ; S -> MoveDown
      (bind (device_button 4) (action 6))  ; A -> MoveLeft
      (bind (device_button 7) (action 7))  ; D -> MoveRight
      (bind (device_button 8) (action 8))  ; E -> Use
      (bind (device_button 44) (action 8))) ; Space -> Use
    (analog_1d_binds)
    (analog_2d_binds)))
```

Menu consumption:

```cpp
MenuInput input{};

if (frame.is_down(MenuUp)) {
    input.up = true;
}
if (frame.is_down(MenuDown)) {
    input.down = true;
}
if (frame.was_pressed(MenuSelect)) {
    input.select = true;
}
if (frame.was_pressed(MenuBack)) {
    input.back = true;
}
```

Gameplay consumption:

```cpp
Vec2 direction{};

if (frame.is_down(MoveUp)) {
    direction.y -= 1.0f;
}
if (frame.is_down(MoveDown)) {
    direction.y += 1.0f;
}
if (frame.is_down(MoveLeft)) {
    direction.x -= 1.0f;
}
if (frame.is_down(MoveRight)) {
    direction.x += 1.0f;
}
```

## Shooter Profiles

Shooter schema:

```cpp
enum ShooterAction {
    Fire = 0,
    Reload = 1,
    Jump = 2,
    Crouch = 3,
    Pause = 4,
};

enum ShooterAxis2D {
    Move = 0,
    Look = 1,
};
```

Keyboard and mouse:

```lisp
(profile
  (id 200)
  (name "Mouse Keyboard")
  (button_binds
    (bind (device_button 3000) (action 0)) ; Mouse left -> Fire
    (bind (device_button 21) (action 1))   ; R -> Reload
    (bind (device_button 44) (action 2))   ; Space -> Jump
    (bind (device_button 6) (action 3))    ; C -> Crouch
    (bind (device_button 41) (action 4)))  ; Escape -> Pause
  (analog_1d_binds)
  (analog_2d_binds
    (bind (device_stick 4000) (axis_2d 1)))) ; Mouse XY -> Look
```

Gamepad:

```lisp
(profile
  (id 201)
  (name "Gamepad")
  (button_binds
    (bind (device_button 5100) (action 0)) ; Right trigger button -> Fire
    (bind (device_button 5002) (action 1)) ; X -> Reload
    (bind (device_button 5000) (action 2)) ; A -> Jump
    (bind (device_button 5001) (action 3)) ; B -> Crouch
    (bind (device_button 5010) (action 4))) ; Start -> Pause
  (analog_1d_binds)
  (analog_2d_binds
    (bind (device_stick 5200) (axis_2d 0) (deadzone 0.15))
    (bind (device_stick 5201) (axis_2d 1) (scale_y -1.0) (deadzone 0.12))))
```

Left-handed mouse:

```lisp
(profile
  (id 202)
  (name "Left Handed Mouse")
  (button_binds
    (bind (device_button 3001) (action 0)) ; Mouse right -> Fire
    (bind (device_button 21) (action 1))
    (bind (device_button 44) (action 2))
    (bind (device_button 6) (action 3))
    (bind (device_button 41) (action 4)))
  (analog_1d_binds)
  (analog_2d_binds
    (bind (device_stick 4000) (axis_2d 1))))
```

## Flight Sim Profiles

Flight schema:

```cpp
enum FlightAction {
    FirePrimary = 0,
    LandingGear = 1,
    Flaps = 2,
    Pause = 3,
};

enum FlightAxis1D {
    Roll = 0,
    Pitch = 1,
    Yaw = 2,
    Throttle = 3,
};
```

Gamepad split-stick:

```lisp
(profile
  (id 300)
  (name "Gamepad Flight")
  (button_binds
    (bind (device_button 5100) (action 0))
    (bind (device_button 5003) (action 1))
    (bind (device_button 5002) (action 2))
    (bind (device_button 5010) (action 3)))
  (analog_1d_binds
    (bind (device_axis 5200) (axis_1d 0) (deadzone 0.10)) ; Left stick X -> Roll
    (bind (device_axis 5201) (axis_1d 1) (scale -1.0) (deadzone 0.10)) ; Left stick Y -> Pitch
    (bind (device_axis 5202) (axis_1d 2) (deadzone 0.08)) ; Right stick X -> Yaw
    (bind (device_axis 5300) (axis_1d 3))) ; Trigger -> Throttle
  (analog_2d_binds))
```

Joystick and throttle:

```lisp
(profile
  (id 301)
  (name "Joystick Throttle")
  (button_binds
    (bind (device_button 7000) (action 0))
    (bind (device_button 7001) (action 1))
    (bind (device_button 7002) (action 2))
    (bind (device_button 7003) (action 3)))
  (analog_1d_binds
    (bind (device_axis 7100) (axis_1d 0) (deadzone 0.04))
    (bind (device_axis 7101) (axis_1d 1) (scale -1.0) (deadzone 0.04))
    (bind (device_axis 7102) (axis_1d 2) (deadzone 0.04))
    (bind (device_axis 7200) (axis_1d 3)))
  (analog_2d_binds))
```

Keyboard fallback:

```lisp
(profile
  (id 302)
  (name "Keyboard Flight")
  (button_binds
    (bind (device_button 44) (action 0))
    (bind (device_button 10) (action 1))
    (bind (device_button 9) (action 2))
    (bind (device_button 41) (action 3)))
  (analog_1d_binds)
  (analog_2d_binds))
```

Keyboard flight would usually use game-owned button actions for pitch/roll
unless a real consumer proves button-to-axis binds are needed.

## Platformer Or Zelda-Style Profiles

Adventure schema:

```cpp
enum AdventureAction {
    MoveUp = 0,
    MoveDown = 1,
    MoveLeft = 2,
    MoveRight = 3,
    Attack = 4,
    Use = 5,
    Dash = 6,
    Inventory = 7,
    Pause = 8,
};

enum AdventureAxis2D {
    MoveStick = 0,
};
```

Keyboard:

```lisp
(profile
  (id 400)
  (name "Keyboard Adventure")
  (button_binds
    (bind (device_button 26) (action 0))
    (bind (device_button 22) (action 1))
    (bind (device_button 4) (action 2))
    (bind (device_button 7) (action 3))
    (bind (device_button 13) (action 4))
    (bind (device_button 8) (action 5))
    (bind (device_button 15) (action 6))
    (bind (device_button 12) (action 7))
    (bind (device_button 41) (action 8)))
  (analog_1d_binds)
  (analog_2d_binds))
```

Gamepad:

```lisp
(profile
  (id 401)
  (name "Gamepad Adventure")
  (button_binds
    (bind (device_button 5002) (action 4))
    (bind (device_button 5000) (action 5))
    (bind (device_button 5001) (action 6))
    (bind (device_button 5003) (action 7))
    (bind (device_button 5010) (action 8)))
  (analog_1d_binds)
  (analog_2d_binds
    (bind (device_stick 5200) (axis_2d 0) (deadzone 0.18))))
```

Arrow-key menu-friendly keyboard:

```lisp
(profile
  (id 402)
  (name "Arrow Keys Adventure")
  (button_binds
    (bind (device_button 82) (action 0))
    (bind (device_button 81) (action 1))
    (bind (device_button 80) (action 2))
    (bind (device_button 79) (action 3))
    (bind (device_button 44) (action 4))
    (bind (device_button 40) (action 5))
    (bind (device_button 225) (action 6))
    (bind (device_button 12) (action 7))
    (bind (device_button 41) (action 8)))
  (analog_1d_binds)
  (analog_2d_binds))
```

Consumption can combine digital movement and analog movement explicitly:

```cpp
Vec2 move = frame.axis_2d[MoveStick];

if (frame.is_down(MoveUp)) {
    move.y -= 1.0f;
}
if (frame.is_down(MoveDown)) {
    move.y += 1.0f;
}
if (frame.is_down(MoveLeft)) {
    move.x -= 1.0f;
}
if (frame.is_down(MoveRight)) {
    move.x += 1.0f;
}

move = normalize_if_needed(move);
```

That is intentionally game code. `ginput` stores the profile; it does not decide
how movement works.
