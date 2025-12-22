# Menu System Reference

This document summarizes the current in-game menu pages, their behavior, and the main code that drives them. Use it as a map before replacing screens with the upcoming GUI editor.

## Global Behavior
* Entry point is `step_menu_logic` and `render_menu` (`src/main_menu/menu_step.cpp`, `menu_render.cpp`).
* Navigation:
  * Keyboard/controller inputs are debounced in `collect_menu_inputs` and stored in `ss->menu_inputs`.
  * Focus is tracked by `ss->menu.focus_id`. `ensure_focus_default` chooses the first enabled control per page.
  * `nav_*_for_id` functions define per-page graph edges; sliders consume left/right for value changes.
* Mouse:
  * Hover steals focus if the mouse was the last-used device; clicking always activates.
  * Sliders store `ss->menu.dragging_id` while held.
* Text Input:
  * `open_text_input(mode, limit, initial, target, title)` enables modal input for search boxes, bind names, lobby seed/name, etc.
  * `Esc` cancels, `Enter` submits, `Backspace` deletes. While active, menu navigation is suppressed.
* Audio feedback:
  * Slider drags preview audio via `play_audio_slider_preview`.
  * Movement between focus targets plays `base:ui_cursor_move` (and directional sounds on slider adjustments).
* Persistence:
  * Audio sliders flush to `data/settings_profiles/audio.lisp` using `save_audio_settings`.
  * Video state stored in `ss->menu` until `Apply` is pressed.
  * Input profiles live under `data/binds_profiles/*.lisp`.
  * Lobby settings persisted in state only (mods filtered via `set_demo_item_mod_filter` when session starts).

## Page Overview

| Page ID | Description | Primary File(s) |
| --- | --- | --- |
| `MAIN` | Root menu (Play / Mods / Settings / Quit) | `menu_buttons.cpp`, `menu_step.cpp` |
| `SETTINGS` | Hub for Audio, Video, Controls, Binds, Players, Other | same |
| `AUDIO` | Volume sliders with real-time preview | `menu_buttons.cpp`, `menu_step.cpp` |
| `VIDEO` | Resolution/mode controls with Apply button | same |
| `CONTROLS` | Sensitivity, vibration toggles/intensity | same |
| `BINDS` | Action binding grid (keyboard/controller) | `menu_buttons.cpp`, `menu_step.cpp`, `menu_binds.cpp` |
| `BINDS_LOAD` | Preset list (rename/delete/activate) | `menu_buttons.cpp`, `menu_step.cpp` |
| `PLAYERS` | Local player preset assignment | same |
| `OTHER` | Placeholder (currently only Back) | same |
| `MODS` | Online mod catalog browser | `menu_mods.cpp`, `menu_step.cpp` |
| `LOBBY` | Game options / active mods summary | `menu_lobby.cpp` |
| `LOBBY_MODS` | Session mod toggle screen | `menu_lobby.cpp` |

Each section below details specific behaviors.

---

## Main Menu (`MAIN`)
* Buttons: Play (100), Mods (110), Settings (101), Quit (102).
* Behavior:
  * Play → `enter_lobby_page()` (opens Game Options screen).
  * Mods → `enter_mods_page()` (loads catalog if needed).
  * Settings → `SETTINGS` hub.
  * Quit → sets `ss->running = false`.

## Settings Hub (`SETTINGS`)
* Buttons (`menu_buttons.cpp`):
  * Video (201) → `VIDEO`
  * Audio (200) → `AUDIO`
  * Controls (202) → `CONTROLS`
  * Binds (204) → `BINDS`
  * Players (205) → `PLAYERS`
  * Other (203) → `OTHER`
  * Back (299) → `MAIN`

### Audio (`AUDIO`)
* Sliders for master/music/SFX volumes (IDs 300-302).
* Dragging previews audio pips; releasing triggers `SliderPreviewEvent::Release` and writes to `data/settings_profiles/audio.lisp`.
* Back (399) returns to `SETTINGS`.

### Video (`VIDEO`)
* Controls:
  * Resolution (400): cycles `ss->menu.video_res_index`, limited to four hardcoded resolutions.
  * Window Size (405): cycles preset window sizes (enabled only when window mode is “Windowed”).
  * Window Mode (401): toggles windowed → borderless → fullscreen.
  * VSync (402): toggle.
  * Frame Rate Limit (403): cycles fixed caps.
  * UI Scale (404): slider stored in `ss->menu.ui_scale`.
* Back (499) returns without applying pending changes.
* Apply (498) enabled when resolution or mode differs from actual SDL window flags. Clicking applies via `SDL_SetWindowSize` or toggling fullscreen flags.

### Controls (`CONTROLS`)
* Sliders:
  * Screen Shake (500) stored in `ss->menu.screen_shake`.
  * Mouse Sensitivity (501) (log-style adjustments using 0.01 increments).
  * Controller Sensitivity (502).
  * Vibration Intensity (507), enabled only if Vibration Enabled is true.
* Toggles:
  * Vibration Enabled (505).
  * Additional toggles such as invert axes exist in `menu_step.cpp` (IDs 503-505).
* Back (599) returns to `SETTINGS`.

### Binds (`BINDS`)
* Five visible actions per page (IDs 700-704). `ss->menu.binds_keys_page` indexes additional actions.
* Selecting an action opens capture mode (handled in `menu_binds.cpp` / `engine/input.cpp`). `Esc` cancels capture.
* Footer buttons:
  * Back (799)
  * Reset to Defaults (791) → `apply_default_bindings_to_active()`
  * Load Preset (792) → `BINDS_LOAD`
  * Save As New (793) → opens text input, sanitized via `sanitize_profile_name`, writes to `data/binds_profiles`.
  * Undo Changes (794) → restores snapshot captured when entering page.
* Additional features:
  * Duplicate name prevention when saving.
  * Profiles limited to 24 characters, alphanumeric / `_` / `-`.
  * `kMaxBindsProfiles` (50) enforced in `menu_binds.cpp`.

### Binds – Load Preset (`BINDS_LOAD`)
* Shows existing profiles stored under `data/binds_profiles`.
* Rows 720-724 show preset names, 730-734 rename, 740-744 delete (disabled for read-only `Default`).
* Back (799) returns to binds grid.
* Paging (5 per page) controlled by `ss->menu.binds_list_page`.

### Players (`PLAYERS`)
* Allows assigning input presets to up to `ss->menu.players_count` local players.
* Buttons 900+ open a cycle list of available presets.
* `Add Player` (980) increments count, `Remove Player` (981) decrements.
* Back (999) returns to `SETTINGS`.
* note, that players should be replaced with "Profiles" probably. it isnt well developed at all. 

### Other (`OTHER`)
* Placeholder page with only Back (899). No extra functionality currently.

---

## Mods Browser (`MODS`)
* Catalog fetched from `ss->menu.mods_server_url` using `httplib` (see `menu_mods.cpp`).
* Search (button 950) opens text input (`TEXT_INPUT_MODS_SEARCH`), filters catalog by title/summary/author.
* Paging: Prev (951) and Next (952) across `kModsPerPage` cards (currently 4 per page).
* Each card (IDs 900–903) displays:
  * Title, author, version, summary, dependency list (rendered in `menu_render.cpp`).
  * Status indicator: `Core`, `Installed`, `Not installed`, or transitional text (Installing/Removing with counters).
  * Action button on right toggles install/uninstall.
* Install flow (`install_mod_by_index`):
  * Validates server endpoint (supports `http://host[:port]`).
  * Recursively installs dependencies via `install_recursive`, detecting cycles, downloading all files under `mods/<folder>`.
  * Updates runtime assets: `discover_mods`, `scan_mods_for_sprite_defs`, `load_all_textures_in_sprite_lookup`, `load_mod_sounds`, `load_demo_item_defs`.
* Uninstall flow (`uninstall_mod_by_index`):
  * Prevents removal if another installed mod lists it as a dependency.
  * Deletes folder, refreshes runtime, updates flags.
* Required mods (`entry.required == true`) always show as installed and cannot be removed.
* Back (998) returns to `MAIN`.

---

## Game Options / Lobby (`LOBBY`)
* Buttons (IDs 1400–1441) render in `menu_lobby.cpp`:
  * Lobby Name (text input), Privacy (cycle Solo/Friends/Public), Invite Friends (placeholder), Scenario (currently only "Demo Pad Arena"), Difficulty (Easy/Normal/Hard), Max Players (1–64), Allow Join (toggle), Seed (text input), Randomize Seed (generates 8-digit hex).
  * Right panel shows “Active Mods” summary assembled from `LobbyModEntry` list. Always includes required/core mods plus optional toggles.
  * Manage Mods (1409) opens `LOBBY_MODS` subpage.
  * Start Game (1440): validates dependencies via `dependencies_satisfied`. On success, calls `set_demo_item_mod_filter`, `load_demo_item_defs`, and switches `ss->mode = modes::PLAYING`.
  * Back (1441) returns to `MAIN`.
* Layout rectangles support manual overrides through `menu_layout.cpp`. Additional nav editing currently lives in `menu_navgraph.cpp`.

### Lobby Mods (`LOBBY_MODS`)
* Displays session mod entries with status badges:
  * Each row shows title, author, description, dependencies, and whether it’s `Core`, `Enabled`, or `Disabled`.
  * Clicking toggles `entry.enabled` for non-required mods.
* Paging via Prev (1430) / Next (1431) across `kLobbyModsPerPage` entries.
* Back (1450) returns to the main lobby page.
* Selected mod list is summarized back on the lobby panel (first few names + “…and N more”).

---

## Input Binding Helpers
* `menu_binds.cpp` contains:
  * `apply_default_bindings_to_active`, `undo_active_bind_changes`, `delete_binds_profile`, `set_active_binds_profile`.
  * Dirty tracking via `ss->menu.binds_snapshot`.
  * Text input validations: disallow spaces, sanitize to `[A-Za-z0-9_-]`, enforce length ≤ 24.
  * Profiles stored under `data/binds_profiles`, with `Default` treated as read-only baseline.
* `input_active_profile.txt` stores the last used preset name.

---

## Mods Installer / Network Notes
* Catalog endpoint: `http://host[:port]/mods/catalog`.
* File downloads: `http://host[:port]/mods/files/<mod_id>/<relative path>` (URL-escaped).
* `mods/install_state` map stores boolean installed flags for UI refresh.
* Server interaction happens on the menu thread (blocking calls) with a 5s timeout; status text updates reflect progress.

---

## Lobby State & Session Start
* `LobbySettings` (`state.hpp`) stores:
  * `session_name`, `privacy`, `scenario_index`, `difficulty`, `max_players`, `allow_join`, `seed`, modulo per-page flags (mod list, selected mod index, page).
* When starting a session:
  * `enabled_mod_ids()` returns all required+enabled IDs.
  * `set_demo_item_mod_filter(ids)` filters definitions loaded through the demo content system.
  * On failure (missing dependency, load error), user sees an alert and start is aborted.

---

## Miscellaneous UI
* Alerts:
  * Pushed into `ss->alerts` as `Alert{text, age, ttl, purge_eof}`. Displayed by overlay renderer (not part of menus).
* Help text:
  * Binds page displays hints (“Prev: Q Next: E Back: Esc” or controller equivalents).
* Search boxes:
  * Mods search (button 950) uses `TEXT_INPUT_MODS_SEARCH`.
  * Lobby name/seed inputs use `TEXT_INPUT_LOBBY_NAME` and `TEXT_INPUT_LOBBY_SEED`.

---

## Relevant Files
| File | Purpose |
| --- | --- |
| `src/main_menu/menu_buttons.cpp` | Defines controls for each page. |
| `src/main_menu/menu_step.cpp` | Handles input, navigation, activation, page transitions, text input bridging. |
| `src/main_menu/menu_render.cpp` | Renders shared UI chrome for most screens. |
| `src/main_menu/menu_lobby.cpp` | Custom rendering + logic for lobby and lobby-mods screens. |
| `src/main_menu/menu_mods.cpp` | Mod catalog networking, filtering, install/uninstall, search. |
| `src/main_menu/menu_binds.cpp` | Bind persistence, preset management, dirty tracking. |
| `src/main_menu/menu_layout.cpp` | Layout override storage/editor (Ctrl+L). |
| `src/main_menu/menu_navgraph.cpp` | Prototype nav graph editor and persistence. |
| `src/config.cpp/.hpp` | Input profile sanitization, path helpers. |
| `src/state.hpp` | Stores menu, lobby, and mod state referenced above. |

Use this reference while evaluating the new GUI editor so that existing behaviors are preserved or intentionally replaced.
