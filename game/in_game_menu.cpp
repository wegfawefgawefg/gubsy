#include "game/in_game_menu.hpp"

#include <algorithm>

#include <glm/glm.hpp>

#include "engine/globals.hpp"
#include "engine/graphics.hpp"
#include "engine/menu/menu_system.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/menu/menu_input.hpp"

namespace {

bool g_in_game_menu_open = false;
MenuInputState g_prev_menu_input{};

bool back_pressed(const MenuInputState& input) {
    return input.back && !g_prev_menu_input.back;
}

bool menu_stack_is_open() {
    return es && !es->menu_manager.stack().empty();
}

void open_in_game_menu() {
    if (!es || g_in_game_menu_open)
        return;
    es->menu_manager.clear();
    es->menu_manager.push_screen(MenuScreenID::IN_GAME_SESSION);
    menu_system_reset();
    g_in_game_menu_open = true;
}

void close_in_game_menu() {
    if (!es)
        return;
    es->menu_manager.clear();
    menu_system_reset();
    g_in_game_menu_open = false;
}

} // namespace

void in_game_menu_reset() {
    g_prev_menu_input = MenuInputState{};
    close_in_game_menu();
}

bool in_game_menu_open() {
    return g_in_game_menu_open;
}

bool in_game_menu_blocks_game_input() {
    return g_in_game_menu_open;
}

void in_game_menu_process_inputs() {
    if (!es || !current_graphics() || !current_graphics()->renderer)
        return;

    const MenuInputState input = gather_menu_input();
    if (!g_in_game_menu_open) {
        if (back_pressed(input))
            open_in_game_menu();
        g_prev_menu_input = input;
        return;
    }

    glm::ivec2 dims = get_render_dimensions();
    const int width = std::max(dims.x, 1);
    const int height = std::max(dims.y, 1);
    menu_system_set_input(input);
    menu_system_update(es->dt, width, height);
    if (!menu_stack_is_open())
        close_in_game_menu();
    g_prev_menu_input = input;
}

void in_game_menu_render(SDL_Renderer* renderer, int width, int height) {
    if (!g_in_game_menu_open || !renderer)
        return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 6, 8, 12, 160);
    SDL_Rect backdrop{0, 0, width, height};
    SDL_RenderFillRect(renderer, &backdrop);
    menu_system_render(renderer, width, height);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
