#include "demo/title.hpp"

#include <algorithm>

#include <SDL2/SDL_render.h>

#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/graphics.hpp"
#include "src/menu/menu_system.hpp"
#include "demo/in_game_menu.hpp"
#include "demo/menu/lobby_online.hpp"
#include "demo/menu/menu_input.hpp"
#include "demo/menu/lobby_state.hpp"
#include "demo/menu/menu_ids.hpp"
#include "demo/modes.hpp"

namespace {

bool g_menu_initialized = false;

void ensure_menu_ready(EngineState& engine) {
    if (g_menu_initialized && !engine.menu_manager.stack().empty())
        return;
    engine.menu_manager.clear();
    engine.menu_manager.push_screen(MenuScreenID::MAIN);
    engine.menu_manager.push_screen(MenuScreenID::LOBBY);
    menu_system_reset(engine);
    g_menu_initialized = true;
}

} // namespace

void title_step(EngineState& engine, void*) {
    LobbySession& lobby = lobby_state();
    lobby_online_tick(lobby);
    std::string close_reason;
    if (lobby_online_consume_session_close(lobby, close_reason))
        add_alert(engine, close_reason);
    if (lobby_online_ready_to_enter_game(lobby)) {
        in_game_menu_reset(engine);
        engine.mode = modes::PLAYING;
    }
}

void title_process_inputs(EngineState& engine, void*) {
    ensure_menu_ready(engine);
    if (!current_graphics(engine) || !current_graphics(engine)->renderer)
        return;
    glm::ivec2 dims = get_render_dimensions(engine);
    int width = std::max(dims.x, 1);
    int height = std::max(dims.y, 1);
    MenuInputState input = gather_menu_input(engine);
    menu_system_set_input(engine, input);
    menu_system_update(engine, engine.dt, width, height);
}

void title_draw(EngineState& engine, void*) {
    if (!current_graphics(engine) || !current_graphics(engine)->renderer)
        return;
    SDL_Renderer* renderer = current_graphics(engine)->renderer;
    glm::ivec2 dims = get_render_dimensions(engine);
    int width = std::max(dims.x, 1);
    int height = std::max(dims.y, 1);
    SDL_SetRenderDrawColor(renderer, 14, 12, 26, 255);
    SDL_RenderClear(renderer);
    menu_system_render(engine, renderer, width, height);
}
