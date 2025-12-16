#pragma once

#include "main_menu/menu.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

void navgraph_set_page(const std::string& page_key);
NavNode navgraph_apply(int button_id, const NavNode& fallback);

void navgraph_toggle_edit_mode();
bool navgraph_edit_enabled();
void navgraph_handle_editor(const std::vector<ButtonDesc>& buttons, int width, int height);
void navgraph_render_overlay(SDL_Renderer* r, const std::vector<ButtonDesc>& buttons, int width, int height);
void navgraph_reset_current();
