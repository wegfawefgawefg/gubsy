#include "engine/run.hpp"
#include "game/title.hpp"
#include "game/playing.hpp"
#include "demo_content.hpp"

int main() {
    if (!init_state()) {
        return 1;
    }
    load_demo_content();

    register_mode(modes::TITLE, nullptr, nullptr, draw_title);
    register_mode(modes::PLAYING, step_playing, nullptr, render_playing);
    
    do_the_gubsy();

    cleanup_state();
    stop_doing_the_gubsy();
    return 0;
}
