#pragma once

#include "engine/graphics.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Renders a full frame, including world and UI. Safe to call with null renderer
// (falls back to a short sleep to avoid busy-wait). Uses gfx.renderer.
void render();

// Mode-specific render entry points (registered with mode registry).
void render_mode_title();
void render_mode_playing();
void render_mode_score_review();
void render_mode_next_stage();
void render_mode_game_over();
