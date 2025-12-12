#include "globals.hpp"
#include "engine/input_defs.hpp"

void MenuInputDebounceTimers::step(float dt) {
    left  = std::max(0.0f, left  - dt);
    right = std::max(0.0f, right - dt);
    up    = std::max(0.0f, up    - dt);
    down  = std::max(0.0f, down  - dt);
    page_prev = std::max(0.0f, page_prev - dt);
    page_next = std::max(0.0f, page_next - dt);
}

MenuInputs MenuInputDebounceTimers::debounce(const MenuInputs& in) const {
    MenuInputs out = in;
    out.left  = (left  == 0.0f) && in.left;
    out.right = (right == 0.0f) && in.right;
    out.up    = (up    == 0.0f) && in.up;
    out.down  = (down  == 0.0f) && in.down;
    out.page_prev = (page_prev == 0.0f) && in.page_prev;
    out.page_next = (page_next == 0.0f) && in.page_next;
    return out;
}

void PlayingInputDebounceTimers::step() {
    inventory_prev = std::max(0.0f, inventory_prev - ss->dt);
    inventory_next = std::max(0.0f, inventory_next - ss->dt);
}

PlayingInputs PlayingInputDebounceTimers::debounce(const PlayingInputs& in) const {
    PlayingInputs out = in;
    out.inventory_prev = (inventory_prev == 0.0f) && in.inventory_prev;
    out.inventory_next = (inventory_next == 0.0f) && in.inventory_next;
    return out;
}
