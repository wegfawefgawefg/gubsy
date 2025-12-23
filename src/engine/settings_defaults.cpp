#include "engine/settings_defaults.hpp"

#include "engine/settings_schema.hpp"

#include <initializer_list>

namespace {

void assign_categories(SettingMetadata& meta, std::initializer_list<const char*> categories) {
    for (const char* cat : categories)
        meta.categories.emplace_back(cat);
}

SettingMetadata make_slider_setting(SettingScope scope,
                                    const char* key,
                                    const char* label,
                                    const char* description,
                                    std::initializer_list<const char*> categories,
                                    float min,
                                    float max,
                                    float step,
                                    float default_value) {
    SettingMetadata meta{};
    meta.scope = scope;
    meta.key = key;
    meta.label = label;
    meta.description = description;
    assign_categories(meta, categories);
    meta.widget.kind = SettingWidgetKind::Slider;
    meta.widget.min = min;
    meta.widget.max = max;
    meta.widget.step = step;
    meta.widget.max_text_len = 8;
    meta.default_value = default_value;
    return meta;
}

SettingMetadata make_toggle_setting(SettingScope scope,
                                    const char* key,
                                    const char* label,
                                    const char* description,
                                    std::initializer_list<const char*> categories,
                                    bool default_on) {
    SettingMetadata meta{};
    meta.scope = scope;
    meta.key = key;
    meta.label = label;
    meta.description = description;
    assign_categories(meta, categories);
    meta.widget.kind = SettingWidgetKind::Toggle;
    meta.default_value = default_on ? 1 : 0;
    return meta;
}

SettingMetadata make_option_setting(SettingScope scope,
                                    const char* key,
                                    const char* label,
                                    const char* description,
                                    std::initializer_list<const char*> categories,
                                    std::initializer_list<SettingOption> options,
                                    const char* default_value) {
    SettingMetadata meta{};
    meta.scope = scope;
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

void register_engine_settings_schema_entries() {
    SettingsSchema schema;

    // Localization
    schema.add_setting(make_option_setting(
        SettingScope::Install,
        "gubsy.localization.language",
        "Language",
        "Sets the UI language.",
        {"Profiles"},
        {SettingOption{"en", "English"}, SettingOption{"es", "Spanish"}, SettingOption{"fr", "French"}},
        "en"));

    // Audio
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.audio.master_volume",
                                           "Master Volume",
                                           "Controls the overall output volume for engine-managed audio.",
                                           {"Audio"},
                                           0.0f,
                                           1.0f,
                                           0.01f,
                                           1.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.audio.music_volume",
                                           "Music Volume",
                                           "Controls the music mix level.",
                                           {"Audio"},
                                           0.0f,
                                           1.0f,
                                           0.01f,
                                           0.8f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.audio.sfx_volume",
                                           "SFX Volume",
                                           "Controls the volume for sound effects.",
                                           {"Audio"},
                                           0.0f,
                                           1.0f,
                                           0.01f,
                                           1.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.audio.ui_volume",
                                           "UI Volume",
                                           "Adjusts menu and interface sound effects.",
                                           {"Audio"},
                                           0.0f,
                                           1.0f,
                                           0.01f,
                                           0.75f));
    schema.add_setting(make_option_setting(SettingScope::Install,
                                           "gubsy.audio.output_device",
                                           "Output Device",
                                           "Selects the preferred audio output device.",
                                           {"Audio"},
                                           {SettingOption{"default", "System Default"},
                                            SettingOption{"headset", "Headset"},
                                            SettingOption{"speakers", "Speakers"}},
                                           "default"));
    schema.add_setting(make_option_setting(SettingScope::Install,
                                           "gubsy.audio.dynamic_range",
                                           "Dynamic Range",
                                           "Choose a mixing profile for different listening environments.",
                                           {"Audio"},
                                           {SettingOption{"studio", "Studio"},
                                            SettingOption{"night", "Night Mode"},
                                            SettingOption{"tv", "TV Speakers"}},
                                           "studio"));

    // Video / Display
    schema.add_setting(make_option_setting(SettingScope::Install,
                                           "gubsy.video.render_resolution",
                                           "Render Resolution",
                                           "Target internal render resolution.",
                                           {"Video"},
                                           {SettingOption{"3840x2160", "4K"},
                                            SettingOption{"2560x1440", "1440p"},
                                            SettingOption{"1920x1080", "1080p"},
                                            SettingOption{"1280x720", "720p"}},
                                           "1920x1080"));
    schema.add_setting(make_option_setting(SettingScope::Install,
                                           "gubsy.video.window_mode",
                                           "Display Mode",
                                           "Choose how the window is presented.",
                                           {"Video"},
                                           {SettingOption{"fullscreen", "Fullscreen"},
                                            SettingOption{"borderless", "Borderless"},
                                            SettingOption{"windowed", "Windowed"}},
                                           "fullscreen"));
    schema.add_setting(make_toggle_setting(SettingScope::Install,
                                           "gubsy.video.vsync",
                                           "V-Sync",
                                           "Synchronize frames with monitor refresh.",
                                           {"Video"},
                                           true));
    {
        SettingMetadata meta = make_slider_setting(SettingScope::Install,
                                                   "gubsy.video.frame_cap",
                                                   "Frame Rate Cap",
                                                   "Limit rendering to a target framerate.",
                                                   {"Video"},
                                                   0.0f,
                                                   1024.0f,
                                                   1.0f,
                                                   60.0f);
        meta.widget.display_precision = 0;
        meta.widget.max_text_len = 4;
        meta.widget.options = {
            SettingOption{"30", "30"},
            SettingOption{"60", "60"},
            SettingOption{"120", "120"},
            SettingOption{"144", "144"},
            SettingOption{"240", "240"},
            SettingOption{"0", "Unlimited"},
        };
        schema.add_setting(meta);
    }
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.ui_scale",
                                           "UI Scale",
                                           "Uniformly scale all UI.",
                                           {"Video"},
                                           0.75f,
                                           1.5f,
                                           0.05f,
                                           1.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.safe_area",
                                           "Safe Area Padding",
                                           "Insets HUD elements to avoid overscan.",
                                           {"Video"},
                                           0.0f,
                                           0.1f,
                                           0.005f,
                                           0.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.render_scale",
                                           "Render Scale",
                                           "Scales the internal resolution relative to the target window.",
                                           {"Video"},
                                           0.5f,
                                           2.0f,
                                           0.05f,
                                           1.0f));
    schema.add_setting(make_toggle_setting(SettingScope::Install,
                                           "gubsy.video.show_fps",
                                           "FPS Counter",
                                           "Display the current frames-per-second.",
                                           {"Video", "Debug"},
                                           false));

    // Input (profile-specific)
    schema.add_setting(make_slider_setting(SettingScope::Profile,
                                           "gubsy.input.mouse_sensitivity",
                                           "Mouse Sensitivity",
                                           "Scales mouse look speed.",
                                           {"Controls"},
                                           0.1f,
                                           10.0f,
                                           0.1f,
                                           1.0f));
    schema.add_setting(make_slider_setting(SettingScope::Profile,
                                           "gubsy.input.controller_deadzone",
                                           "Controller Dead Zone",
                                           "Adjust the inner dead zone for analog sticks.",
                                           {"Controls"},
                                           0.0f,
                                           0.4f,
                                           0.01f,
                                           0.1f));
    schema.add_setting(make_slider_setting(SettingScope::Profile,
                                           "gubsy.input.vibration_strength",
                                           "Vibration Strength",
                                           "Scale controller rumble output.",
                                           {"Controls"},
                                           0.0f,
                                           1.0f,
                                           0.05f,
                                           0.8f));
    schema.add_setting(make_toggle_setting(SettingScope::Install,
                                           "gubsy.input.raw_mouse_input",
                                           "Raw Mouse Input",
                                           "Bypass OS-level mouse acceleration.",
                                           {"Controls"},
                                           true));
    schema.add_setting(make_toggle_setting(SettingScope::Profile,
                                           "gubsy.input.invert_y_axis",
                                           "Invert Y-Axis",
                                           "Flip vertical look direction.",
                                           {"Controls"},
                                           false));
    schema.add_setting(make_option_setting(SettingScope::Profile,
                                           "gubsy.input.controller_layout",
                                           "Controller Layout",
                                           "Select the default controller mapping.",
                                           {"Controls"},
                                           {SettingOption{"standard", "Standard"},
                                            SettingOption{"southpaw", "Southpaw"},
                                            SettingOption{"legacy", "Legacy"}},
                                           "standard"));

    // Accessibility
    schema.add_setting(make_toggle_setting(SettingScope::Profile,
                                           "gubsy.accessibility.subtitles_enabled",
                                           "Subtitles",
                                           "Enable in-game dialogue subtitles.",
                                           {"Accessibility", "Audio"},
                                           true));
    schema.add_setting(make_slider_setting(SettingScope::Profile,
                                           "gubsy.accessibility.subtitle_size",
                                           "Subtitle Size",
                                           "Adjusts subtitle text size.",
                                           {"Accessibility"},
                                           0.5f,
                                           2.0f,
                                           0.1f,
                                           1.0f));
    schema.add_setting(make_slider_setting(SettingScope::Profile,
                                           "gubsy.accessibility.subtitle_background",
                                           "Subtitle Background",
                                           "Opacity of the subtitle backdrop.",
                                           {"Accessibility"},
                                           0.0f,
                                           1.0f,
                                           0.05f,
                                           0.5f));
    schema.add_setting(make_option_setting(SettingScope::Profile,
                                           "gubsy.accessibility.colorblind_mode",
                                           "Colorblind Mode",
                                           "Applies color filters for improved readability.",
                                           {"Accessibility"},
                                           {SettingOption{"none", "None"},
                                            SettingOption{"deuteranopia", "Deuteranopia"},
                                            SettingOption{"protanopia", "Protanopia"},
                                            SettingOption{"tritanopia", "Tritanopia"}},
                                           "none"));

    register_settings_schema(schema);
}
