#include "game/settings_schema_registry.hpp"

#include "engine/settings_schema.hpp"

namespace {



SettingMetadata make_difficulty_setting() {
    SettingMetadata meta{};
    meta.scope = SettingScope::Profile;
    meta.key = "gameplay.difficulty";
    meta.label = "Difficulty";
    meta.description = "Demo gameplay difficulty scalar.";
    meta.categories = {"Gameplay"};
    meta.widget.kind = SettingWidgetKind::Slider;
    meta.widget.min = 0.0f;
    meta.widget.max = 2.0f;
    meta.widget.step = 0.5f;
    meta.default_value = 1.0f;
    return meta;
}

SettingMetadata make_character_name_setting() {
    SettingMetadata meta{};
    meta.scope = SettingScope::Profile;
    meta.key = "gameplay.character_name";
    meta.label = "Character Name";
    meta.description = "Displayed on certain HUD elements.";
    meta.categories = {"Gameplay"};
    meta.widget.kind = SettingWidgetKind::Text;
    meta.widget.max_text_len = 32;
    meta.default_value = std::string("Player");
    return meta;
}

} // namespace

void register_game_settings_schema_entries() {
    SettingsSchema schema;
    schema.add_setting(make_difficulty_setting());
    schema.add_setting(make_character_name_setting());
    register_settings_schema(schema);
}
