#include "engine/input_system.hpp"

#include <SDL2/SDL_keyboard.h>

#include <algorithm>
#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "engine/player.hpp"

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
        controller_state.device_id = device_id;
        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
            Sint16 raw_value = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(axis));
            controller_state.axes[static_cast<std::size_t>(axis)] =
                static_cast<float>(raw_value) / 32767.0f;
        }
        for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button) {
            controller_state.buttons[static_cast<std::size_t>(button)] =
                SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(button));
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
