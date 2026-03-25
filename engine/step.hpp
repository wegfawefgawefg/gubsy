#pragma once

using FixedStepPrepFn = void (*)();

void register_fixed_step_prep(FixedStepPrepFn fn);
void step();
