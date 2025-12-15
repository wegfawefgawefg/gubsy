#include "game_modes.hpp"

#include "engine/mode_registry.hpp"
#include "render.hpp"
#include "step.hpp"
#include "modes.hpp"

void register_game_modes() {
    register_mode(modes::TITLE, step_title, process_inputs_title, render_mode_title);
    register_mode(modes::PLAYING, step_playing, process_inputs_playing, render_mode_playing);
}
