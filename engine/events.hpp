#pragma once

#include "game/actions.hpp"
#include <glm/glm.hpp>

enum class InputEventType {
    BUTTON,
    // Future-proofing for analog events
    // ANALOG_1D,
    // ANALOG_2D
};

struct InputEvent {
    InputEventType type;
    int player_index;
    int action; // This will be the integer from GameAction namespace

    union {
        struct { bool pressed; } button;
        // struct { float value; } analog1D;
        // struct { glm::vec2 value; } analog2D;
    } data;
};
