# Settings Schema Plan

## Goals
- Replace the dual `GameSettingsSchema` / `TopLevelGameSettingsSchema` APIs with a single declarative schema that describes **all** settings.
- Store metadata (scope, widget type, categories, defaults) once and reuse it for persistence, reconciliation, and menu generation.
- Stop using ad-hoc `.ini` files (e.g., audio.ini); all authored + runtime settings should live in `data/settings_profiles/*.lisp`.
- Provide built-in fallbacks when reading settings so callers never need to pass explicit default values.

## Data Model
```cpp
enum class SettingScope { Install, Profile };

enum class SettingWidgetKind {
    Toggle,
    Slider,
    Option,
    Text,
    Custom
};

struct SettingOption {
    std::string value;
    std::string label;
};

struct SettingWidgetDesc {
    SettingWidgetKind kind{SettingWidgetKind::Toggle};
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.0f;
    int max_text_len = 0;
    std::vector<SettingOption> options;
    std::string custom_id;
};

struct SettingMetadata {
    SettingScope scope{SettingScope::Profile};
    std::string key;
    std::string label;
    std::string description;
    std::vector<SettingCategory> categories;   // multi-category support
    int order = 0;
    SettingWidgetDesc widget;
    GameSettingsValue default_value;            // int, float, string, vec2
};
```

A single global `SettingsSchema` holds `std::vector<SettingMetadata> entries`. Games call `global_settings_schema().add_setting(meta)` to register everything.

## Storage
- Install-wide (`SettingScope::Install`) entries map to `TopLevelGameSettings` stored in `data/settings_profiles/top_level_game_settings.lisp`.
- Profile-scoped entries map to each `GameSettings` instance stored in `data/settings_profiles/game_settings.lisp`.
- Audio, mods-enabled flags, etc. migrate from `config/*.ini` / JSON into the same `data` hierarchy (all S-expression).

## Reconciliation Flow
1. On startup, read existing S-expression files.
2. Walk the schema entries:
    - Ensure each key exists in the appropriate map; if missing, insert `default_value`.
    - If a stored value has the wrong variant type, replace it with the default.
    - Remove unknown keys that no longer exist in the schema (optional: log warning).
3. Persist updated maps back to disk (`save_*_settings`).

## Menu Integration (Primary Consumer)
The new settings menu stack is the first consumer of this schema:
- During hub/category screen build, it walks `global_settings_schema().entries()`, filters entries by category/scope, and spawns widgets based on `widget.kind`.
- Each widget stores a pointer to the underlying value (top-level or profile map) so the menu directly edits live settings. Multi-category entries reuse the same pointer, so changes stay in sync across sections.
- Built-in categories (Video, Audio, Controls, Gameplay, Profiles, Players & Devices, Binds, Saves) map to slots in the cloned layout; developers can add more categories via metadata.
- Future consumers (e.g., ImGui debug panels, profile editors) will use this same schema API to stay in sync with the core settings data.

## Getter API + Usage Examples
Provide type-specific getters without explicit default arguments. They return the stored value or a built-in fallback (0 / 0.0f / "" / {0,0}) if the key is missing or the type mismatches.

```cpp
// Profile-scoped value (per user)
GameSettings active = load_game_settings(user_profile.last_game_settings);
float hud_scale = get_game_setting_float(active, "hud.scale"); // defaults to 1.0f if missing
int difficulty = get_game_setting_int(active, "gameplay.difficulty"); // default 0 = Normal
std::string subtitle_font = get_game_setting_string(active, "ui.subtitle_font");
GameSettingsVec2 camera_offset = get_game_setting_vec2(active, "camera.offset");

// Install-wide value (same for all profiles)
const TopLevelGameSettings& global = es->top_level_game_settings;
bool vsync_on = get_top_level_setting_int(global, "video.vsync") != 0;
std::string render_mode = get_top_level_setting_string(global, "video.render_mode");
```

If a caller needs to know whether a value existed before falling back, we can provide optional overloads:

```cpp
std::optional<int> maybe = try_get_game_setting_int(active, "foo");
if (maybe) { /* ... */ }
```

## Menu Widget Binding Example
```cpp
MenuWidget make_slider_from_schema(const SettingMetadata& meta, float* value_ptr) {
    MenuWidget w;
    w.id = next_widget_id();
    w.slot = resolve_slot_for(meta);
    w.type = WidgetType::Slider1D;
    w.label = meta.label.c_str();
    w.secondary = meta.description.c_str();
    w.bind_ptr = value_ptr;          // pointer into GameSettings/TopLevelGameSettings map
    w.min = meta.widget.min;
    w.max = meta.widget.max;
    w.on_left = MenuAction::delta_float(value_ptr, -meta.widget.step);
    w.on_right = MenuAction::delta_float(value_ptr, meta.widget.step);
    return w;
}
```

## Migration Tasks
1. **File locations:** move everything from `config/` into `data/...` folders; delete old references in docs and code.
2. **Audio settings:** replace the INI reader/writer with S-expression storage under `data/settings_profiles/audio.lisp`.
3. **Mods config:** switch `mods_enabled.json` to `data/mods_enabled.lisp` (or embed inside the settings schema if appropriate).
4. **Docs:** update `docs/PROFILES_GUIDE.md`, `docs/menu_reference.md`, etc., to describe the new paths + S-expression formats instead of INI.
5. **Menu builder:** consume the schema entries rather than handwritten navigation tables once the Audio/Video pages pivot to the new system.

## Open Questions
- Do we need schema versioning / migration hooks for settings that change type? (Likely yes; store `version` in metadata.)
- Should custom widgets register callbacks in metadata (`custom_id`, function pointer)? (Needed for non-standard controls.)
- Decide when to delete the old `ui/menu` legacy code entirely; currently it still references the INI files.
