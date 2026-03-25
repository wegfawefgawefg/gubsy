#pragma once

/*
    This file defines the structure of a InputEvent.
    InputEvents are created by the engine's input processing system
    each frame based on the active player bindings.

    The 'action' field corresponds to the integer constants defined
    in 'game/actions.hpp'.

    Game code (like the 'step' function) can get a list of all events
    that occurred since the last frame and use them to drive gameplay.
*/

#include "game/actions.hpp"
#include <glm/glm.hpp>

enum class InputEventType {
    BUTTON,
    ANALOG_1D,
    ANALOG_2D
};

struct InputEvent {
    InputEventType type;
    int player_index;
    int action; // This will be an integer from the GameAction namespace

    union {
        struct { bool pressed; } button;
        struct { float value; } analog1D;
        struct { glm::vec2 value; } analog2D;
    } data;
};
