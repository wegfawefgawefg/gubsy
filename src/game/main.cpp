#include "engine/run.hpp"
#include "game/title.hpp"
#include "game/playing.hpp"
#include "game/setup.hpp"
#include "engine/ui_layouts.hpp"
#include "engine/mod_host.hpp"

#include "engine/player.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/input.hpp"
#include "game/actions.hpp"
#include "engine/top_level_game_settings.hpp"
#include "engine/game_settings.hpp"
#include "game/ui_layout_ids.hpp"
#include "game/mod_api/register_game_mod_apis.hpp"
#include "engine/globals.hpp"

namespace {
constexpr const char* kGameModVersion = "0.1.0";
}

void register_modes(){
    register_mode(modes::SETUP, setup_step, nullptr, setup_draw);
    register_mode(modes::PLAYING, playing_step, nullptr, playing_draw);
}

int main() {
    if (!init_state()) {
        return 1;
    }

    // Define global game settings schema (singleton, per-install)
    TopLevelGameSettingsSchema global_schema;
    global_schema.add_string("language", "en");
    global_schema.add_float("master_volume", 1.0f);
    global_schema.add_int("screen_width", 1920);
    global_schema.add_int("screen_height", 1080);
    global_schema.add_int("fullscreen", 0);
    register_global_settings_schema(global_schema);

    // Define per-user game settings schema
    GameSettingsSchema game_settings_schema;
    game_settings_schema.add_float("difficulty", 1.0f);
    game_settings_schema.add_int("lives", 3);
    game_settings_schema.add_string("character_name", "Player");
    game_settings_schema.add_vec2("spawn_point", 0.0f, 0.0f);
    register_game_settings_schema(game_settings_schema);

    // Define play screen layouts for several resolutions
    {
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

    load_ui_layouts_pool();

    // Define binds schema (singleton, action metadata)
    BindsSchema binds_schema;
    binds_schema.add_action(GameAction::MENU_UP, "Move Up", "Menu");
    binds_schema.add_action(GameAction::MENU_DOWN, "Move Down", "Menu");
    binds_schema.add_action(GameAction::MENU_LEFT, "Move Left", "Menu");
    binds_schema.add_action(GameAction::MENU_RIGHT, "Move Right", "Menu");
    binds_schema.add_action(GameAction::MENU_SELECT, "Select", "Menu");
    binds_schema.add_action(GameAction::UP, "Move Up", "Movement");
    binds_schema.add_action(GameAction::DOWN, "Move Down", "Movement");
    binds_schema.add_action(GameAction::LEFT, "Move Left", "Movement");
    binds_schema.add_action(GameAction::RIGHT, "Move Right", "Movement");
    binds_schema.add_action(GameAction::USE, "Use/Interact", "Actions");
    binds_schema.add_1d_analog(GameAnalog1D::BAR_HEIGHT, "Bar Height", "Demo");
    binds_schema.add_2d_analog(GameAnalog2D::RETICLE_POS, "Reticle Position", "Demo");
    register_binds_schema(binds_schema);

    register_game_mod_apis();
    set_required_mod_game_version(kGameModVersion);

    if (es)
        es->mode = modes::SETUP;

    add_player(0);

    BindsProfile* binds_profile = get_player_binds_profile(0);
    if (binds_profile) {
        // Profile is empty, so populate it with defaults
        // Set menu binds
        bind_button(*binds_profile, GubsyButton::KB_UP, GameAction::MENU_UP);
        bind_button(*binds_profile, GubsyButton::KB_DOWN, GameAction::MENU_DOWN);
        bind_button(*binds_profile, GubsyButton::KB_LEFT, GameAction::MENU_LEFT);
        bind_button(*binds_profile, GubsyButton::KB_RIGHT, GameAction::MENU_RIGHT);
        bind_button(*binds_profile, GubsyButton::KB_ENTER, GameAction::MENU_SELECT);

        // Set game action binds
        bind_button(*binds_profile, GubsyButton::KB_W, GameAction::UP);
        bind_button(*binds_profile, GubsyButton::KB_S, GameAction::DOWN);
        bind_button(*binds_profile, GubsyButton::KB_A, GameAction::LEFT);
        bind_button(*binds_profile, GubsyButton::KB_D, GameAction::RIGHT);
        bind_button(*binds_profile, GubsyButton::KB_E, GameAction::USE);
        bind_button(*binds_profile, GubsyButton::KB_SPACE, GameAction::USE);

        // Set analog binds
        bind_1d_analog(*binds_profile, Gubsy1DAnalog::GP_LEFT_TRIGGER, GameAnalog1D::BAR_HEIGHT);
        bind_2d_analog(*binds_profile, Gubsy2DAnalog::GP_LEFT_STICK, GameAnalog2D::RETICLE_POS);

        // Save the now-populated binds profile to disk
        save_binds_profile(*binds_profile);
    }

    register_modes();
   
    do_the_gubsy();

    cleanup_state();
    stop_doing_the_gubsy();
    return 0;
}
