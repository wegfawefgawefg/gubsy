#include "game_modes.hpp"

#include "engine/mode_registry.hpp"
#include "render.hpp"
#include "step.hpp"
#include "modes.hpp"

void register_game_modes() {
    register_mode(modes::TITLE, step_title, render_mode_title);
    register_mode(modes::PLAYING, step_playing, render_mode_playing);
    register_mode(modes::SCORE_REVIEW, step_score_review, render_mode_score_review);
    register_mode(modes::NEXT_STAGE, step_next_stage, render_mode_next_stage);
    register_mode(modes::GAME_OVER, step_game_over, render_mode_game_over);
}
