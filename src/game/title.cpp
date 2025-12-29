#include "game/title.hpp"

#include "engine/globals.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/input_binding_utils.hpp"
#include "engine/menu/menu_system.hpp"
#include "engine/render.hpp"
#include "engine/graphics.hpp"
#include "game/modes.hpp"
#include "game/actions.hpp"
#include "game/menu/menu_ids.hpp"

#include <SDL2/SDL_render.h>

namespace {

bool g_menu_initialized = false;

MenuInputState gather_menu_input() {
    MenuInputState state{};
    if (!es)
        return state;
    BindsProfile* profile = get_player_binds_profile(0);
    if (!profile)
        return state;
    for (const auto& [device_button, action] : profile->button_binds) {
        bool down = device_button_is_down(es->device_state, device_button);
        switch (action) {
            case GameAction::MENU_UP: state.up |= down; break;
            case GameAction::MENU_DOWN: state.down |= down; break;
            case GameAction::MENU_LEFT: state.left |= down; break;
            case GameAction::MENU_RIGHT: state.right |= down; break;
            case GameAction::MENU_SELECT: state.select |= down; break;
            case GameAction::MENU_BACK: state.back |= down; break;
            case GameAction::MENU_PAGE_PREV: state.page_prev |= down; break;
            case GameAction::MENU_PAGE_NEXT: state.page_next |= down; break;
            default: break;
        }
    }
    return state;
}

void ensure_menu_ready() {
    if (g_menu_initialized || !es)
        return;
    es->menu_manager.clear();
    es->menu_manager.push_screen(MenuScreenID::MAIN);
    es->menu_manager.push_screen(MenuScreenID::LOBBY);
    menu_system_reset();
    g_menu_initialized = true;
}

} // namespace

void title_step() {}

void title_process_inputs() {
    ensure_menu_ready();
    if (!gg || !gg->renderer)
        return;
    glm::ivec2 dims = get_render_dimensions();
    int width = std::max(dims.x, 1);
    int height = std::max(dims.y, 1);
    MenuInputState input = gather_menu_input();
    menu_system_set_input(input);
    menu_system_update(es ? es->dt : 0.0f, width, height);
}

void title_draw() {
    if (!gg || !gg->renderer)
        return;
    SDL_Renderer* renderer = gg->renderer;
    glm::ivec2 dims = get_render_dimensions();
    int width = std::max(dims.x, 1);
    int height = std::max(dims.y, 1);
    SDL_SetRenderDrawColor(renderer, 14, 12, 26, 255);
    SDL_RenderClear(renderer);
    menu_system_render(renderer, width, height);
    render_alerts(renderer, width);
}
