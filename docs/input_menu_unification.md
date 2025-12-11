# Menu/Input Unification Plan

## Goals
- Let gameplay and menu share a single binding system so remapping applies everywhere.
- Support keyboard + controller devices, with multiple bindings per action.
- Keep mods insulated from device specifics by exposing only high-level actions.

## Proposed Steps

### 1. Define Action Sets
- Extend the existing `BindAction` enum (or introduce a parallel menu enum) with:
  - `menu_left/right/up/down`
  - `menu_confirm`, `menu_back`
  - `menu_page_prev`, `menu_page_next`
- Tag actions by logical group (`gameplay`, `menu`) so UI can show them separately.

### 2. Enhance Binding Data Model
- Allow two slots per action per device (keyboard primary/secondary, controller primary/secondary).
- Represent a binding as `{device_type, code}` where `code` is an SDL scancode or controller button/axis identifier.
- Update profile serialization/deserialization (`*.ini`) to persist these fields. Inject defaults for missing entries when loading older profiles.

### 3. Unified Input Dispatcher
- In `input.cpp`, gather SDL events once, normalize to a list of `InputEvent`s.
- Run events through the binding maps to produce `ActionState` structs for gameplay and menu (pressed/held/edge flags).
- Expose `ActionState menu_actions` to `step_menu_logic()` instead of the current `MenuInputs` object; gameplay code already consumes an action-layer interface.

### 4. Menu Refactor
- Replace direct scancode checks inside menu logic with reads from `menu_actions` (e.g., `menu_actions.left_edge`).
- Ensure menu paging uses the new `menu_page_prev/next` bindings so controller bumpers or remapped keys Just Work™.

### 5. Capture Flow Updates
- When entering “press a key to bind”, listen for both keyboard and controller input. Whichever device triggers first sets either the primary or secondary slot (user choice via UI toggle or implicit first/second binding selection).
- Update the bind rows to display two slots per action, with UI affordances for clearing individual slots.

### 6. Controller Support
- Define default controller mappings (e.g., D-pad/left stick for movement, A/B for confirm/back, bumpers for paging).
- Handle analog axes with configurable deadzones so binding an axis direction feels natural.

### 7. Migration & UX
- Provide menu entries for editing the new menu actions, possibly in a dedicated “Menu Controls” page.
- Add “Reset gameplay controls” and “Reset menu controls” buttons that restore the default two-slot mappings.
- Show warnings if the same physical input is assigned to conflicting actions (optional but nice-to-have).

### 8. Testing Checklist
- Keyboard-only remap: verify menus follow AZERTY-style bindings.
- Controller-only navigation: play through menu and game without touching keyboard.
- Mixed bindings: ensure both primaries and secondaries fire (e.g., WASD + arrow keys simultaneously).
- Profile save/load: confirm new fields persist, older profiles upgrade automatically.

## Open Questions
- Do we allow binding the same physical input to multiple actions without warning?
- Should menu and gameplay share certain actions (e.g., confirm/back) so a single binding applies everywhere, or keep them separate for fine-grained control?
- How do we present controller axis bindings in the UI (e.g., “Left Stick Up”)?

This plan keeps the binding logic unified, improves controller support, and keeps mods agnostic by continuing to emit only logical action events.
