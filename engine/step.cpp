#include "step.hpp"

#include "engine/alerts.hpp"
#include "engine/mode_registry.hpp"
#include "engine/runtime_settings.hpp"
#include "globals.hpp"

namespace {

FixedStepPrepFn g_fixed_step_prep{nullptr};

} // namespace

void register_fixed_step_prep(FixedStepPrepFn fn) {
    g_fixed_step_prep = fn;
}

void step() {
    age_and_prune_alerts(es->dt);

    const float fixed_dt = FIXED_TIMESTEP;
    es->accumulator += es->dt;

    while (es->accumulator >= fixed_dt) {
        es->accumulator -= fixed_dt;
        es->now += static_cast<double>(fixed_dt);

        if (g_fixed_step_prep)
            g_fixed_step_prep(es ? es->app_context : nullptr);

        if (const ModeDesc* mode = find_mode(es->mode)) {
            if (mode->step_fn)
                mode->step_fn(es ? es->app_context : nullptr);
        }

        es->frame += 1;
    }
}
