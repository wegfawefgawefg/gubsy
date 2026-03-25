#pragma once

using FixedStepPrepFn = void (*)(void* app_context);

void register_fixed_step_prep(FixedStepPrepFn fn);
void step();
