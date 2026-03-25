#pragma once

struct EngineState;

using FixedStepPrepFn = void (*)(EngineState& engine, void* app_context);

void register_fixed_step_prep(FixedStepPrepFn fn);
void step(EngineState& engine);
