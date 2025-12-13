#pragma once

#include "main_menu/menu.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct LayoutItem {
    std::string id;
    SDL_Rect rect;
};

void layout_set_page(const std::string& page_key);
RectNdc layout_rect(const std::string& id, const RectNdc& fallback);
void layout_toggle_edit_mode();
bool layout_edit_enabled();
void layout_handle_editor(std::vector<LayoutItem> items, int width, int height);
void layout_render_overlay(SDL_Renderer* r, const std::vector<LayoutItem>& items, int width, int height);
void layout_reset_current();
std::string layout_current_section();
