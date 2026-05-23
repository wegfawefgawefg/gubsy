#pragma once

#include "src/engine_state.hpp"
#include "gubsy/app.hpp"

bool do_the_gubsy(EngineState& engine, const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy(EngineState& engine);
