#pragma once

#include <string>
#include <vector>

#include "engine/settings_types.hpp"

enum class SettingScope {
    Install,
    Profile
};

enum class SettingWidgetKind {
    Toggle,
    Slider,
    Option,
    Text,
    Button,
    Custom
};

struct SettingOption {
    std::string value;
    std::string label;
};

struct SettingWidgetDesc {
    SettingWidgetKind kind = SettingWidgetKind::Toggle;
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.0f;
    int max_text_len = 0;
    float display_scale = 1.0f;
    float display_offset = 0.0f;
    int display_precision = 3;
    std::vector<SettingOption> options;
    std::string custom_id;
};

struct SettingMetadata {
    SettingScope scope = SettingScope::Profile;
    std::string key;
    std::string label;
    std::string description;
    std::vector<std::string> categories;
    int order = 0;
    SettingWidgetDesc widget;
    SettingsValue default_value{};
};

class SettingsSchema {
public:
    SettingMetadata& add_setting(SettingMetadata meta);
    std::vector<SettingMetadata>& entries() { return entries_; }
    const std::vector<SettingMetadata>& entries() const { return entries_; }

private:
    std::vector<SettingMetadata> entries_;
};

const SettingsSchema& get_settings_schema();
void register_settings_schema(const SettingsSchema& schema);
