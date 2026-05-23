#include "game/ui_layout_registry.hpp"

#include "engine/ui_layouts.hpp"
#include "game/ui_layout_ids.hpp"

void register_game_ui_layouts() {
    UILayout play_1080p = create_ui_layout(UILayoutID::PLAY_SCREEN, "PlayScreen", 1920, 1080);
    add_ui_object(play_1080p, UIObjectID::HEALTHBAR, "healthbar", 0.02f, 0.02f, 0.15f, 0.03f);
    add_ui_object(play_1080p, UIObjectID::INVENTORY, "inventory", 0.85f, 0.02f, 0.13f, 0.20f);
    add_ui_object(play_1080p, UIObjectID::MINIMAP, "minimap", 0.85f, 0.75f, 0.13f, 0.20f);
    add_ui_object(play_1080p, UIObjectID::BAR_HEIGHT_INDICATOR, "bar", 0.01f, 0.08f, 0.016f, 0.60f);
    save_ui_layout(play_1080p);

    UILayout play_720p = create_ui_layout(UILayoutID::PLAY_SCREEN, "PlayScreen", 1280, 720);
    add_ui_object(play_720p, UIObjectID::HEALTHBAR, "healthbar", 0.02f, 0.03f, 0.18f, 0.04f);
    add_ui_object(play_720p, UIObjectID::INVENTORY, "inventory", 0.82f, 0.02f, 0.16f, 0.22f);
    add_ui_object(play_720p, UIObjectID::MINIMAP, "minimap", 0.82f, 0.73f, 0.16f, 0.22f);
    add_ui_object(play_720p, UIObjectID::BAR_HEIGHT_INDICATOR, "bar", 0.01f, 0.11f, 0.023f, 0.60f);
    save_ui_layout(play_720p);

    UILayout play_ultrawide = create_ui_layout(UILayoutID::PLAY_SCREEN, "PlayScreen", 2560, 1080);
    add_ui_object(play_ultrawide, UIObjectID::HEALTHBAR, "healthbar", 0.01f, 0.02f, 0.12f, 0.03f);
    add_ui_object(play_ultrawide, UIObjectID::INVENTORY, "inventory", 0.88f, 0.02f, 0.11f, 0.20f);
    add_ui_object(play_ultrawide, UIObjectID::MINIMAP, "minimap", 0.88f, 0.75f, 0.11f, 0.20f);
    add_ui_object(play_ultrawide, UIObjectID::BAR_HEIGHT_INDICATOR, "bar", 0.005f, 0.08f, 0.012f,
                  0.60f);
    save_ui_layout(play_ultrawide);

    UILayout settings_1080p =
        create_ui_layout(UILayoutID::GAME_SETTINGS_SCREEN, "GameSettings", 1920, 1080);
    add_ui_object(settings_1080p, GameSettingsObjectID::TITLE, "title", 0.08f, 0.08f, 0.60f, 0.07f);
    add_ui_object(settings_1080p, GameSettingsObjectID::STATUS, "status", 0.08f, 0.16f, 0.78f,
                  0.06f);
    add_ui_object(settings_1080p, GameSettingsObjectID::CARD0, "scenario", 0.08f, 0.28f, 0.46f,
                  0.09f);
    add_ui_object(settings_1080p, GameSettingsObjectID::CARD1, "seed_mode", 0.08f, 0.40f, 0.46f,
                  0.09f);
    add_ui_object(settings_1080p, GameSettingsObjectID::CARD2, "seed_text", 0.08f, 0.52f, 0.46f,
                  0.09f);
    add_ui_object(settings_1080p, GameSettingsObjectID::CARD3, "phase", 0.08f, 0.64f, 0.46f, 0.09f);
    add_ui_object(settings_1080p, GameSettingsObjectID::BACK, "back", 0.08f, 0.80f, 0.22f, 0.08f);
    save_ui_layout(settings_1080p);

    UILayout settings_720p =
        create_ui_layout(UILayoutID::GAME_SETTINGS_SCREEN, "GameSettings", 1280, 720);
    add_ui_object(settings_720p, GameSettingsObjectID::TITLE, "title", 0.08f, 0.07f, 0.64f, 0.08f);
    add_ui_object(settings_720p, GameSettingsObjectID::STATUS, "status", 0.08f, 0.17f, 0.80f,
                  0.07f);
    add_ui_object(settings_720p, GameSettingsObjectID::CARD0, "scenario", 0.08f, 0.29f, 0.58f,
                  0.10f);
    add_ui_object(settings_720p, GameSettingsObjectID::CARD1, "seed_mode", 0.08f, 0.42f, 0.58f,
                  0.10f);
    add_ui_object(settings_720p, GameSettingsObjectID::CARD2, "seed_text", 0.08f, 0.55f, 0.58f,
                  0.10f);
    add_ui_object(settings_720p, GameSettingsObjectID::CARD3, "phase", 0.08f, 0.68f, 0.58f, 0.10f);
    add_ui_object(settings_720p, GameSettingsObjectID::BACK, "back", 0.08f, 0.83f, 0.26f, 0.09f);
    save_ui_layout(settings_720p);
}
