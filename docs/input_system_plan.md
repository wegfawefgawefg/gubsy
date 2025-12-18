# Input System Roadmap

This document captures the plan for replacing the current keystate/event queue approach with a deterministic `InputFrame` pipeline that supports fixed-step simulation, future rollback netcode, and cleaner per-game bindings.

## Current Issues

- `process_inputs()` clears `es->input_event_queue` every render frame, but `step()` only runs on fixed-timestep intervals. Most input events are discarded before gameplay can consume them.
- Gameplay reads `was_pressed()` / `was_released()` from the queue, so any frame where the sim does not advance loses its inputs.
- Keyboard/gamepad data is polled ad hoc; there is no canonical snapshot per frame, making deterministic replay/networking impossible.

## Goals

1. Produce one canonical `InputFrame` per player, per simulation frame.
2. Allow games to define the contents/merge logic of that frame via a callback.
3. Keep raw `DeviceState` (keyboard, mouse, controller) tracking generic inside the engine.
4. Retain menu/UI responsiveness without dropping gameplay inputs.
5. Make `InputFrame` trivially serializable so the engine can help with networking/rollback.

## Proposed Architecture

### DeviceState Sampling

- On each render frame, pump SDL events and update a persistent `DeviceState` structure:
  - `keyboard_down[SDL_NUM_SCANCODES]`
  - `mouse_position`, `mouse_delta`, `mouse_buttons`
  - `gamepad_buttons` and normalized axes per open controller
- This data lives entirely inside the engine and is never cleared mid-frame.

### InputFrame Definition

- Each game defines `input_frame.hpp` with a POD struct that captures the canonical state for that frame:

  ```cpp
  struct InputFrame {
      uint32_t down;      // buttons currently held
      int16_t move_x, move_y; // canonical movement axes
      int16_t aim_x, aim_y;   // canonical aim axes
      int16_t mouse_dx, mouse_dy; // accumulated delta since last frame
  };
  ```

- The struct should be fixed-size, trivially copyable, and quantized (no floats) to ease serialization.
- SDL events update `down` immediately; edges are derived inside the sim by comparing `cur` vs. `prev` snapshots. (Short taps entirely between sim frames can be missed, which is the same compromise Terraria makes.)

### Build Callback

- The engine exposes a hook the game registers:

  ```cpp
  using BuildInputFrameFn = void(*)(const DeviceState&, InputFrame&);
  ```

- Each simulation frame, the engine calls this function to build the canonical snapshot for that player from the latest `DeviceState`, applying the game’s bind schema and merge rules (keyboard vs. gamepad vs. mouse).

### History / Consumption

- Maintain `std::vector<InputFrame> input_history_per_player`, indexed by the simulation frame number.
- Before running `Step(sim_frame)`, fetch:
  - `cur = input_history[player][sim_frame]`
  - `prev = input_history[player][sim_frame - 1]` (or zeroed on frame 0)
- Gameplay derives edges (`pressed = cur & ~prev`) deterministically.
- The sim loop samples once per fixed step—if rendering stalls and the accumulator needs multiple catch-up steps, we run the loop `N` times: update `DeviceState`, build the next `InputFrame`, run `Step`, increment `sim_frame`. This is the same “fixed update, variable render” pattern used by games like Terraria.
- Because we accept one snapshot per fixed frame, extremely short taps (entirely between two sim frames) can be missed. That’s the same compromise Terraria makes; in exchange we get deterministic input. Game code should implement repeat/hold behavior off the sampled frames (edge detection + timers) and not rely on OS key repeat.
- Only after `Step` completes do we discard or reuse history entries (or keep longer if we need replay/rollback).

### Event Queue + Menus

- Menu/UI code that needs high-frequency responses can still read from the raw `DeviceState` or from the latest `InputFrame`.
- The legacy `process_inputs_fn` callback becomes optional or can be removed entirely; all gameplay logic reads from the per-frame snapshots.
- If we keep a queue for text/UI, we clear it only after both menu code and the fixed-step sim have consumed the events for that frame.

### Networking / Rollback

- Because `InputFrame` is canonical and fixed-size, the engine can provide helpers to:
  - Serialize/deserialize frames to byte buffers.
  - Maintain ring buffers (`input_history[player][frame]`).
  - Re-simulate from a checkpoint when late inputs arrive (rollback).

## Transition Plan

1. **Documentation (this file).**
2. Add `DeviceState` tracking code and ensure it updates every render frame.
3. Introduce `input_frame.hpp` in the game module and implement the build callback.
4. Replace the current event-queue consumption with the per-frame `InputFrame` history (preserve existing API like `was_pressed` by comparing `cur` vs `prev`).
5. Remove the redundant `process_inputs_fn` callback or re-purpose it to read the canonical frames instead of the raw queue.
6. Extend the engine/netcode layer with serialization helpers for `InputFrame`.

Once complete, button presses will never be dropped between sim steps, the game will have deterministic inputs per frame, and we have a foundation for replay/rollback/networking without rewriting the input layer later.
