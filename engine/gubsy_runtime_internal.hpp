#pragma once

#include "engine/engine_state.hpp"
#include "gubsy/runtime.hpp"

EngineState& gubsy_runtime_engine(GubsyRuntime& runtime);
const EngineState& gubsy_runtime_engine(const GubsyRuntime& runtime);
