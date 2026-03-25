#include "engine/menu/menu_render.hpp"

#include <SDL2/SDL.h>

#include "engine/engine_state.hpp"
#include "game/state.hpp"

void render_menu(MenuManager& manager,
                 EngineState& engine,
                 State& game,
                 SDL_Renderer* renderer,
                 int width,
                 int height) {
    (void)renderer;
    if (manager.stack().empty())
        return;
    auto& inst = const_cast<MenuManager::ScreenInstance&>(manager.stack().back());
    if (!inst.def || !inst.def->build)
        return;
    MenuContext ctx{engine,
                    game,
                    manager,
                    width,
                    height,
                    inst.player_index,
                    inst.def ? inst.def->id : kMenuIdInvalid,
                    inst.state_ptr};
    BuiltScreen built = inst.def->build(ctx);
    (void)built;
}
