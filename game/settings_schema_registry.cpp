#include "game/settings_schema_registry.hpp"

#include "engine/settings_schema.hpp"

#include <initializer_list>

namespace {

void assign_categories(SettingMetadata& meta, std::initializer_list<const char*> categories) {
    for (const char* cat : categories)
        meta.categories.emplace_back(cat);
}

SettingMetadata make_slider_setting(const char* key,
                                    const char* label,
                                    const char* description,
                                    std::initializer_list<const char*> categories,
                                    float min,
                                    float max,
                                    float step,
                                    float default_value) {
    SettingMetadata meta{};
    meta.scope = SettingScope::Profile;
    meta.key = key;
    meta.label = label;
    meta.description = description;
    assign_categories(meta, categories);
    meta.widget.kind = SettingWidgetKind::Slider;
    meta.widget.min = min;
    meta.widget.max = max;
    meta.widget.step = step;
    meta.default_value = default_value;
    return meta;
}

SettingMetadata make_toggle_setting(const char* key,
                                    const char* label,
                                    const char* description,
                                    std::initializer_list<const char*> categories,
                                    bool default_on) {
    SettingMetadata meta{};
    meta.scope = SettingScope::Profile;
    meta.key = key;
    meta.label = label;
    meta.description = description;
    assign_categories(meta, categories);
    meta.widget.kind = SettingWidgetKind::Toggle;
    meta.default_value = default_on ? 1 : 0;
    return meta;
}

SettingMetadata make_option_setting(const char* key,
                                    const char* label,
                                    const char* description,
                                    std::initializer_list<const char*> categories,
                                    std::initializer_list<SettingOption> options,
                                    const char* default_value) {
    SettingMetadata meta{};
    meta.scope = SettingScope::Profile;
    meta.key = key;
    meta.label = label;
    meta.description = description;
    assign_categories(meta, categories);
    meta.widget.kind = SettingWidgetKind::Option;
    meta.widget.options.assign(options.begin(), options.end());
    meta.default_value = std::string(default_value);
    return meta;
}

} // namespace

void register_game_settings_schema_entries() {
    SettingsSchema schema;

    schema.add_setting(make_toggle_setting("demo.controls.invert_x_axis",
                                           "Invert X-Axis",
                                           "Flip horizontal look direction.",
                                           {"Controls"},
                                           false));
    schema.add_setting(make_slider_setting("demo.hud.scale",
                                           "HUD Scale",
                                           "Resize HUD elements.",
                                           {"HUD"},
                                           0.5f,
                                           1.5f,
                                           0.05f,
                                           1.0f));
    schema.add_setting(make_slider_setting("demo.hud.opacity",
                                           "HUD Opacity",
                                           "Set HUD transparency.",
                                           {"HUD"},
                                           0.2f,
                                           1.0f,
                                           0.05f,
                                           0.85f));
    schema.add_setting(make_toggle_setting("demo.hud.objective_markers",
                                           "Objective Markers",
                                           "Show or hide guidance markers.",
                                           {"HUD"},
                                           true));
    schema.add_setting(make_option_setting("demo.hud.minimap_mode",
                                           "Minimap Mode",
                                           "Configure the minimap style.",
                                           {"HUD"},
                                           {SettingOption{"corner", "Corner"},
                                            SettingOption{"expanded", "Expanded"},
                                            SettingOption{"disabled", "Disabled"}},
                                           "corner"));
    schema.add_setting(make_slider_setting("demo.accessibility.camera_shake",
                                           "Camera Shake",
                                           "Adjusts camera shake intensity.",
                                           {"Accessibility"},
                                           0.0f,
                                           1.0f,
                                           0.01f,
                                           1.0f));
    schema.add_setting(make_toggle_setting("demo.debug.show_hitboxes",
                                           "Show Hitboxes",
                                           "Render debug hitboxes over actors.",
                                           {"Debug"},
                                           false));

    register_settings_schema(schema);
}
