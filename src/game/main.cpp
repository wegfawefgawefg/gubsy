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
#include "engine/game_settings.hpp"
#include "game/ui_layout_ids.hpp"
#include "game/mod_api/register_game_mod_apis.hpp"
#include "engine/globals.hpp"
#include "engine/input_system.hpp"
#include "game/input_frame.hpp"
#include "game/menu/screens/main_menu_screen.hpp"
#include "game/menu/screens/lobby_screen.hpp"
#include "game/menu/screens/lobby_mods_screen.hpp"
#include "game/menu/screens/server_browser_screen.hpp"
#include "game/menu/screens/game_settings_screen.hpp"
#include "game/menu/screens/local_players_screen.hpp"
#include "game/menu/screens/player_settings_screen.hpp"
#include "game/settings_schema_registry.hpp"
#include "game/ui_layout_registry.hpp"
#include "game/binds_schema_registry.hpp"

namespace {
constexpr const char* kGameModVersion = "0.1.0";
}

void register_modes(){
    register_mode(modes::TITLE, title_step, title_process_inputs, title_draw);
    register_mode(modes::SETUP, setup_step, nullptr, setup_draw);
    register_mode(modes::PLAYING, playing_step, nullptr, playing_draw);
}

int main() {
    if (!init_engine_state()) {
        return 1;
    }
    if (!init_state()) {
        cleanup_engine_state();
        return 1;
    }
    register_input_frame_builder(build_input_frame);

    register_game_settings_schema_entries();
    register_game_ui_layouts();

    load_ui_layouts_pool();

    register_binds_schema_entries();

    register_game_mod_apis();
    set_required_mod_game_version(kGameModVersion);
    register_main_menu_screen();
    register_lobby_screen();
    register_lobby_mods_screen();
    register_server_browser_screen();
    register_game_settings_screen();
    register_local_players_screen();
    register_player_settings_screen();

    if (es)
        es->mode = modes::TITLE;

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
        bind_button(*binds_profile, GubsyButton::KB_SPACE, GameAction::MENU_SELECT);
        bind_button(*binds_profile, GubsyButton::KB_ESCAPE, GameAction::MENU_BACK);
        bind_button(*binds_profile, GubsyButton::KB_E, GameAction::MENU_PAGE_NEXT);
        bind_button(*binds_profile, GubsyButton::KB_Q, GameAction::MENU_PAGE_PREV);
        bind_button(*binds_profile, GubsyButton::KB_W, GameAction::MENU_UP);
        bind_button(*binds_profile, GubsyButton::KB_S, GameAction::MENU_DOWN);
        bind_button(*binds_profile, GubsyButton::KB_A, GameAction::MENU_LEFT);
        bind_button(*binds_profile, GubsyButton::KB_D, GameAction::MENU_RIGHT);

        // Set game action binds
        bind_button(*binds_profile, GubsyButton::KB_W, GameAction::UP);
        bind_button(*binds_profile, GubsyButton::KB_S, GameAction::DOWN);
        bind_button(*binds_profile, GubsyButton::KB_A, GameAction::LEFT);
        bind_button(*binds_profile, GubsyButton::KB_D, GameAction::RIGHT);
        bind_button(*binds_profile, GubsyButton::KB_E, GameAction::USE);
        bind_button(*binds_profile, GubsyButton::KB_SPACE, GameAction::USE);
        bind_button(*binds_profile, GubsyButton::GP_DPAD_UP, GameAction::MENU_UP);
        bind_button(*binds_profile, GubsyButton::GP_DPAD_DOWN, GameAction::MENU_DOWN);
        bind_button(*binds_profile, GubsyButton::GP_DPAD_LEFT, GameAction::MENU_LEFT);
        bind_button(*binds_profile, GubsyButton::GP_DPAD_RIGHT, GameAction::MENU_RIGHT);
        bind_button(*binds_profile, GubsyButton::GP_A, GameAction::MENU_SELECT);
        bind_button(*binds_profile, GubsyButton::GP_RIGHT_SHOULDER, GameAction::MENU_PAGE_NEXT);
        bind_button(*binds_profile, GubsyButton::GP_LEFT_SHOULDER, GameAction::MENU_PAGE_PREV);
        bind_button(*binds_profile, GubsyButton::GP_B, GameAction::MENU_BACK);

        // Set analog binds
        bind_1d_analog(*binds_profile, Gubsy1DAnalog::GP_LEFT_TRIGGER, GameAnalog1D::BAR_HEIGHT);
        bind_1d_analog(*binds_profile, Gubsy1DAnalog::MOUSE_WHEEL, GameAnalog1D::BAR_HEIGHT);
        bind_2d_analog(*binds_profile, Gubsy2DAnalog::GP_LEFT_STICK, GameAnalog2D::RETICLE_POS);
        bind_2d_analog(*binds_profile, Gubsy2DAnalog::MOUSE_XY, GameAnalog2D::RETICLE_POS);

        // Save the now-populated binds profile to disk
        save_binds_profile(*binds_profile);
    }

    register_modes();
   
    do_the_gubsy();

    cleanup_state();
    stop_doing_the_gubsy();
    return 0;
}
