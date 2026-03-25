#include "game/menu/menu_input.hpp"

#include "engine/binds_profiles.hpp"
#include "engine/globals.hpp"
#include "engine/input_binding_utils.hpp"
#include "game/actions.hpp"

MenuInputState gather_menu_input() {
    MenuInputState state{};
    if (!es)
        return state;
    BindsProfile* profile = get_player_binds_profile(0);
    if (!profile)
        return state;
    for (const auto& [device_button, action] : profile->button_binds) {
        const bool down = device_button_is_down(es->device_state, device_button);
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
