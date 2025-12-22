#include "engine/settings_defaults.hpp"

#include "engine/settings_schema.hpp"

namespace {

SettingMetadata make_master_volume_setting() {
    SettingMetadata meta{};
    meta.scope = SettingScope::Install;
    meta.key = "gubsy.audio.master_volume";
    meta.label = "Master Volume";
    meta.description = "Controls the overall output volume for engine-managed audio.";
    meta.categories = {"Audio"};
    meta.widget.kind = SettingWidgetKind::Slider;
    meta.widget.min = 0.0f;
    meta.widget.max = 1.0f;
    meta.widget.step = 0.01f;
    meta.default_value = 1.0f;
    return meta;
}

SettingMetadata make_sfx_volume_setting() {
    SettingMetadata meta{};
    meta.scope = SettingScope::Install;
    meta.key = "gubsy.audio.sfx_volume";
    meta.label = "SFX Volume";
    meta.description = "Controls the volume for sound effects.";
    meta.categories = {"Audio"};
    meta.widget.kind = SettingWidgetKind::Slider;
    meta.widget.min = 0.0f;
    meta.widget.max = 1.0f;
    meta.widget.step = 0.01f;
    meta.default_value = 1.0f;
    return meta;
}
SettingMetadata make_language_setting() {
    SettingMetadata meta{};
    meta.scope = SettingScope::Install;
    meta.key = "language";
    meta.label = "Language";
    meta.description = "Sets the UI language.";
    meta.categories = {"Profiles"};
    meta.widget.kind = SettingWidgetKind::Option;
    meta.widget.options = {SettingOption{"en", "English"}};
    meta.default_value = std::string("en");
    return meta;
}

} // namespace


void register_engine_settings_schema_entries() {
    SettingsSchema schema;
    schema.add_setting(make_master_volume_setting());
    schema.add_setting(make_sfx_volume_setting());
    schema.add_setting(make_language_setting());
    register_settings_schema(schema);
}

