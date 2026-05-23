#pragma once

#include "SDL.h"

struct Mix_Chunk;

#ifndef MIX_MAX_VOLUME
#define MIX_MAX_VOLUME 128
#endif

inline const char* Mix_GetError() {
    return SDL_GetError();
}
