#pragma once

#include "main_menu/menu.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

struct LayoutItem;

// Manage editor placeholder objects (non-button GUI rectangles)
void gui_objects_set_page(const std::string& page_key);
void gui_objects_save_if_dirty();
std::vector<LayoutItem> gui_objects_layout_items(int width, int height);

// Helpers for inline editing
struct GuiObjectPixel {
    std::string id;
    SDL_Rect rect;
    std::string label;
};

std::vector<GuiObjectPixel> gui_objects_pixel_items(int width, int height);
bool gui_objects_rename(const std::string& id, const std::string& new_label);
RectNdc gui_objects_rect_from_pixels(const SDL_Rect& rect, int width, int height);
