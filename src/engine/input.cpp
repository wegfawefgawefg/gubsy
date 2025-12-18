#include "engine/input_system.hpp"

#include <SDL2/SDL_keyboard.h>

#include <algorithm>
#include <unordered_map>

#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "engine/player.hpp"

const std::unordered_map<int, SDL_Scancode> gubsy_to_sdl_scancode = {
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

void register_input_frame_builder(BuildInputFrameFn fn) {
    if (!es)
        return;
    es->input_frame_builder = fn;
}

void update_device_state_from_sdl() {
    if (!es)
        return;

    auto& state = es->device_state;
    const Uint8* sdl_keystate = SDL_GetKeyboardState(nullptr);
    std::copy(sdl_keystate, sdl_keystate + SDL_NUM_SCANCODES, state.keyboard.begin());

    int x = 0;
    int y = 0;
    state.mouse_buttons = SDL_GetMouseState(&x, &y);
    state.mouse_dx = x - state.mouse_x;
    state.mouse_dy = y - state.mouse_y;
    state.mouse_x = x;
    state.mouse_y = y;
    state.controllers.clear();
    state.controllers.reserve(es->open_controllers.size());
    for (auto const& [device_id, controller] : es->open_controllers) {
        DeviceState::ControllerState controller_state{};
        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
            Sint16 raw_value = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(axis));
            controller_state.axes[static_cast<std::size_t>(axis)] =
                static_cast<float>(raw_value) / 32767.0f;
        }
        state.controllers.push_back(controller_state);
    }
}

void accumulate_mouse_wheel_delta(int delta) {
    if (!es)
        return;
    es->device_state.mouse_wheel += delta;
}

namespace {

void ensure_input_frame_capacity() {
    const std::size_t count = es ? es->players.size() : 0;
    if (!es)
        return;
    if (es->input_frames_current.size() < count) {
        es->input_frames_current.resize(count);
        es->input_frames_previous.resize(count);
    }
}

const InputFrame& safe_frame(const std::vector<InputFrame>& frames, int player_index) {
    static InputFrame empty{};
    if (!es)
        return empty;
    if (player_index < 0 || static_cast<std::size_t>(player_index) >= frames.size())
        return empty;
    return frames[static_cast<std::size_t>(player_index)];
}

} // namespace

void build_input_frames_for_step() {
    if (!es)
        return;

    ensure_input_frame_capacity();

    const std::size_t player_count = es->players.size();
    for (std::size_t i = 0; i < player_count; ++i) {
        InputFrame next{};
        if (es->input_frame_builder)
            es->input_frame_builder(static_cast<int>(i), es->device_state, next);
        es->input_frames_previous[i] = es->input_frames_current[i];
        es->input_frames_current[i] = next;
    }

    es->device_state.mouse_wheel = 0;
    es->device_state.mouse_dx = 0;
    es->device_state.mouse_dy = 0;
}

const InputFrame& current_input_frame(int player_index) {
    return safe_frame(es->input_frames_current, player_index);
}

const InputFrame& previous_input_frame(int player_index) {
    return safe_frame(es->input_frames_previous, player_index);
}
