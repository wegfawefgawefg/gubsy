#include "engine/run.hpp"
#include "game/title.hpp"
#include "game/playing.hpp"
#include "demo_content.hpp"

void register_modes(){
    // register_mode(modes::TITLE, title_step, title_process_inputs, title_draw);
    register_mode(modes::PLAYING, playing_step, nullptr, playing_draw);
}



int main() {
    if (!init_state()) {
        return 1;
    }
    load_demo_content();

    register_modes();
   
    do_the_gubsy();

    cleanup_state();
    stop_doing_the_gubsy();
    return 0;
}
