#include "engine/run.hpp"
#include "game/title.hpp"
#include "game/playing.hpp"
#include "demo_content.hpp"

#include "engine/player.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/input.hpp"
#include "game/actions.hpp"

void register_modes(){
    // register_mode(modes::TITLE, title_step, title_process_inputs, title_draw);
    register_mode(modes::PLAYING, playing_step, nullptr, playing_draw);
}

int main() {
    if (!init_state()) {
        return 1;
    }
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

        // Save the now-populated binds profile to disk
        save_binds_profile(*binds_profile);
    }

    register_modes();
   
    do_the_gubsy();

    cleanup_state();
    stop_doing_the_gubsy();
    return 0;
}
