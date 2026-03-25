#include "step.hpp"

#include "engine/alerts.hpp"
#include "engine/engine_state.hpp"
#include "engine/mode_registry.hpp"
#include "engine/runtime_settings.hpp"

namespace {

FixedStepPrepFn g_fixed_step_prep{nullptr};

} // namespace

void register_fixed_step_prep(FixedStepPrepFn fn) {
    g_fixed_step_prep = fn;
}

void step(EngineState& engine) {
    age_and_prune_alerts(engine, engine.dt);

    const float fixed_dt = FIXED_TIMESTEP;
    engine.accumulator += engine.dt;

    while (engine.accumulator >= fixed_dt) {
        engine.accumulator -= fixed_dt;
        engine.now += static_cast<double>(fixed_dt);

        if (g_fixed_step_prep)
            g_fixed_step_prep(engine, engine.app_context);

        if (const ModeDesc* mode = find_mode(engine, engine.mode)) {
            if (mode->step_fn)
                mode->step_fn(engine, engine.app_context);
        }

        engine.frame += 1;
    }
}
