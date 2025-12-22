#include "game/ui_layout_registry.hpp"

#include "engine/ui_layouts.hpp"
#include "game/ui_layout_ids.hpp"

void register_game_ui_layouts() {
    UILayout play_1080p = create_ui_layout(UILayoutID::PLAY_SCREEN, "PlayScreen", 1920, 1080);
    play_1080p.add_object(UIObjectID::HEALTHBAR, "healthbar", 0.02f, 0.02f, 0.15f, 0.03f);
    play_1080p.add_object(UIObjectID::INVENTORY, "inventory", 0.85f, 0.02f, 0.13f, 0.20f);
    play_1080p.add_object(UIObjectID::MINIMAP, "minimap", 0.85f, 0.75f, 0.13f, 0.20f);
    play_1080p.add_object(UIObjectID::BAR_HEIGHT_INDICATOR, "bar", 0.01f, 0.08f, 0.016f, 0.60f);
    save_ui_layout(play_1080p);

    UILayout play_720p = create_ui_layout(UILayoutID::PLAY_SCREEN, "PlayScreen", 1280, 720);
    play_720p.add_object(UIObjectID::HEALTHBAR, "healthbar", 0.02f, 0.03f, 0.18f, 0.04f);
    play_720p.add_object(UIObjectID::INVENTORY, "inventory", 0.82f, 0.02f, 0.16f, 0.22f);
    play_720p.add_object(UIObjectID::MINIMAP, "minimap", 0.82f, 0.73f, 0.16f, 0.22f);
    play_720p.add_object(UIObjectID::BAR_HEIGHT_INDICATOR, "bar", 0.01f, 0.11f, 0.023f, 0.60f);
    save_ui_layout(play_720p);

    UILayout play_ultrawide = create_ui_layout(UILayoutID::PLAY_SCREEN, "PlayScreen", 2560, 1080);
    play_ultrawide.add_object(UIObjectID::HEALTHBAR, "healthbar", 0.01f, 0.02f, 0.12f, 0.03f);
    play_ultrawide.add_object(UIObjectID::INVENTORY, "inventory", 0.88f, 0.02f, 0.11f, 0.20f);
    play_ultrawide.add_object(UIObjectID::MINIMAP, "minimap", 0.88f, 0.75f, 0.11f, 0.20f);
    play_ultrawide.add_object(UIObjectID::BAR_HEIGHT_INDICATOR, "bar", 0.005f, 0.08f, 0.012f, 0.60f);
    save_ui_layout(play_ultrawide);
}
