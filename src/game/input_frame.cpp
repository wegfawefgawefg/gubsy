#include "game/input_frame.hpp"

#include "engine/binds_profiles.hpp"
#include "engine/device_state.hpp"
#include "engine/globals.hpp"
#include "engine/input.hpp"
#include "engine/player.hpp"

#include <algorithm>
#include <cmath>

namespace {

bool button_is_down(const DeviceState& state, int device_button) {
    auto it = gubsy_to_sdl_scancode.find(device_button);
    if (it != gubsy_to_sdl_scancode.end()) {
        SDL_Scancode sc = it->second;
        if (sc >= 0 && sc < static_cast<int>(state.keyboard.size()))
            return state.keyboard[sc] != 0;
    }
    return false;
}

float controller_axis(const DeviceState& state, SDL_GameControllerAxis axis) {
    if (state.controllers.empty())
        return 0.0f;
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
        return 0.0f;
    return state.controllers.front().axes[axis];
}

int16_t encode_axis(float value) {
    value = std::clamp(value, -1.0f, 1.0f);
    return static_cast<int16_t>(std::round(value * 32767.0f));
}

void write_1d(InputFrame& frame, int analog_index, float value) {
    frame.analog_1d[static_cast<std::size_t>(analog_index)] = encode_axis(value);
}

void write_2d(InputFrame& frame, int analog_index, float x, float y) {
    auto& dest = frame.analog_2d[static_cast<std::size_t>(analog_index)];
    dest.x = encode_axis(x);
    dest.y = encode_axis(y);
}

std::pair<float, float> normalized_mouse_coords(const DeviceState& state) {
    if (!gg || !gg->renderer)
        return {0.0f, 0.0f};
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(gg->renderer, &width, &height);
    if (width <= 1)
        width = 2;
    if (height <= 1)
        height = 2;
    float norm_x = (static_cast<float>(state.mouse_x) / static_cast<float>(width - 1)) * 2.0f - 1.0f;
    float norm_y = (static_cast<float>(state.mouse_y) / static_cast<float>(height - 1)) * 2.0f - 1.0f;
    return {std::clamp(norm_x, -1.0f, 1.0f), std::clamp(norm_y, -1.0f, 1.0f)};
}

} // namespace

void build_input_frame(int player_index, const DeviceState& device_state, InputFrame& out) {
    out = InputFrame{};

    const BindsProfile* binds = get_player_binds_profile(player_index);
    if (!binds)
        return;

    for (const auto& [device_button, action] : binds->button_binds) {
        if (action < 0 || action >= 32)
            continue;
        if (button_is_down(device_state, device_button))
            out.down_bits |= (1u << action);
    }

    for (const auto& [device_axis, analog_id] : binds->analog_1d_binds) {
        if (analog_id < 0 || static_cast<std::size_t>(analog_id) >= out.analog_1d.size())
            continue;
        float value = 0.0f;
        switch (static_cast<Gubsy1DAnalog>(device_axis)) {
            case Gubsy1DAnalog::GP_LEFT_TRIGGER:
                value = controller_axis(device_state, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                if (device_state.controllers.empty()) {
                    auto [norm_x, norm_y] = normalized_mouse_coords(device_state);
                    value = -norm_y;
                }
                break;
            case Gubsy1DAnalog::GP_RIGHT_TRIGGER:
                value = controller_axis(device_state, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
                break;
            case Gubsy1DAnalog::MOUSE_WHEEL:
                value = static_cast<float>(device_state.mouse_wheel) * 0.1f;
                break;
            case Gubsy1DAnalog::COUNT:
                break;
        }
        write_1d(out, analog_id, value);
    }

    auto [mouse_norm_x, mouse_norm_y] = normalized_mouse_coords(device_state);
    for (const auto& [device_axis, analog_id] : binds->analog_2d_binds) {
        if (analog_id < 0 || static_cast<std::size_t>(analog_id) >= out.analog_2d.size())
            continue;
        float x = 0.0f;
        float y = 0.0f;
        switch (static_cast<Gubsy2DAnalog>(device_axis)) {
            case Gubsy2DAnalog::GP_LEFT_STICK:
                if (!device_state.controllers.empty()) {
                    x = controller_axis(device_state, SDL_CONTROLLER_AXIS_LEFTX);
                    y = controller_axis(device_state, SDL_CONTROLLER_AXIS_LEFTY);
                } else {
                    x = mouse_norm_x;
                    y = mouse_norm_y;
                }
                break;
            case Gubsy2DAnalog::GP_RIGHT_STICK:
                x = controller_axis(device_state, SDL_CONTROLLER_AXIS_RIGHTX);
                y = controller_axis(device_state, SDL_CONTROLLER_AXIS_RIGHTY);
                break;
            case Gubsy2DAnalog::MOUSE_XY:
                x = mouse_norm_x;
                y = mouse_norm_y;
                break;
            case Gubsy2DAnalog::COUNT:
                break;
        }
        write_2d(out, analog_id, x, y);
    }
}
