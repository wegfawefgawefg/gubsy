#include "game/title.hpp"

#include <algorithm>

#include <SDL2/SDL_render.h>

#include "engine/alerts.hpp"
#include "engine/globals.hpp"
#include "engine/graphics.hpp"
#include "engine/menu/menu_system.hpp"
#include "game/menu/lobby_online.hpp"
#include "game/menu/menu_input.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/modes.hpp"

namespace {

bool g_menu_initialized = false;

void ensure_menu_ready() {
    if (!es)
        return;
    if (g_menu_initialized && !es->menu_manager.stack().empty())
        return;
    es->menu_manager.clear();
    es->menu_manager.push_screen(MenuScreenID::MAIN);
    es->menu_manager.push_screen(MenuScreenID::LOBBY);
    menu_system_reset();
    g_menu_initialized = true;
}

} // namespace

void title_step(void*) {
    LobbySession& lobby = lobby_state();
    lobby_online_tick(lobby);
    std::string close_reason;
    if (lobby_online_consume_session_close(lobby, close_reason))
        add_alert(close_reason);
    if (es && lobby_online_ready_to_enter_game(lobby))
        es->mode = modes::PLAYING;
}

void title_process_inputs(void*) {
    ensure_menu_ready();
    if (!current_graphics() || !current_graphics()->renderer)
        return;
    glm::ivec2 dims = get_render_dimensions();
    int width = std::max(dims.x, 1);
    int height = std::max(dims.y, 1);
    MenuInputState input = gather_menu_input();
    menu_system_set_input(input);
    menu_system_update(es ? es->dt : 0.0f, width, height);
}

void title_draw(void*) {
    if (!current_graphics() || !current_graphics()->renderer)
        return;
    SDL_Renderer* renderer = current_graphics()->renderer;
    glm::ivec2 dims = get_render_dimensions();
    int width = std::max(dims.x, 1);
    int height = std::max(dims.y, 1);
    SDL_SetRenderDrawColor(renderer, 14, 12, 26, 255);
    SDL_RenderClear(renderer);
    menu_system_render(renderer, width, height);
}
