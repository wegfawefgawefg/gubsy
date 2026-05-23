#pragma once

#include "gubsy/app.hpp"
#include "gubsy/engine_state.hpp"

bool do_the_gubsy(EngineState& engine, const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy(EngineState& engine);
