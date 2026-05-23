#pragma once

namespace ginput {

struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

inline ButtonState make_button_state(bool current_down, bool previous_down) {
    return ButtonState{
        current_down,
        current_down && !previous_down,
        !current_down && previous_down,
    };
}

} // namespace ginput
