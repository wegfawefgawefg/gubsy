#include "demo/in_game_menu.hpp"

#include <algorithm>

#include <glm/glm.hpp>

#include "src/engine_state.hpp"
#include "src/graphics.hpp"
#include "src/menu/menu_system.hpp"
#include "demo/menu/menu_ids.hpp"
#include "demo/menu/menu_input.hpp"

namespace {

bool g_in_game_menu_open = false;
MenuInputState g_prev_menu_input{};

bool back_pressed(const MenuInputState& input) {
    return input.back && !g_prev_menu_input.back;
}

bool menu_stack_is_open(const EngineState& engine) {
    return !engine.menu_manager.stack().empty();
}

void open_in_game_menu(EngineState& engine) {
    if (g_in_game_menu_open)
        return;
    engine.menu_manager.clear();
    engine.menu_manager.push_screen(MenuScreenID::IN_GAME_SESSION);
    menu_system_reset(engine);
    g_in_game_menu_open = true;
}

void close_in_game_menu(EngineState& engine) {
    engine.menu_manager.clear();
    menu_system_reset(engine);
    g_in_game_menu_open = false;
}

} // namespace

void in_game_menu_reset(EngineState& engine) {
    g_prev_menu_input = MenuInputState{};
    close_in_game_menu(engine);
}

bool in_game_menu_open() {
    return g_in_game_menu_open;
}

bool in_game_menu_blocks_game_input() {
    return g_in_game_menu_open;
}

void in_game_menu_process_inputs(EngineState& engine) {
    if (!current_graphics(engine) || !current_graphics(engine)->renderer)
        return;

    const MenuInputState input = gather_menu_input(engine);
    if (!g_in_game_menu_open) {
        if (back_pressed(input))
            open_in_game_menu(engine);
        g_prev_menu_input = input;
        return;
    }

    glm::ivec2 dims = get_render_dimensions(engine);
    const int width = std::max(dims.x, 1);
    const int height = std::max(dims.y, 1);
    menu_system_set_input(engine, input);
    menu_system_update(engine, engine.dt, width, height);
    if (!menu_stack_is_open(engine))
        close_in_game_menu(engine);
    g_prev_menu_input = input;
}

void in_game_menu_render(EngineState& engine, SDL_Renderer* renderer, int width, int height) {
    if (!g_in_game_menu_open || !renderer)
        return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 6, 8, 12, 160);
    SDL_FRect backdrop{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    SDL_RenderFillRect(renderer, &backdrop);
    menu_system_render(engine, renderer, width, height);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
