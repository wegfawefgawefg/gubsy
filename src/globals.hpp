#pragma once

#include "luamgr.hpp"
#include "sprites.hpp"
#include "engine/graphics.hpp"
#include "engine/audio.hpp"
#include "mods.hpp"
#include "state.hpp"
#include "engine/input.hpp"
#include "runtime_settings.hpp"

// Shared pointers to systems used across modules.
extern State* ss;
extern Graphics* gg;
extern Audio* aa;

extern ModManager* mm;
extern LuaManager* luam;
