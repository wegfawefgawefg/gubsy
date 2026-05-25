# In-Game Pause Menu Plan

This plan covers adding a reusable Gubsy in-game menu overlay and wiring it into
`splonks-cpp`.

The target is a real shared menu path, not a second Splonks-only pause menu.
Gubsy should own the menu screens, navigation, layout, and reusable settings
screens. Splonks should own gameplay state, pause policy, restart behavior, and
network/session-specific consequences.

## Goals

1. Pressing `Start` on a controller or `Escape` on keyboard opens an in-game
   menu while Splonks is playing.
2. Offline/local play pauses simulation while the in-game menu is open.
3. Multiplayer play opens the same menu without pausing simulation.
4. The menu consumes input while open so gameplay does not also receive jump,
   bombs, ropes, movement, or text-edit keys.
5. The top-level menu uses the normal Gubsy settings layout shape: four main
   slots and two bottom actions where needed.
6. Existing Gubsy settings, binds, profiles, input-device, and local-player
   screens are reused where they make sense.
7. Splonks supplies game-specific commands such as restart run and quit to main
   menu through callbacks.

## Non-Goals

1. Do not build a separate Splonks pause menu framework.
2. Do not duplicate Gubsy settings screens under Splonks.
3. Do not make Gubsy own Splonks world simulation, stage loading, renderer
   policy, or netcode.
4. Do not allow game-state mutation from menu callbacks in the middle of Gubsy
   widget traversal. Use the existing command boundary.
5. Do not solve every possible in-game HUD or inventory UI case in this slice.
   This is the pause/system overlay path.

## Top-Level Menu Shape

The first pause screen should be boring and reusable:

1. `Resume`
2. `Settings`
3. `Restart Run`
4. `Quit to Main Menu`

Bottom actions are optional:

1. `Back` should behave like `Resume` on the top-level pause screen.
2. Online client sessions may expose `Leave Room`.
3. Online host sessions may expose `End Room` or `Leave Room`, depending on the
   final session policy.

Initial implementation should keep the four main slots above. Online-specific
actions can be added after the base overlay and settings reuse work correctly.

## Resolved Behavior

1. `Start` and `Escape` open the in-game menu while Splonks is in gameplay.
2. `Back`/`Escape` on the top-level in-game menu resumes gameplay.
3. `Back`/`Escape` inside a submenu returns to the previous menu screen first.
4. Offline/local gameplay pauses while the in-game menu is open.
5. Multiplayer gameplay does not pause while the in-game menu is open.
6. Menu input is exclusive while the in-game menu is open. Gameplay should not
   receive the same button, key, or text-edit input.
7. Gubsy owns menu screens, navigation, layout, settings reuse, and rendering
   the menu overlay.
8. Splonks owns simulation pause policy, restart-run behavior, quit-to-title
   behavior, session leave/disconnect behavior, and game-specific lobby values.
9. Splonks should call into a small `gubsy_shell` surface instead of poking
   private Gubsy runtime/menu state.

## Settings Reuse

The pause menu should push existing Gubsy settings screens with an explicit
in-game context.

Allowed in-game settings:

1. Audio volume, channel, and filter settings.
2. Video settings that Gubsy can apply safely at runtime.
3. UI settings.
4. Input binds, bind profiles, input settings profiles, and device assignment.
5. Local player setup where the current session can support it.

Potentially disabled in-game:

1. Profile deletion or destructive profile management.
2. Mod browser or mod activation.
3. Lobby/server browser screens that do not make sense while already in a run.
4. Settings that require a full app restart.
5. Settings that would desync an active network session unless host-owned and
   synchronized.

In-game settings should not be copied into a separate pause menu tree. The same
settings screens should adapt from the explicit `InGame` context.

The context should be explicit, for example:

```cpp
enum class GubsyMenuContext {
    Title,
    Lobby,
    InGame,
};
```

Screens can then hide or disable rows based on context without duplicating the
whole screen implementation.

## Gubsy Runtime Work

Add reusable in-game menu support to Gubsy:

1. Add a pause/in-game root screen registration.
2. Add public helpers to open, close, and query the in-game menu stack.
3. Add commands for `resume`, `open_settings`, `restart_run`, and
   `quit_to_main_menu`.
4. Reuse the existing menu manager rather than creating a second menu manager.
5. Track the active menu context so settings screens can adapt.
6. Ensure layout/nav/debug editor behavior still works for the overlay screens.
7. Add smoke coverage that opens the in-game menu, pushes settings, pops back,
   and dispatches the resume command.

Suggested public API shape:

```cpp
struct GubsyInGameMenuConfig {
    GubsyMenuCommand resume{};
    GubsyMenuCommand restart_run{};
    GubsyMenuCommand quit_to_main_menu{};
    void* user_data{};
};

void gubsy_configure_in_game_menu(GubsyRuntime& runtime,
                                  const GubsyInGameMenuConfig& config);
void gubsy_open_in_game_menu(GubsyRuntime& runtime);
void gubsy_close_in_game_menu(GubsyRuntime& runtime);
bool gubsy_in_game_menu_open(const GubsyRuntime& runtime);
```

The exact API can be smaller if the existing command registration surface is
enough. The important part is that Splonks should not poke private Gubsy menu
internals to open the pause stack.

## Splonks Integration Work

Wire the in-game overlay into Splonks through `gubsy_shell`.

1. Add `OpenInGameMenu`, `CloseInGameMenu`, and `InGameMenuOpen` wrappers in
   `src/gubsy_shell.hpp/.cpp`.
2. Register Splonks callbacks for resume, restart run, and quit to main menu.
3. Detect `Start`/`Escape` while `Mode::Playing`.
4. If the menu is closed, open it.
5. If the menu is open, route inputs to Gubsy and do not route them to gameplay.
6. Offline/local sessions pause simulation while the menu is open.
7. Multiplayer sessions keep simulation and network stepping active while the
   menu is open.
8. Render gameplay first, then draw the Gubsy menu overlay, then debug/imgui.
9. Make `Escape` close submenu screens normally before it resumes gameplay.

The Splonks callback ownership should stay explicit:

1. `resume`: close Gubsy in-game menu and clear local pause state.
2. `restart_run`: queue or load the first Splonks quest stage using Splonks
   stage/progression helpers.
3. `quit_to_main_menu`: leave/end network session as needed, clear gameplay
   transient state, return to title/main menu.

Do not make the main loop depend on internal Gubsy screen ids. The main loop
should only need to ask whether the in-game menu is open, open it from gameplay
input edges, and update/render the active Gubsy menu path.

## Local Player And Input Management

The in-game menu should be able to reach the same local-player and input-profile
tools as the lobby/settings path where those operations are safe.

1. Input binds and bind profiles are allowed in-game.
2. Input device assignment is allowed in-game.
3. Local player add/remove can be available for local/offline runs.
4. Local player add/remove in online sessions should be host/session-policy
   controlled, not blindly local-only.
5. Character selection and game-specific lobby values are Splonks-owned data
   exposed through Gubsy config/provider hooks.
6. Gubsy should not know Splonks character enums, stage ids, or respawn enum
   internals.

## Pause Policy

Splonks should own pause semantics.

Offline/local:

1. Opening the in-game menu sets a Splonks pause flag.
2. Gameplay simulation does not advance while paused.
3. Audio policy should be explicit: either pause music/emitters or let music
   continue at normal volume.
4. Debug UI can still render.

Multiplayer:

1. Opening the in-game menu does not pause lockstep/network simulation.
2. Local gameplay inputs should be neutralized or continue from last known
   state only if the netcode explicitly requires it.
3. Menu input must not leak into gameplay actions.
4. If a local player opens the menu, only that local UI focus is affected; the
   session continues.

Network pause/barrier behavior is a separate feature. Do not smuggle it into
this first pause menu implementation.

## Input Handling

The clean input rule:

1. Raw SDL events still go to Gubsy so text input and device state stay current.
2. When the in-game menu is open, Gubsy consumes menu navigation and text input.
3. Splonks gameplay input latching should not produce fresh action presses from
   the same physical inputs.
4. Opening the menu should clear or neutralize immediate gameplay inputs for
   that frame.
5. Closing the menu should avoid one-frame confirm/back leakage into gameplay.

This likely needs a small helper in Splonks:

```cpp
bool ShouldRouteGameplayInput(const State& state, const gubsy_shell::Shell& shell);
```

That helper should be boring and visible in the main loop/input path.

Opening or closing the menu should also suppress gameplay input for the current
frame and a small follow-up guard frame. This prevents the same `Start`,
`Escape`, or confirm press from opening the menu and immediately selecting,
backing out, jumping, bombing, or otherwise affecting gameplay.

## Rendering

Initial rendering should be simple:

1. Render Splonks gameplay as usual.
2. Draw the Gubsy frame/menu overlay on top when the in-game menu is open.
3. Use existing Gubsy layout coordinates and menu renderer.
4. Add a dim background panel if needed, but do not block the functional slice
   on visual polish.

If the current title menu path assumes a full-screen Gubsy frame, split naming
so the same update/render helpers can be called for title menus and overlays
without implying Gubsy owns the whole screen.

The overlay does not need a separate renderer. Splonks renders gameplay into
the Gubsy-owned frame/render target, then Gubsy renders the active menu overlay
on top before present/debug UI.

## Implementation Order

1. Add the Gubsy in-game menu screen and public open/close/query helpers.
2. Add Gubsy smoke tests for opening, closing, and dispatching resume.
3. Add the Splonks `gubsy_shell` wrappers and callbacks.
4. Add Splonks input detection for `Start`/`Escape` while playing.
5. Gate Splonks gameplay input while the menu is open.
6. Add offline pause behavior while the menu is open.
7. Keep multiplayer simulation running while the menu is open.
8. Render the overlay over gameplay.
9. Reuse Gubsy settings screens from the pause menu with an in-game context.
10. Disable or hide settings rows that are unsafe in-game.
11. Add restart-run behavior.
12. Add quit-to-main-menu behavior.
13. Add local player/input profile management from the in-game settings path.
14. Add tests/smokes for the Splonks shell callbacks where practical.
15. Playtest keyboard, controller, text input, local pause, and online no-pause.

Recommended commit slices:

1. Gubsy in-game menu API and smoke tests.
2. Splonks shell wrappers and command callbacks.
3. Splonks main-loop/input routing and offline pause behavior.
4. Overlay rendering and settings-context restrictions.
5. Restart/quit/session behavior and manual polish fixes.

## Files Likely Touched

Gubsy:

1. `include/gubsy/runtime.hpp` or another public runtime header.
2. `src/menu/screens/...` for the pause root screen.
3. `src/menu/...` for context/open-close helpers if needed.
4. `src/runtime.cpp` or equivalent runtime command registration area.
5. `data/ui_layouts/layouts.lisp` only if the existing settings layout is not
   sufficient.
6. Existing smoke tests, or a new in-game menu smoke.

Splonks:

1. `src/gubsy_shell.hpp`
2. `src/gubsy_shell.cpp`
3. `src/main.cpp`
4. `src/inputs.cpp`
5. `src/step.cpp`
6. Stage progression helpers if restart needs a cleaner public helper.
7. Shell smoke tests.

## Verification

Gubsy:

```sh
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

Splonks:

```sh
./scripts/build.sh
ctest --test-dir build --output-on-failure
```

Manual checks:

1. Offline game: `Escape` opens the menu and the world freezes.
2. Offline game: `Start` opens the menu and the world freezes.
3. Resume returns to gameplay without triggering a gameplay action.
4. Settings opens from pause and Back returns to pause.
5. Bind/profile/device screens are reachable from in-game settings.
6. Restart run returns to the first stage cleanly.
7. Quit to main menu returns to the Gubsy title/main menu cleanly.
8. Multiplayer game: menu opens without stopping network stepping.
9. Multiplayer game: menu input does not leak into gameplay controls.
10. Text input fields consume Backspace/Escape appropriately.

## Remaining Decisions

1. Audio while paused: leave music alone initially unless it causes obvious
   problems. SFX emitters can continue or be handled by Splonks later.
2. Multiplayer `Restart Run`: hide or disable initially unless the local player
   is host. Later it should become a host/session command.
3. Multiplayer `Quit to Main Menu`: disconnect/leave immediately for the first
   implementation. Add confirmation only if accidental exits become a problem.
4. Online local-player add/remove: keep the UI path available only where the
   session policy can apply it correctly.
