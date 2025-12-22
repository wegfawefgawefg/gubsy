# Settings Menu Generation Plan

## Goals
- Generate fully navigable settings screens from schemas, instead of hand-authoring every widget.
- Blend engine-provided settings (audio master/music/sfx, render/window options, reticle scale, etc.) with developer-defined keys.
- Keep input/navigation consistent with the existing menu manager (mouse, keyboard, controller) and leverage UI layouts.
- Reuse schemas already registered through `register_game_settings_schema` / `register_top_level_game_settings_schema` so a single truth source drives persistence **and** UI.
- Allow future schema updates to automatically ripple through menus without touching rendering code.

## Data Sources
1. **Top-level (installation) settings** – SDL/window/audio knobs (window size, fullscreen mode, render scale presets, master/music/sfx volumes). These live under `TopLevelGameSettings` today.
2. **Per-user game settings** – developer schema describing gameplay/accessibility preferences (difficulty, HUD scale, subtitle font, etc.). Stored as `GameSettings` keyed by `user_profile.last_game_settings`.
3. **Input settings** – sensitivity/invert data already has its own schema; for the first pass we expose only the subset relevant to the Settings > Controls pages (mouse sens, controller sens, invert toggles).

Engine-owned keys must always be present; developer keys can be appended to categories.

## Schema Extensions
Current schemas only store key + default value. To render widgets we also need:
- Display label & optional description/tooltip
- Widget type hint (toggle, slider, dropdown, text)
- Numeric bounds or option lists
- Category (`Audio`, `Video`, `Controls`, `Gameplay`, `Accessibility`, custom)
- Page order weight (so dev can interleave items around the built-in ones)

Plan: extend schema entry structs:
```cpp
struct SettingMetadata {
    std::string key;
    SettingKind kind;      // Toggle, Slider, Option, Text
    std::string label;
    std::string description;
    float min, max;        // for sliders
    std::vector<SettingOption> options; // for dropdowns
    SettingCategory category;
    int order;             // ordering inside category
};
```
`GameSettingsSchema` and `TopLevelGameSettingsSchema` get helpers like `add_slider(key, label, min, max, default)`.

## Catalog Build Pipeline
1. **Registration**
    - During startup, the game calls `register_game_settings_schema(schema)` and (optionally) `register_global_settings_schema(schema)`.
    - Engine also registers its built-in schema entries (audio sliders, render/window selectors, UI scale, etc.).
2. **Merge**
    - A new `SettingsCatalog` combines both sources:
        * `catalog.top_level_entries` (top-level settings only)
        * `catalog.user_entries` (per-user settings)
    - Each entry knows where the data lives (pointer into `TopLevelGameSettings`, `GameSettings`, or `InputSettingsProfile`).
3. **Category Buckets**
    - Group entries by `SettingCategory`. Built-in categories (Audio, Video, Controls, Gameplay, Accessibility) exist by default.
    - Developer can define new categories which create new menu subpages automatically.
4. **Page Slicing**
    - If a category has more entries than can fit in one layout (e.g., >6 controls), automatically slice into `Category Page 1`, `Category Page 2`, etc., while keeping Prev/Next nav buttons enabled.

## Menu Building
- The **Settings Hub** reuses the "card list" layout from the Mods screen. We clone that layout into a reusable `SettingsSubmenu` layout family (desktop/tablet/mobile) consisting of:
    * Title row, optional description text.
    * Search field row (future filtering hook).
    * Prev/Next buttons aligned to the right for page navigation (LB/RB shortcuts still work).
    * Four card slots per page for buttons (Video, Audio, Controls, Gameplay, Profiles, Binds). Additional categories automatically flow into the card list and paginate.
    * `Back` button row anchored at the bottom.
- Each settings **category page** (Audio, Video, Controls, Gameplay) uses the same layout, but instead of category buttons the card slots host actual controls (sliders/toggles). When the schema produces more items than fit, we paginate using the built-in Prev/Next buttons.
- Accessibility-specific options live inside whichever category they belong to (e.g., color filters inside Video, subtitles under Audio). There is no dedicated Accessibility top-level button.
- `MenuScreenDef`s for these pages simply iterate the catalog bucket for their category/page and emit widgets anchored to the slot IDs defined in the layout.
- Widget helper factories: `make_slider(entry)`, `make_toggle(entry)`, `make_option(entry)`, `make_text(entry)`, each wiring `MenuAction::delta_float`, `MenuAction::toggle_bool`, `MenuAction::run_command`, etc.
- Engine-provided sections:
    1. **Settings Hub** – auto-populated card list for every category.
    2. **Video** – render & window presets, window mode, render-scale fit, VSync, FPS cap, safe area, UI scale; gamma/brightness/color-filter controls appear here (disabled until implemented).
    3. **Audio** – master/music/SFX sliders plus an output-device selector (mute = master 0).
    4. **Controls** – mouse & controller sensitivities, per-axis invert, vibration intensity (0–100%), quick links to Input Settings Profiles and Binds.
    5. **Gameplay/HUD** – HUD scale, HUD opacity, subtitle toggle/size/background; developer-provided gameplay settings land here.
    6. **Profiles** – create/rename/delete user profiles, choose shared binds/settings profiles.
    7. **Players & Devices** – assign physical devices to players, add/remove players.
    8. **Binds** – choose a binds profile, edit actions, clear/reset.
    9. **Saves** – browse/delete saves, filter per profile.
    10. Additional categories auto-create when schema metadata defines new `SettingCategory`s. Accessibility-specific options (color filters, subtitles) live inside their natural parent categories.

## Widget Types & Metadata
Schema metadata needs to describe the UI control that represents a setting. Minimal list of widget kinds and arguments:

| Kind          | Description                                        | Required metadata                                   |
|---------------|----------------------------------------------------|-----------------------------------------------------|
| `Toggle`      | Boolean on/off                                     | `label`, optional `description`                     |
| `Slider`      | Continuous value                                   | `label`, `min`, `max`, optional step, formatter     |
| `Option`      | Discrete choices (dropdown/left-right)             | `label`, ordered list of `{value,label}` options    |
| `Text`        | Free-form UTF-8 input (limited use)                | `label`, `max_len`, optional validator callback     |
| `Button`      | Execute a command (reset defaults, open subpage)   | `label`, `command_id`                               |
| `Custom`      | Developer-provided render/adjust hook              | `label`, `custom_id`, callback pointers             |

Each entry also stores `category`, `order`, and `layout_slot_preference` (optional) so we can pack the page predictably.

## Navigation Graph Generation
Manual `nav_*` links remain supported (and are what the future layout-editor nav tool will write). For generated pages we can use a helper that, when no manual neighbor is provided, locates the closest widget in each direction by comparing slot centers. The plan:
1. After layout resolution, compute rect centers per widget.
2. For each widget, if `nav_left` et al. are still invalid, choose the neighbor whose center lies in that direction with the smallest distance.
3. Apply the computed IDs just before caching `g_cache.widgets`, so existing hard-coded nav graphs keep working.

This way the current manual wiring isn’t invalidated, but new dynamically generated widgets still get controller navigation for free.

## Interaction + Persistence
- Widgets modify settings immediately in-memory (`GameSettings`, `TopLevelGameSettings`, `InputSettingsProfile`).
- Each category page owns an `Apply`/`Revert` action when the underlying setting requires confirmation (resolution changes, language reload). Simpler fields (volumes, sensitivities) apply instantly.
- `MenuAction::RunCommand` can dispatch to `CMD_APPLY_VIDEO_SETTINGS`, `CMD_RESET_TO_DEFAULTS`, etc.
- On exit (`Back`), pending unapplied changes produce a confirm dialog (future nice-to-have).

## Developer Hooks
- Games register schema entries with metadata and optionally supply callbacks for custom widgets (`SettingKind::Custom` → `on_render` + `on_adjust` function pointer).
- They can also hide engine defaults if desired (e.g., a game that never exposes render settings can mark those entries as hidden in `SettingsCatalogConfig`).

## Files & Structure
- `docs/settings_menu_plan.md` (this doc)
- Engine code additions (future PRs):
    * `engine/settings_catalog.hpp/cpp` – merges schemas, stores metadata, exposes buckets.
    * `engine/menu/screens/settings_hub.cpp`, `settings_category_screen.cpp` – build widgets from catalog.
    * `engine/menu/nav_builder.hpp` – shared nav auto-generation (used by settings + mods + future menus).
    * Schema metadata updates in `engine/game_settings.hpp`, `engine/top_level_game_settings.hpp`, `engine/input_settings.hpp`.

## Open Questions / Next Steps
1. Define the exact metadata API (C++ helpers) and migrate existing schema registration code.
2. Enumerate engine-owned entries (audio volumes, render/window, UI scale) and decide on defaults per platform.
3. Implement nav auto-builder and retrofit existing menus to it.
4. Decide on persistence UX for “dangerous” settings (Apply/Cancel pattern?).
5. Create layouts for each settings category (desktop/tablet/mobile variants) via the layout editor.
6. Expose developer configuration to reorder/hide categories or inject custom sections.
