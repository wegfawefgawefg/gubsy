#include <algorithm>
#include <cctype>
#include "mode_registry.hpp"
#include <engine/mode_registry.hpp>
#include <SDL_keyboard.h>
#include <engine/globals.hpp>


#include <algorithm>
#include <cctype>
#include "mode_registry.hpp"
#include <engine/mode_registry.hpp>
#include <SDL_keyboard.h>
#include <engine/globals.hpp>
#include <engine/input.hpp>
#include "engine/binds_profiles.hpp"
#include "engine/player.hpp"
#include "engine/user_profiles.hpp"
#include <unordered_map>

namespace {
// This map is crucial for translating the abstract GubsyButton enum into a concrete SDL_Scancode
// that can be used to check the keyboard state array.
static const std::unordered_map<int, SDL_Scancode> gubsy_to_sdl_scancode = {
    {static_cast<int>(GubsyButton::KB_A), SDL_SCANCODE_A},
    {static_cast<int>(GubsyButton::KB_B), SDL_SCANCODE_B},
    {static_cast<int>(GubsyButton::KB_C), SDL_SCANCODE_C},
    {static_cast<int>(GubsyButton::KB_D), SDL_SCANCODE_D},
    {static_cast<int>(GubsyButton::KB_E), SDL_SCANCODE_E},
    {static_cast<int>(GubsyButton::KB_F), SDL_SCANCODE_F},
    {static_cast<int>(GubsyButton::KB_G), SDL_SCANCODE_G},
    {static_cast<int>(GubsyButton::KB_H), SDL_SCANCODE_H},
    {static_cast<int>(GubsyButton::KB_I), SDL_SCANCODE_I},
    {static_cast<int>(GubsyButton::KB_J), SDL_SCANCODE_J},
    {static_cast<int>(GubsyButton::KB_K), SDL_SCANCODE_K},
    {static_cast<int>(GubsyButton::KB_L), SDL_SCANCODE_L},
    {static_cast<int>(GubsyButton::KB_M), SDL_SCANCODE_M},
    {static_cast<int>(GubsyButton::KB_N), SDL_SCANCODE_N},
    {static_cast<int>(GubsyButton::KB_O), SDL_SCANCODE_O},
    {static_cast<int>(GubsyButton::KB_P), SDL_SCANCODE_P},
    {static_cast<int>(GubsyButton::KB_Q), SDL_SCANCODE_Q},
    {static_cast<int>(GubsyButton::KB_R), SDL_SCANCODE_R},
    {static_cast<int>(GubsyButton::KB_S), SDL_SCANCODE_S},
    {static_cast<int>(GubsyButton::KB_T), SDL_SCANCODE_T},
    {static_cast<int>(GubsyButton::KB_U), SDL_SCANCODE_U},
    {static_cast<int>(GubsyButton::KB_V), SDL_SCANCODE_V},
    {static_cast<int>(GubsyButton::KB_W), SDL_SCANCODE_W},
    {static_cast<int>(GubsyButton::KB_X), SDL_SCANCODE_X},
    {static_cast<int>(GubsyButton::KB_Y), SDL_SCANCODE_Y},
    {static_cast<int>(GubsyButton::KB_Z), SDL_SCANCODE_Z},
    {static_cast<int>(GubsyButton::KB_0), SDL_SCANCODE_0},
    {static_cast<int>(GubsyButton::KB_1), SDL_SCANCODE_1},
    {static_cast<int>(GubsyButton::KB_2), SDL_SCANCODE_2},
    {static_cast<int>(GubsyButton::KB_3), SDL_SCANCODE_3},
    {static_cast<int>(GubsyButton::KB_4), SDL_SCANCODE_4},
    {static_cast<int>(GubsyButton::KB_5), SDL_SCANCODE_5},
    {static_cast<int>(GubsyButton::KB_6), SDL_SCANCODE_6},
    {static_cast<int>(GubsyButton::KB_7), SDL_SCANCODE_7},
    {static_cast<int>(GubsyButton::KB_8), SDL_SCANCODE_8},
    {static_cast<int>(GubsyButton::KB_9), SDL_SCANCODE_9},
    {static_cast<int>(GubsyButton::KB_F1), SDL_SCANCODE_F1},
    {static_cast<int>(GubsyButton::KB_F2), SDL_SCANCODE_F2},
    {static_cast<int>(GubsyButton::KB_F3), SDL_SCANCODE_F3},
    {static_cast<int>(GubsyButton::KB_F4), SDL_SCANCODE_F4},
    {static_cast<int>(GubsyButton::KB_F5), SDL_SCANCODE_F5},
    {static_cast<int>(GubsyButton::KB_F6), SDL_SCANCODE_F6},
    {static_cast<int>(GubsyButton::KB_F7), SDL_SCANCODE_F7},
    {static_cast<int>(GubsyButton::KB_F8), SDL_SCANCODE_F8},
    {static_cast<int>(GubsyButton::KB_F9), SDL_SCANCODE_F9},
    {static_cast<int>(GubsyButton::KB_F10), SDL_SCANCODE_F10},
    {static_cast<int>(GubsyButton::KB_F11), SDL_SCANCODE_F11},
    {static_cast<int>(GubsyButton::KB_F12), SDL_SCANCODE_F12},
    {static_cast<int>(GubsyButton::KB_KP_0), SDL_SCANCODE_KP_0},
    {static_cast<int>(GubsyButton::KB_KP_1), SDL_SCANCODE_KP_1},
    {static_cast<int>(GubsyButton::KB_KP_2), SDL_SCANCODE_KP_2},
    {static_cast<int>(GubsyButton::KB_KP_3), SDL_SCANCODE_KP_3},
    {static_cast<int>(GubsyButton::KB_KP_4), SDL_SCANCODE_KP_4},
    {static_cast<int>(GubsyButton::KB_KP_5), SDL_SCANCODE_KP_5},
    {static_cast<int>(GubsyButton::KB_KP_6), SDL_SCANCODE_KP_6},
    {static_cast<int>(GubsyButton::KB_KP_7), SDL_SCANCODE_KP_7},
    {static_cast<int>(GubsyButton::KB_KP_8), SDL_SCANCODE_KP_8},
    {static_cast<int>(GubsyButton::KB_KP_9), SDL_SCANCODE_KP_9},
    {static_cast<int>(GubsyButton::KB_KP_PERIOD), SDL_SCANCODE_KP_PERIOD},
    {static_cast<int>(GubsyButton::KB_KP_DIVIDE), SDL_SCANCODE_KP_DIVIDE},
    {static_cast<int>(GubsyButton::KB_KP_MULTIPLY), SDL_SCANCODE_KP_MULTIPLY},
    {static_cast<int>(GubsyButton::KB_KP_MINUS), SDL_SCANCODE_KP_MINUS},
    {static_cast<int>(GubsyButton::KB_KP_PLUS), SDL_SCANCODE_KP_PLUS},
    {static_cast<int>(GubsyButton::KB_KP_ENTER), SDL_SCANCODE_KP_ENTER},
    {static_cast<int>(GubsyButton::KB_KP_EQUALS), SDL_SCANCODE_KP_EQUALS},
    {static_cast<int>(GubsyButton::KB_UP), SDL_SCANCODE_UP},
    {static_cast<int>(GubsyButton::KB_DOWN), SDL_SCANCODE_DOWN},
    {static_cast<int>(GubsyButton::KB_LEFT), SDL_SCANCODE_LEFT},
    {static_cast<int>(GubsyButton::KB_RIGHT), SDL_SCANCODE_RIGHT},
    {static_cast<int>(GubsyButton::KB_LSHIFT), SDL_SCANCODE_LSHIFT},
    {static_cast<int>(GubsyButton::KB_RSHIFT), SDL_SCANCODE_RSHIFT},
    {static_cast<int>(GubsyButton::KB_LCTRL), SDL_SCANCODE_LCTRL},
    {static_cast<int>(GubsyButton::KB_RCTRL), SDL_SCANCODE_RCTRL},
    {static_cast<int>(GubsyButton::KB_LALT), SDL_SCANCODE_LALT},
    {static_cast<int>(GubsyButton::KB_RALT), SDL_SCANCODE_RALT},
    {static_cast<int>(GubsyButton::KB_LGUI), SDL_SCANCODE_LGUI},
    {static_cast<int>(GubsyButton::KB_RGUI), SDL_SCANCODE_RGUI},
    {static_cast<int>(GubsyButton::KB_ESCAPE), SDL_SCANCODE_ESCAPE},
    {static_cast<int>(GubsyButton::KB_TAB), SDL_SCANCODE_TAB},
    {static_cast<int>(GubsyButton::KB_CAPSLOCK), SDL_SCANCODE_CAPSLOCK},
    {static_cast<int>(GubsyButton::KB_SPACE), SDL_SCANCODE_SPACE},
    {static_cast<int>(GubsyButton::KB_BACKSPACE), SDL_SCANCODE_BACKSPACE},
    {static_cast<int>(GubsyButton::KB_ENTER), SDL_SCANCODE_RETURN},
    {static_cast<int>(GubsyButton::KB_INSERT), SDL_SCANCODE_INSERT},
    {static_cast<int>(GubsyButton::KB_DELETE), SDL_SCANCODE_DELETE},
    {static_cast<int>(GubsyButton::KB_HOME), SDL_SCANCODE_HOME},
    {static_cast<int>(GubsyButton::KB_END), SDL_SCANCODE_END},
    {static_cast<int>(GubsyButton::KB_PAGEUP), SDL_SCANCODE_PAGEUP},
    {static_cast<int>(GubsyButton::KB_PAGEDOWN), SDL_SCANCODE_PAGEDOWN},
    {static_cast<int>(GubsyButton::KB_PRINTSCREEN), SDL_SCANCODE_PRINTSCREEN},
    {static_cast<int>(GubsyButton::KB_SCROLLLOCK), SDL_SCANCODE_SCROLLLOCK},
    {static_cast<int>(GubsyButton::KB_PAUSE), SDL_SCANCODE_PAUSE},
    {static_cast<int>(GubsyButton::KB_MINUS), SDL_SCANCODE_MINUS},
    {static_cast<int>(GubsyButton::KB_EQUALS), SDL_SCANCODE_EQUALS},
    {static_cast<int>(GubsyButton::KB_LEFTBRACKET), SDL_SCANCODE_LEFTBRACKET},
    {static_cast<int>(GubsyButton::KB_RIGHTBRACKET), SDL_SCANCODE_RIGHTBRACKET},
    {static_cast<int>(GubsyButton::KB_BACKSLASH), SDL_SCANCODE_BACKSLASH},
    {static_cast<int>(GubsyButton::KB_SEMICOLON), SDL_SCANCODE_SEMICOLON},
    {static_cast<int>(GubsyButton::KB_APOSTROPHE), SDL_SCANCODE_APOSTROPHE},
    {static_cast<int>(GubsyButton::KB_GRAVE), SDL_SCANCODE_GRAVE},
    {static_cast<int>(GubsyButton::KB_COMMA), SDL_SCANCODE_COMMA},
    {static_cast<int>(GubsyButton::KB_PERIOD), SDL_SCANCODE_PERIOD},
    {static_cast<int>(GubsyButton::KB_SLASH), SDL_SCANCODE_SLASH},
    {static_cast<int>(GubsyButton::KB_NUMLOCK), SDL_SCANCODE_NUMLOCKCLEAR},
    {static_cast<int>(GubsyButton::KB_APPLICATION), SDL_SCANCODE_APPLICATION},
    {static_cast<int>(GubsyButton::KB_MENU), SDL_SCANCODE_MENU},
};
} // namespace


void process_inputs() {
    // 1. Update keyboard state arrays
    const Uint8* sdl_keystate = SDL_GetKeyboardState(nullptr);
    memcpy(es->last_keystate, es->keystate, SDL_NUM_SCANCODES * sizeof(Uint8));
    memcpy(es->keystate, sdl_keystate, SDL_NUM_SCANCODES * sizeof(Uint8));
    if (es->frame == 0) {
        memset(es->last_keystate, 0, SDL_NUM_SCANCODES * sizeof(Uint8));
    }

    // 2. Clear the event queue for the new frame
    es->input_event_queue.clear();

    // 3. Find the active binds profile for player 0 (simplified)
    const BindsProfile* binds = get_player_binds(0);
    if (!binds) return;


// 4. Iterate through all possible physical keys to check for state changes
    for (int i = 0; i < static_cast<int>(GubsyButton::COUNT); ++i) {
        // Find the corresponding SDL scancode for this GubsyButton
        if (gubsy_to_sdl_scancode.count(i) == 0) {
            continue; // Skip non-keyboard buttons for now
        }
        SDL_Scancode scancode = gubsy_to_sdl_scancode.at(i);

        bool is_down_now = es->keystate[scancode];
        bool was_down_before = es->last_keystate[scancode];

        if (is_down_now == was_down_before) {
            continue; // No change in state for this key
        }

        // A press or release was detected for this physical key `i`.
        // Now, find all game actions that are bound to this physical key.
        for (const auto& [game_action, device_button] : binds->button_binds) {
            if (device_button == i) {
                // Match found! Create and push an event.
                InputEvent event;
                event.type = InputEventType::BUTTON;
                event.player_index = 0; // Hardcoded for now
                event.action = game_action;
                event.data.button.pressed = is_down_now;
                es->input_event_queue.push_back(event);
            }
        }
    }

    // 5. Iterate through all open game controllers for analog changes
    const float DEADZONE = 0.2f;
    for (auto const& [device_id, controller] : es->open_controllers) {
        auto& state = es->gamepad_states[device_id];
        
        // Copy last state
        memcpy(state.last_axes, state.axes, sizeof(state.axes));

        // Get new state for all axes
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
            Sint16 raw_value = SDL_GameControllerGetAxis(controller, (SDL_GameControllerAxis)i);
            // Normalize to -1.0 to 1.0 for sticks, 0.0 to 1.0 for triggers
            float normalized_value = raw_value / 32767.0f;
            state.axes[i] = normalized_value;
        }

        // Handle 2D sticks
        // Left Stick
        glm::vec2 left_stick_now(state.axes[SDL_CONTROLLER_AXIS_LEFTX], state.axes[SDL_CONTROLLER_AXIS_LEFTY]);
        glm::vec2 left_stick_last(state.last_axes[SDL_CONTROLLER_AXIS_LEFTX], state.last_axes[SDL_CONTROLLER_AXIS_LEFTY]);
        if ((glm::length(left_stick_now) > DEADZONE || glm::length(left_stick_last) > DEADZONE) && glm::distance(left_stick_now, left_stick_last) > 0.001f) {
            for (const auto& [game_action, device_2d_axis] : binds->analog_2d_binds) {
                if (device_2d_axis == static_cast<int>(Gubsy2DAnalog::GP_LEFT_STICK)) {
                    InputEvent event;
                    event.type = InputEventType::ANALOG_2D;
                    event.player_index = 0;
                    event.action = game_action;
                    event.data.analog2D.value = left_stick_now;
                    es->input_event_queue.push_back(event);
                }
            }
        }
        // Right Stick
        glm::vec2 right_stick_now(state.axes[SDL_CONTROLLER_AXIS_RIGHTX], state.axes[SDL_CONTROLLER_AXIS_RIGHTY]);
        glm::vec2 right_stick_last(state.last_axes[SDL_CONTROLLER_AXIS_RIGHTX], state.last_axes[SDL_CONTROLLER_AXIS_RIGHTY]);
        if ((glm::length(right_stick_now) > DEADZONE || glm::length(right_stick_last) > DEADZONE) && glm::distance(right_stick_now, right_stick_last) > 0.001f) {
            for (const auto& [game_action, device_2d_axis] : binds->analog_2d_binds) {
                if (device_2d_axis == static_cast<int>(Gubsy2DAnalog::GP_RIGHT_STICK)) {
                    InputEvent event;
                    event.type = InputEventType::ANALOG_2D;
                    event.player_index = 0;
                    event.action = game_action;
                    event.data.analog2D.value = right_stick_now;
                    es->input_event_queue.push_back(event);
                }
            }
        }

        // Handle 1D triggers
        // Left Trigger
        float left_trigger_now = state.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT];
        float left_trigger_last = state.last_axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT];
        if ((left_trigger_now > DEADZONE || left_trigger_last > DEADZONE) && abs(left_trigger_now - left_trigger_last) > 0.001f) {
            for (const auto& [game_action, device_1d_axis] : binds->analog_1d_binds) {
                if (device_1d_axis == static_cast<int>(Gubsy1DAnalog::GP_LEFT_TRIGGER)) {
                    InputEvent event;
                    event.type = InputEventType::ANALOG_1D;
                    event.player_index = 0;
                    event.action = game_action;
                    event.data.analog1D.value = left_trigger_now;
                    es->input_event_queue.push_back(event);
                }
            }
        }
        // Right Trigger
        float right_trigger_now = state.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT];
        float right_trigger_last = state.last_axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT];
        if ((right_trigger_now > DEADZONE || right_trigger_last > DEADZONE) && abs(right_trigger_now - right_trigger_last) > 0.001f) {
            for (const auto& [game_action, device_1d_axis] : binds->analog_1d_binds) {
                if (device_1d_axis == static_cast<int>(Gubsy1DAnalog::GP_RIGHT_TRIGGER)) {
                    InputEvent event;
                    event.type = InputEventType::ANALOG_1D;
                    event.player_index = 0;
                    event.action = game_action;
                    event.data.analog1D.value = right_trigger_now;
                    es->input_event_queue.push_back(event);
                }
            }
        }
    }


    // 6. Dispatch to legacy mode-specific input processor (will be phased out)
    if (const ModeDesc* mode = find_mode(es->mode)) {
        if (mode->process_inputs_fn) {
            mode->process_inputs_fn();
        }
    }

    // 7. Handle hardcoded debug inputs
    static bool was_debug_combo_down = false;
    bool is_debug_combo_down = es->keystate[SDL_SCANCODE_LCTRL] && es->keystate[SDL_SCANCODE_LALT] && es->keystate[SDL_SCANCODE_I];
    if (is_debug_combo_down && !was_debug_combo_down) {
        es->draw_input_device_overlay = !es->draw_input_device_overlay;
    }
    was_debug_combo_down = is_debug_combo_down;
}

const BindsProfile* get_player_binds(int player_index) {
    if (!es || player_index < 0 || player_index >= es->players.size()) {
        return nullptr;
    }
    const auto& player = es->players[static_cast<std::size_t>(player_index)];

    // Find user profile
    const UserProfile* user_profile = nullptr;
    for (const auto& up : es->user_profiles_pool) {
        if (up.id == player.profile.id) {
            user_profile = &up;
            break;
        }
    }

    // if there isnt one, just set it to the first one that exists
    if (!user_profile && !es->user_profiles_pool.empty()) {
        user_profile = &es->user_profiles_pool[0];
    }

    // Find binds profile
    const BindsProfile* binds_profile = nullptr;
    for (const auto& bp : es->binds_profiles) {
        if (bp.id == user_profile->last_binds_profile_id) {
            binds_profile = &bp;
            break;
        }
    }

    // As a fallback, just grab the first binds profile
    if (!binds_profile && !es->binds_profiles.empty()) {
        binds_profile = &es->binds_profiles[0];
    }

    return binds_profile;
}

