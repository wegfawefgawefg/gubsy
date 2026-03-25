#pragma once

#include "engine/graphics.hpp"
#include "engine/audio.hpp"
#include "engine/engine_state.hpp"
#include "engine/mods.hpp"

// Shared pointers to systems used across modules.
extern EngineState* es;
extern Graphics* gg;
extern Audio* aa;
extern ModManager* mm;
