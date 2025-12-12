// Temporary dispatcher that forwards to the legacy step implementation.
// This scaffolds the target architecture without changing behavior yet.

#include "step.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "engine/mode_registry.hpp"
#include "engine/alerts.hpp"
#include "crates.hpp"

// Forwarder: branch to mode-specific steps as they come online.
void step() {
    if (!ss) return;
    // Age alerts once per frame
    age_and_prune_alerts(ss->dt);

    // Fixed timestep catch-up. Apply to all modes for consistent timing.
    ss->time_since_last_update += static_cast<float>(ss->dt);
    while (ss->time_since_last_update > TIMESTEP) {
        ss->time_since_last_update -= TIMESTEP;
        if (ss->frame_pause > 0) { ss->frame_pause -= 1; return; }

        bool stepped = false;
        if (const ModeDesc* mode = find_mode(ss->mode)) {
            if (mode->step_fn) {
                mode->step_fn();
                stepped = true;
            }
        }
        if (!stepped) {
            if (ss->mode == modes::PLAYING) step_playing();
            else if (ss->mode == modes::TITLE) step_title();
            else if (ss->mode == modes::SCORE_REVIEW) step_score_review();
            else if (ss->mode == modes::NEXT_STAGE) step_next_stage();
            else if (ss->mode == modes::GAME_OVER) step_game_over();
        }

        ss->scene_frame = ss->scene_frame + 1u;
    }

    // Per-frame updates that should not run inside the fixed-step loop
    update_crates_open();
}
