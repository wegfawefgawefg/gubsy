#include "gubsy/ui/gview.hpp"

#include <SDL3/SDL.h>

namespace gubsy::ui {

// Adapts typed GView reads, writes, conditions, and actions directly to game events.
gview::Host make_view_host(ViewModel& model) {
    gview::Host host;
    host.read = model.read;
    host.write = model.write;
    host.condition = model.condition;
    host.action = model.event;
    host.revision = model.revision;
    return host;
}

// Reuses Gubsy's already-mapped semantic menu input without a second binding map.
gview::InputFrame make_view_input(const MenuInputState& input, std::string text) {
    gview::InputFrame frame;
    if (input.up)
        frame.navigation.push_back(gview::NavAction::Up);
    if (input.down)
        frame.navigation.push_back(gview::NavAction::Down);
    if (input.left)
        frame.navigation.push_back(gview::NavAction::Left);
    if (input.right)
        frame.navigation.push_back(gview::NavAction::Right);
    if (input.select)
        frame.navigation.push_back(gview::NavAction::Confirm);
    if (input.back)
        frame.navigation.push_back(gview::NavAction::Back);
    if (input.page_prev)
        frame.navigation.push_back(gview::NavAction::TabPrevious);
    if (input.page_next)
        frame.navigation.push_back(gview::NavAction::TabNext);
    frame.text = std::move(text);
    return frame;
}

// Adds native pointer input while leaving controller semantics independent.
void append_view_pointer(gview::InputFrame& input, const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        input.pointer.x = event.motion.x;
        input.pointer.y = event.motion.y;
        input.pointer.moved = true;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
               event.button.button == SDL_BUTTON_LEFT) {
        input.pointer.x = event.button.x;
        input.pointer.y = event.button.y;
        input.pointer.pressed = true;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
        input.pointer.x = event.button.x;
        input.pointer.y = event.button.y;
        input.pointer.released = true;
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        input.pointer.scroll_y += event.wheel.y;
    } else if (event.type == SDL_EVENT_TEXT_INPUT) {
        input.text += event.text.text;
    } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_BACKSPACE) {
        input.text.push_back('\b');
    }
}

} // namespace gubsy::ui
