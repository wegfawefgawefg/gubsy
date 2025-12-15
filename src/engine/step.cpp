#include "step.hpp"

#include "engine/alerts.hpp"
#include "engine/mode_registry.hpp"
#include "globals.hpp"

void step() {
    age_and_prune_alerts(es->dt);

    const float fixed_dt = FIXED_TIMESTEP;
    es->accumulator += es->dt;

    while (es->accumulator >= fixed_dt) {
        es->accumulator -= fixed_dt;
        es->now += static_cast<double>(fixed_dt);

        if (const ModeDesc* mode = find_mode(es->mode)) {
            if (mode->step_fn) {
                mode->step_fn();
            }
        }

        es->frame += 1;
    }
}
