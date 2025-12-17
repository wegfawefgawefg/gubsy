#include "engine/run.hpp"
#include "game/title.hpp"
#include "game/playing.hpp"
#include "demo_content.hpp"

#include "engine/player.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/input.hpp"
#include "game/actions.hpp"
#include "engine/top_level_game_settings.hpp"
#include "engine/game_settings.hpp"

void register_modes(){
    // register_mode(modes::TITLE, title_step, title_process_inputs, title_draw);
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

    load_demo_content();

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
