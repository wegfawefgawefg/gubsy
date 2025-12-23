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
    {
        SettingMetadata meta = make_slider_setting(SettingScope::Install,
                                                   "gubsy.audio.master_volume",
                                                   "Master Volume",
                                                   "Controls the overall output volume for engine-managed audio.",
                                                   {"Audio"},
                                                   0.0f,
                                                   1.0f,
                                                   0.01f,
                                                   1.0f);
        meta.order = -100;
        schema.add_setting(meta);
    }
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
        meta.order = -99;
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
        meta.order = -98;
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
        {// 16:9 Desktop / TV
         SettingOption{"1280x720", "1280x720 (720p)"},
         SettingOption{"1366x768", "1366x768 (16:9)"},
         SettingOption{"1920x1080", "1920x1080 (1080p)"},
         SettingOption{"2560x1440", "2560x1440 (1440p)"},
         SettingOption{"3840x2160", "3840x2160 (4K)"},
         SettingOption{"7680x4320", "7680x4320 (8K)"},
         // 16:10 Laptops
         SettingOption{"1280x800", "1280x800 (16:10)"},
         SettingOption{"1440x900", "1440x900 (16:10)"},
         SettingOption{"1680x1050", "1680x1050 (16:10)"},
         SettingOption{"1920x1200", "1920x1200 (WUXGA)"},
         SettingOption{"2560x1600", "2560x1600 (WQXGA)"},
         SettingOption{"2880x1800", "2880x1800 (Retina)"},
         // Ultrawide 21:9
         SettingOption{"2560x1080", "2560x1080 (21:9)"},
         SettingOption{"3440x1440", "3440x1440 (UWQHD)"},
         SettingOption{"5120x2160", "5120x2160 (5K2K)"},
         // Super Ultrawide 32:9
         SettingOption{"3840x1080", "3840x1080 (32:9)"},
         SettingOption{"5120x1440", "5120x1440 (DQHD)"},
         // Console & Retro Landscape
         SettingOption{"1280x720", "1280x720 (Switch HD)"},
         SettingOption{"640x480", "640x480 (SD Consoles)"},
         SettingOption{"480x272", "480x272 (PSP)"},
         SettingOption{"480x270", "480x270 (PSP Crop)"},
         SettingOption{"320x240", "320x240 (N64/PS1)"},
         SettingOption{"256x192", "256x192 (Nintendo DS)"},
         SettingOption{"240x160", "240x160 (GBA)"},
         SettingOption{"160x144", "160x144 (Game Boy)"},
         // Phone Portrait
         SettingOption{"720x1280", "720x1280 (HD Phone)"},
         SettingOption{"1080x1920", "1080x1920 (FHD Phone)"},
         SettingOption{"1080x2340", "1080x2340 (19.5:9)"},
         SettingOption{"1080x2400", "1080x2400 (20:9)"},
         SettingOption{"1170x2532", "1170x2532 (iPhone 12/13)"},
         SettingOption{"1179x2556", "1179x2556 (iPhone 14/15)"},
         SettingOption{"1440x3200", "1440x3200 (QHD+ Phone)"},
         // Tablet Portrait
         SettingOption{"1536x2048", "1536x2048 (iPad)"},
         SettingOption{"1668x2388", "1668x2388 (iPad Pro 11\")"},
         SettingOption{"2048x2732", "2048x2732 (iPad Pro 12.9\")"},
         SettingOption{"1600x2560", "1600x2560 (Android Tablet)"}},
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
    schema.add_setting(make_option_setting(SettingScope::Install,
                                           "gubsy.video.render_scale_mode",
                                           "Render Scale",
                                           "How to scale render resolution to window.",
                                           {"Video"},
                                           {SettingOption{"fit", "Fit (letterbox)"},
                                            SettingOption{"stretch", "Stretch"}},
                                           "fit"));
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
