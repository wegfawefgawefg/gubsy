#pragma once

#include "engine/graphics.hpp"
#include "engine/audio.hpp"
#include "engine/engine_state.hpp"
#include "engine/mods.hpp"
#include "engine/engine_state.hpp"
#include "state.hpp"

// Shared pointers to systems used across modules.
extern EngineState* es;
extern State* ss;
extern Graphics* gg;
extern Audio* aa;
extern ModManager* mm;
