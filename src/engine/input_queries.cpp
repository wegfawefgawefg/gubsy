#include "engine/input_queries.hpp"
#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "game/events.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/player.hpp"
#include "engine/user_profiles.hpp"

#include <SDL_scancode.h>
#include <vector>

const std::vector<InputEvent>& get_input_events() {
    return es->input_event_queue;
}

// is_down remains state-based for continuous actions.
bool is_down(int player_index, int game_button) {
    const BindsProfile* binds = get_player_binds(player_index);
    if (!binds || !es->keystate) return false;

    // This is a fast lookup: GameAction -> GubsyButton
    if (binds->button_binds.count(game_button)) {
        int device_button = binds->button_binds.at(game_button);

        // Then GubsyButton -> SDL_Scancode
        if (gubsy_to_sdl_scancode.count(device_button)) {
            SDL_Scancode scancode = gubsy_to_sdl_scancode.at(device_button);
            return es->keystate[scancode];
        }
    }
    return false;
}

// was_pressed is now event-based for reliability
bool was_pressed(int player_index, int game_button) {
    for (const auto& event : es->input_event_queue) {
        if (event.player_index == player_index && event.action == game_button && event.type == InputEventType::BUTTON && event.data.button.pressed) {
            return true;
        }
    }
    return false;
}

// was_released is now event-based for reliability
bool was_released(int player_index, int game_button) {
    for (const auto& event : es->input_event_queue) {
        if (event.player_index == player_index && event.action == game_button && event.type == InputEventType::BUTTON && !event.data.button.pressed) {
            return true;
        }
    }
    return false;
}

float get_1d_analog(int player_index, int game_axis) {
    float result = 0.0f;
    // Iterate backwards to find the most recent event for this action
    for (auto it = es->input_event_queue.rbegin(); it != es->input_event_queue.rend(); ++it) {
        const auto& event = *it;
        if (event.player_index == player_index && event.action == game_axis && event.type == InputEventType::ANALOG_1D) {
            return event.data.analog1D.value;
        }
    }
    return result;
}

float get_1d_analog_delta(int player_index, int game_axis) { 
    // TODO: Not implemented. Requires historical state access.
    return 0.0f; 
}

glm::vec2 get_2d_analog(int player_index, int game_axis) {
    glm::vec2 result(0.0f);
    // Iterate backwards to find the most recent event for this action
    for (auto it = es->input_event_queue.rbegin(); it != es->input_event_queue.rend(); ++it) {
        const auto& event = *it;
        if (event.player_index == player_index && event.action == game_axis && event.type == InputEventType::ANALOG_2D) {
            return event.data.analog2D.value;
        }
    }
    return result;
}

glm::vec2 get_2d_analog_delta(int player_index, int game_axis) { 
    // TODO: Not implemented. Requires historical state access.
    return glm::vec2(0.0f); 
}

glm::vec2 get_mouse_pos(int player_index) { return glm::vec2(0.0f); }
glm::vec2 get_mouse_delta(int player_index) { return glm::vec2(0.0f); }
glm::vec2 get_mouse_pos_window(int player_index) { return glm::vec2(0.0f); }
glm::vec2 get_mouse_pos_screen(int player_index) { return glm::vec2(0.0f); }
int get_mouse_wheel_delta(int player_index) { return 0; }