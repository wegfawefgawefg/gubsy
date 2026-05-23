#pragma once

#include "gubsy/app.hpp"

struct EngineState;

bool do_the_gubsy(EngineState& engine, const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy(EngineState& engine);
