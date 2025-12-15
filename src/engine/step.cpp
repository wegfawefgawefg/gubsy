#include "step.hpp"

#include "engine/alerts.hpp"
#include "engine/mode_registry.hpp"
#include "globals.hpp"
#include "modes.hpp"
#include "settings.hpp"

void step() {
    age_and_prune_alerts(ss->dt);

    const float fixed_dt = FIXED_TIMESTEP;
    ss->accumulator += ss->dt;

    while (ss->accumulator >= fixed_dt) {
        ss->accumulator -= fixed_dt;
        ss->now += static_cast<double>(fixed_dt);

        if (const ModeDesc* mode = find_mode(ss->mode)) {
            if (mode->step_fn) {
                mode->step_fn();
            }
        }

        ss->frame += 1;
    }
}
