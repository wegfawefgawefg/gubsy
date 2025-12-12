#pragma once

#include "engine/graphics.hpp"
#include "engine/audio.hpp"
#include "mods.hpp"
#include "state.hpp"

// Shared pointers to systems used across modules.
extern State* ss;
extern Graphics* gg;
extern Audio* aa;
extern ModManager* mm;
