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
    {
        SettingMetadata meta = make_slider_setting(SettingScope::Install,
                                                   "gubsy.audio.music_volume",
                                                   "Music Volume",
                                                   "Controls the music mix level.",
                                                   {"Audio"},
                                                   0.0f,
                                                   1.0f,
                                                   0.01f,
                                                   0.8f);
        meta.order = -100;
        schema.add_setting(meta);
    }
    {
        SettingMetadata meta = make_slider_setting(SettingScope::Install,
                                                   "gubsy.audio.sfx_volume",
                                                   "SFX Volume",
                                                   "Controls the volume for sound effects.",
                                                   {"Audio"},
                                                   0.0f,
                                                   1.0f,
                                                   0.01f,
                                                   1.0f);
        meta.order = -99;
        schema.add_setting(meta);
    }
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

    // Video / Display
    schema.add_setting(make_option_setting(
        SettingScope::Install,
        "gubsy.video.render_resolution",
        "Render Resolution",
        "Target internal render resolution.",
        {"Video"},
        {SettingOption{"3840x2160", "4K UHD (16:9)"},
         SettingOption{"3840x1600", "Ultra-wide 24:10"},
         SettingOption{"3440x1440", "UWQHD 21:9"},
         SettingOption{"2560x1440", "1440p QHD"},
         SettingOption{"2560x1080", "Ultra-wide 21:9"},
         SettingOption{"2560x1600", "WQXGA 16:10"},
         SettingOption{"1920x1200", "WUXGA 16:10"},
         SettingOption{"1920x1080", "1080p Full HD"},
         SettingOption{"1600x900", "900p HD+"},
         SettingOption{"1536x2048", "Tablet 4:3"},
         SettingOption{"1440x900", "WXGA+ 16:10"},
         SettingOption{"1366x768", "Laptop 16:9"},
         SettingOption{"1280x800", "WXGA 16:10"},
         SettingOption{"1280x720", "720p HD"},
         SettingOption{"1080x2400", "Mobile 20:9"},
         SettingOption{"1080x1920", "Mobile FHD+"},
         SettingOption{"1024x768", "XGA 4:3"}},
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
                                           "gubsy.video.safe_area_left",
                                           "Safe Area Left",
                                           "Inset from the left edge to avoid overscan.",
                                           {"Video"},
                                           0.0f,
                                           0.2f,
                                           0.005f,
                                           0.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.safe_area_right",
                                           "Safe Area Right",
                                           "Inset from the right edge to avoid overscan.",
                                           {"Video"},
                                           0.0f,
                                           0.2f,
                                           0.005f,
                                           0.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.safe_area_top",
                                           "Safe Area Top",
                                           "Inset from the top edge to avoid overscan.",
                                           {"Video"},
                                           0.0f,
                                           0.2f,
                                           0.005f,
                                           0.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.safe_area_bottom",
                                           "Safe Area Bottom",
                                           "Inset from the bottom edge to avoid overscan.",
                                           {"Video"},
                                           0.0f,
                                           0.2f,
                                           0.005f,
                                           0.0f));
    schema.add_setting(make_toggle_setting(SettingScope::Install,
                                           "gubsy.video.show_fps",
                                           "FPS Counter",
                                           "Display the current frames-per-second.",
                                           {"Video", "Debug"},
                                           false));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.preview_zoom",
                                           "Zoom",
                                           "Preview zoom adjustment for debugging.",
                                           {"Video", "Debug"},
                                           0.5f,
                                           3.0f,
                                           0.01f,
                                           1.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.preview_pan_x",
                                           "Pan X",
                                           "Preview pan X offset in pixels for debugging.",
                                           {"Video", "Debug"},
                                           -1000.0f,
                                           1000.0f,
                                           1.0f,
                                           0.0f));
    schema.add_setting(make_slider_setting(SettingScope::Install,
                                           "gubsy.video.preview_pan_y",
                                           "Pan Y",
                                           "Preview pan Y offset in pixels for debugging.",
                                           {"Video", "Debug"},
                                           -1000.0f,
                                           1000.0f,
                                           1.0f,
                                           0.0f));

    // Input (profile-specific)
    {
        SettingMetadata meta = make_slider_setting(SettingScope::Profile,
                                                   "gubsy.input.mouse_sensitivity",
                                                   "Mouse Sensitivity",
                                                   "Scales mouse look speed.",
                                                   {"Controls"},
                                                   0.1f,
                                                   10.0f,
                                                   0.1f,
                                                   1.0f);
        meta.order = -100;
        schema.add_setting(meta);
    }
    schema.add_setting(make_slider_setting(SettingScope::Profile,
                                           "gubsy.input.vibration_strength",
                                           "Vibration Strength",
                                           "Scale controller rumble output.",
                                           {"Controls"},
                                           0.0f,
                                           1.0f,
                                           0.05f,
                                           0.8f));
    schema.add_setting(make_toggle_setting(SettingScope::Profile,
                                           "gubsy.input.invert_y_axis",
                                           "Invert Y-Axis",
                                           "Flip vertical look direction.",
                                           {"Controls"},
                                           false));


    register_settings_schema(schema);
}
