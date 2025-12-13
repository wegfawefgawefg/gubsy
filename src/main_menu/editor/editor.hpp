#pragma once

#include "main_menu/menu.hpp"

#include <SDL2/SDL.h>
#include <vector>

struct ButtonDesc;

// Handle Ctrl+E / Ctrl+L / Ctrl+N shortcuts before normal menu logic.
void gui_editor_handle_shortcuts(Page current_page);

// Returns true if the editor is active and should consume menu input this frame.
bool gui_editor_consumes_input();

// Step the editor while it owns input.
void gui_editor_step(const std::vector<ButtonDesc>& buttons, int width, int height);

// Render overlay text/status while the editor is active.
void gui_editor_render(SDL_Renderer* r, int width, int height);
