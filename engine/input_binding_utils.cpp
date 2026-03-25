#include "engine/input_binding_utils.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <unordered_map>

#include <glm/geometric.hpp>

#include "engine/globals.hpp"
#include "engine/graphics.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/imgui_layer.hpp"

namespace {

constexpr int kExtendedBindingFlag = 1 << 29;
constexpr int kKindShift = 26;
constexpr int kDeviceShift = 16;
constexpr int kDeviceMask = 0x3FF;
constexpr int kCodeMask = 0xFFFF;
constexpr int kAnyDeviceEncoded = kDeviceMask;

constexpr int kMouseWheelAxis = 0;
constexpr int kMouseAxisX = 0;
constexpr int kMouseAxisY = 1;

struct ButtonDescriptor {
    DeviceInputKind kind;
    int code;
};

constexpr uint32_t mouse_mask(int button_constant) {
    return SDL_BUTTON(button_constant);
}

const std::unordered_map<int, ButtonDescriptor> kLegacyButtonMap = {
    {static_cast<int>(GubsyButton::KB_A), {DeviceInputKind::Keyboard, SDL_SCANCODE_A}},
    {static_cast<int>(GubsyButton::KB_B), {DeviceInputKind::Keyboard, SDL_SCANCODE_B}},
    {static_cast<int>(GubsyButton::KB_C), {DeviceInputKind::Keyboard, SDL_SCANCODE_C}},
    {static_cast<int>(GubsyButton::KB_D), {DeviceInputKind::Keyboard, SDL_SCANCODE_D}},
    {static_cast<int>(GubsyButton::KB_E), {DeviceInputKind::Keyboard, SDL_SCANCODE_E}},
    {static_cast<int>(GubsyButton::KB_F), {DeviceInputKind::Keyboard, SDL_SCANCODE_F}},
    {static_cast<int>(GubsyButton::KB_G), {DeviceInputKind::Keyboard, SDL_SCANCODE_G}},
    {static_cast<int>(GubsyButton::KB_H), {DeviceInputKind::Keyboard, SDL_SCANCODE_H}},
    {static_cast<int>(GubsyButton::KB_I), {DeviceInputKind::Keyboard, SDL_SCANCODE_I}},
    {static_cast<int>(GubsyButton::KB_J), {DeviceInputKind::Keyboard, SDL_SCANCODE_J}},
    {static_cast<int>(GubsyButton::KB_K), {DeviceInputKind::Keyboard, SDL_SCANCODE_K}},
    {static_cast<int>(GubsyButton::KB_L), {DeviceInputKind::Keyboard, SDL_SCANCODE_L}},
    {static_cast<int>(GubsyButton::KB_M), {DeviceInputKind::Keyboard, SDL_SCANCODE_M}},
    {static_cast<int>(GubsyButton::KB_N), {DeviceInputKind::Keyboard, SDL_SCANCODE_N}},
    {static_cast<int>(GubsyButton::KB_O), {DeviceInputKind::Keyboard, SDL_SCANCODE_O}},
    {static_cast<int>(GubsyButton::KB_P), {DeviceInputKind::Keyboard, SDL_SCANCODE_P}},
    {static_cast<int>(GubsyButton::KB_Q), {DeviceInputKind::Keyboard, SDL_SCANCODE_Q}},
    {static_cast<int>(GubsyButton::KB_R), {DeviceInputKind::Keyboard, SDL_SCANCODE_R}},
    {static_cast<int>(GubsyButton::KB_S), {DeviceInputKind::Keyboard, SDL_SCANCODE_S}},
    {static_cast<int>(GubsyButton::KB_T), {DeviceInputKind::Keyboard, SDL_SCANCODE_T}},
    {static_cast<int>(GubsyButton::KB_U), {DeviceInputKind::Keyboard, SDL_SCANCODE_U}},
    {static_cast<int>(GubsyButton::KB_V), {DeviceInputKind::Keyboard, SDL_SCANCODE_V}},
    {static_cast<int>(GubsyButton::KB_W), {DeviceInputKind::Keyboard, SDL_SCANCODE_W}},
    {static_cast<int>(GubsyButton::KB_X), {DeviceInputKind::Keyboard, SDL_SCANCODE_X}},
    {static_cast<int>(GubsyButton::KB_Y), {DeviceInputKind::Keyboard, SDL_SCANCODE_Y}},
    {static_cast<int>(GubsyButton::KB_Z), {DeviceInputKind::Keyboard, SDL_SCANCODE_Z}},
    {static_cast<int>(GubsyButton::KB_0), {DeviceInputKind::Keyboard, SDL_SCANCODE_0}},
    {static_cast<int>(GubsyButton::KB_1), {DeviceInputKind::Keyboard, SDL_SCANCODE_1}},
    {static_cast<int>(GubsyButton::KB_2), {DeviceInputKind::Keyboard, SDL_SCANCODE_2}},
    {static_cast<int>(GubsyButton::KB_3), {DeviceInputKind::Keyboard, SDL_SCANCODE_3}},
    {static_cast<int>(GubsyButton::KB_4), {DeviceInputKind::Keyboard, SDL_SCANCODE_4}},
    {static_cast<int>(GubsyButton::KB_5), {DeviceInputKind::Keyboard, SDL_SCANCODE_5}},
    {static_cast<int>(GubsyButton::KB_6), {DeviceInputKind::Keyboard, SDL_SCANCODE_6}},
    {static_cast<int>(GubsyButton::KB_7), {DeviceInputKind::Keyboard, SDL_SCANCODE_7}},
    {static_cast<int>(GubsyButton::KB_8), {DeviceInputKind::Keyboard, SDL_SCANCODE_8}},
    {static_cast<int>(GubsyButton::KB_9), {DeviceInputKind::Keyboard, SDL_SCANCODE_9}},
    {static_cast<int>(GubsyButton::KB_F1), {DeviceInputKind::Keyboard, SDL_SCANCODE_F1}},
    {static_cast<int>(GubsyButton::KB_F2), {DeviceInputKind::Keyboard, SDL_SCANCODE_F2}},
    {static_cast<int>(GubsyButton::KB_F3), {DeviceInputKind::Keyboard, SDL_SCANCODE_F3}},
    {static_cast<int>(GubsyButton::KB_F4), {DeviceInputKind::Keyboard, SDL_SCANCODE_F4}},
    {static_cast<int>(GubsyButton::KB_F5), {DeviceInputKind::Keyboard, SDL_SCANCODE_F5}},
    {static_cast<int>(GubsyButton::KB_F6), {DeviceInputKind::Keyboard, SDL_SCANCODE_F6}},
    {static_cast<int>(GubsyButton::KB_F7), {DeviceInputKind::Keyboard, SDL_SCANCODE_F7}},
    {static_cast<int>(GubsyButton::KB_F8), {DeviceInputKind::Keyboard, SDL_SCANCODE_F8}},
    {static_cast<int>(GubsyButton::KB_F9), {DeviceInputKind::Keyboard, SDL_SCANCODE_F9}},
    {static_cast<int>(GubsyButton::KB_F10), {DeviceInputKind::Keyboard, SDL_SCANCODE_F10}},
    {static_cast<int>(GubsyButton::KB_F11), {DeviceInputKind::Keyboard, SDL_SCANCODE_F11}},
    {static_cast<int>(GubsyButton::KB_F12), {DeviceInputKind::Keyboard, SDL_SCANCODE_F12}},
    {static_cast<int>(GubsyButton::KB_KP_0), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_0}},
    {static_cast<int>(GubsyButton::KB_KP_1), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_1}},
    {static_cast<int>(GubsyButton::KB_KP_2), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_2}},
    {static_cast<int>(GubsyButton::KB_KP_3), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_3}},
    {static_cast<int>(GubsyButton::KB_KP_4), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_4}},
    {static_cast<int>(GubsyButton::KB_KP_5), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_5}},
    {static_cast<int>(GubsyButton::KB_KP_6), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_6}},
    {static_cast<int>(GubsyButton::KB_KP_7), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_7}},
    {static_cast<int>(GubsyButton::KB_KP_8), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_8}},
    {static_cast<int>(GubsyButton::KB_KP_9), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_9}},
    {static_cast<int>(GubsyButton::KB_KP_PERIOD), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_PERIOD}},
    {static_cast<int>(GubsyButton::KB_KP_DIVIDE), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_DIVIDE}},
    {static_cast<int>(GubsyButton::KB_KP_MULTIPLY), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_MULTIPLY}},
    {static_cast<int>(GubsyButton::KB_KP_MINUS), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_MINUS}},
    {static_cast<int>(GubsyButton::KB_KP_PLUS), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_PLUS}},
    {static_cast<int>(GubsyButton::KB_KP_ENTER), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_ENTER}},
    {static_cast<int>(GubsyButton::KB_KP_EQUALS), {DeviceInputKind::Keyboard, SDL_SCANCODE_KP_EQUALS}},
    {static_cast<int>(GubsyButton::KB_UP), {DeviceInputKind::Keyboard, SDL_SCANCODE_UP}},
    {static_cast<int>(GubsyButton::KB_DOWN), {DeviceInputKind::Keyboard, SDL_SCANCODE_DOWN}},
    {static_cast<int>(GubsyButton::KB_LEFT), {DeviceInputKind::Keyboard, SDL_SCANCODE_LEFT}},
    {static_cast<int>(GubsyButton::KB_RIGHT), {DeviceInputKind::Keyboard, SDL_SCANCODE_RIGHT}},
    {static_cast<int>(GubsyButton::KB_LSHIFT), {DeviceInputKind::Keyboard, SDL_SCANCODE_LSHIFT}},
    {static_cast<int>(GubsyButton::KB_RSHIFT), {DeviceInputKind::Keyboard, SDL_SCANCODE_RSHIFT}},
    {static_cast<int>(GubsyButton::KB_LCTRL), {DeviceInputKind::Keyboard, SDL_SCANCODE_LCTRL}},
    {static_cast<int>(GubsyButton::KB_RCTRL), {DeviceInputKind::Keyboard, SDL_SCANCODE_RCTRL}},
    {static_cast<int>(GubsyButton::KB_LALT), {DeviceInputKind::Keyboard, SDL_SCANCODE_LALT}},
    {static_cast<int>(GubsyButton::KB_RALT), {DeviceInputKind::Keyboard, SDL_SCANCODE_RALT}},
    {static_cast<int>(GubsyButton::KB_LGUI), {DeviceInputKind::Keyboard, SDL_SCANCODE_LGUI}},
    {static_cast<int>(GubsyButton::KB_RGUI), {DeviceInputKind::Keyboard, SDL_SCANCODE_RGUI}},
    {static_cast<int>(GubsyButton::KB_ESCAPE), {DeviceInputKind::Keyboard, SDL_SCANCODE_ESCAPE}},
    {static_cast<int>(GubsyButton::KB_TAB), {DeviceInputKind::Keyboard, SDL_SCANCODE_TAB}},
    {static_cast<int>(GubsyButton::KB_CAPSLOCK), {DeviceInputKind::Keyboard, SDL_SCANCODE_CAPSLOCK}},
    {static_cast<int>(GubsyButton::KB_SPACE), {DeviceInputKind::Keyboard, SDL_SCANCODE_SPACE}},
    {static_cast<int>(GubsyButton::KB_BACKSPACE), {DeviceInputKind::Keyboard, SDL_SCANCODE_BACKSPACE}},
    {static_cast<int>(GubsyButton::KB_ENTER), {DeviceInputKind::Keyboard, SDL_SCANCODE_RETURN}},
    {static_cast<int>(GubsyButton::KB_INSERT), {DeviceInputKind::Keyboard, SDL_SCANCODE_INSERT}},
    {static_cast<int>(GubsyButton::KB_DELETE), {DeviceInputKind::Keyboard, SDL_SCANCODE_DELETE}},
    {static_cast<int>(GubsyButton::KB_HOME), {DeviceInputKind::Keyboard, SDL_SCANCODE_HOME}},
    {static_cast<int>(GubsyButton::KB_END), {DeviceInputKind::Keyboard, SDL_SCANCODE_END}},
    {static_cast<int>(GubsyButton::KB_PAGEUP), {DeviceInputKind::Keyboard, SDL_SCANCODE_PAGEUP}},
    {static_cast<int>(GubsyButton::KB_PAGEDOWN), {DeviceInputKind::Keyboard, SDL_SCANCODE_PAGEDOWN}},
    {static_cast<int>(GubsyButton::KB_PRINTSCREEN), {DeviceInputKind::Keyboard, SDL_SCANCODE_PRINTSCREEN}},
    {static_cast<int>(GubsyButton::KB_SCROLLLOCK), {DeviceInputKind::Keyboard, SDL_SCANCODE_SCROLLLOCK}},
    {static_cast<int>(GubsyButton::KB_PAUSE), {DeviceInputKind::Keyboard, SDL_SCANCODE_PAUSE}},
    {static_cast<int>(GubsyButton::KB_MINUS), {DeviceInputKind::Keyboard, SDL_SCANCODE_MINUS}},
    {static_cast<int>(GubsyButton::KB_EQUALS), {DeviceInputKind::Keyboard, SDL_SCANCODE_EQUALS}},
    {static_cast<int>(GubsyButton::KB_LEFTBRACKET), {DeviceInputKind::Keyboard, SDL_SCANCODE_LEFTBRACKET}},
    {static_cast<int>(GubsyButton::KB_RIGHTBRACKET), {DeviceInputKind::Keyboard, SDL_SCANCODE_RIGHTBRACKET}},
    {static_cast<int>(GubsyButton::KB_BACKSLASH), {DeviceInputKind::Keyboard, SDL_SCANCODE_BACKSLASH}},
    {static_cast<int>(GubsyButton::KB_SEMICOLON), {DeviceInputKind::Keyboard, SDL_SCANCODE_SEMICOLON}},
    {static_cast<int>(GubsyButton::KB_APOSTROPHE), {DeviceInputKind::Keyboard, SDL_SCANCODE_APOSTROPHE}},
    {static_cast<int>(GubsyButton::KB_GRAVE), {DeviceInputKind::Keyboard, SDL_SCANCODE_GRAVE}},
    {static_cast<int>(GubsyButton::KB_COMMA), {DeviceInputKind::Keyboard, SDL_SCANCODE_COMMA}},
    {static_cast<int>(GubsyButton::KB_PERIOD), {DeviceInputKind::Keyboard, SDL_SCANCODE_PERIOD}},
    {static_cast<int>(GubsyButton::KB_SLASH), {DeviceInputKind::Keyboard, SDL_SCANCODE_SLASH}},
    {static_cast<int>(GubsyButton::KB_NUMLOCK), {DeviceInputKind::Keyboard, SDL_SCANCODE_NUMLOCKCLEAR}},
    {static_cast<int>(GubsyButton::KB_APPLICATION), {DeviceInputKind::Keyboard, SDL_SCANCODE_APPLICATION}},
    {static_cast<int>(GubsyButton::KB_MENU), {DeviceInputKind::Keyboard, SDL_SCANCODE_MENU}},

    {static_cast<int>(GubsyButton::MOUSE_LEFT), {DeviceInputKind::Mouse, static_cast<int>(mouse_mask(SDL_BUTTON_LEFT))}},
    {static_cast<int>(GubsyButton::MOUSE_RIGHT), {DeviceInputKind::Mouse, static_cast<int>(mouse_mask(SDL_BUTTON_RIGHT))}},
    {static_cast<int>(GubsyButton::MOUSE_MIDDLE), {DeviceInputKind::Mouse, static_cast<int>(mouse_mask(SDL_BUTTON_MIDDLE))}},
    {static_cast<int>(GubsyButton::MOUSE_X1), {DeviceInputKind::Mouse, static_cast<int>(mouse_mask(SDL_BUTTON_X1))}},
    {static_cast<int>(GubsyButton::MOUSE_X2), {DeviceInputKind::Mouse, static_cast<int>(mouse_mask(SDL_BUTTON_X2))}},

    {static_cast<int>(GubsyButton::GP_A), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_A}},
    {static_cast<int>(GubsyButton::GP_B), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_B}},
    {static_cast<int>(GubsyButton::GP_X), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_X}},
    {static_cast<int>(GubsyButton::GP_Y), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_Y}},
    {static_cast<int>(GubsyButton::GP_BACK), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_BACK}},
    {static_cast<int>(GubsyButton::GP_GUIDE), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_GUIDE}},
    {static_cast<int>(GubsyButton::GP_START), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_START}},
    {static_cast<int>(GubsyButton::GP_LEFT_STICK_BUTTON), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_LEFTSTICK}},
    {static_cast<int>(GubsyButton::GP_RIGHT_STICK_BUTTON), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_RIGHTSTICK}},
    {static_cast<int>(GubsyButton::GP_LEFT_SHOULDER), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}},
    {static_cast<int>(GubsyButton::GP_RIGHT_SHOULDER), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}},
    {static_cast<int>(GubsyButton::GP_DPAD_UP), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP}},
    {static_cast<int>(GubsyButton::GP_DPAD_DOWN), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN}},
    {static_cast<int>(GubsyButton::GP_DPAD_LEFT), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT}},
    {static_cast<int>(GubsyButton::GP_DPAD_RIGHT), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}},
    {static_cast<int>(GubsyButton::GP_MISC1), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_MISC1}},
    {static_cast<int>(GubsyButton::GP_PADDLE1), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_PADDLE1}},
    {static_cast<int>(GubsyButton::GP_PADDLE2), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_PADDLE2}},
    {static_cast<int>(GubsyButton::GP_PADDLE3), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_PADDLE3}},
    {static_cast<int>(GubsyButton::GP_PADDLE4), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_PADDLE4}},
    {static_cast<int>(GubsyButton::GP_TOUCHPAD), {DeviceInputKind::Gamepad, SDL_CONTROLLER_BUTTON_TOUCHPAD}},
};

bool decode_extended_button(int encoded, DeviceInputKind& kind, int& device_id, int& code) {
    if ((encoded & kExtendedBindingFlag) == 0)
        return false;
    kind = static_cast<DeviceInputKind>((encoded >> kKindShift) & 0x7);
    int encoded_id = (encoded >> kDeviceShift) & kDeviceMask;
    device_id = (encoded_id == kAnyDeviceEncoded) ? kAnyDeviceId : encoded_id;
    code = encoded & kCodeMask;
    return true;
}

bool decode_extended_analog_1d(int encoded, DeviceInputKind& kind, int& device_id, int& axis) {
    if ((encoded & kExtendedBindingFlag) == 0)
        return false;
    kind = static_cast<DeviceInputKind>((encoded >> kKindShift) & 0x7);
    int encoded_id = (encoded >> kDeviceShift) & kDeviceMask;
    device_id = (encoded_id == kAnyDeviceEncoded) ? kAnyDeviceId : encoded_id;
    axis = encoded & kCodeMask;
    return true;
}

bool decode_extended_analog_2d(int encoded, DeviceInputKind& kind, int& device_id, int& axis_x, int& axis_y) {
    if ((encoded & kExtendedBindingFlag) == 0)
        return false;
    kind = static_cast<DeviceInputKind>((encoded >> kKindShift) & 0x7);
    int encoded_id = (encoded >> kDeviceShift) & kDeviceMask;
    device_id = (encoded_id == kAnyDeviceEncoded) ? kAnyDeviceId : encoded_id;
    int packed_axes = encoded & kCodeMask;
    axis_x = packed_axes & 0xFF;
    axis_y = (packed_axes >> 8) & 0xFF;
    return true;
}

float controller_axis(const DeviceState& state, SDL_GameControllerAxis axis, int device_id) {
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
        return 0.0f;
    auto axis_index = static_cast<size_t>(axis);
    float best = 0.0f;
    for (const auto& controller : state.controllers) {
        if (device_id != kAnyDeviceId && controller.device_id != device_id)
            continue;
        float value = controller.axes[axis_index];
        if (std::fabs(value) > std::fabs(best))
            best = value;
    }
    return best;
}

glm::vec2 controller_stick(const DeviceState& state,
                           SDL_GameControllerAxis axis_x,
                           SDL_GameControllerAxis axis_y,
                           int device_id) {
    float best_length = 0.0f;
    glm::vec2 best(0.0f);
    for (const auto& controller : state.controllers) {
        if (device_id != kAnyDeviceId && controller.device_id != device_id)
            continue;
        if (axis_x < 0 || axis_x >= SDL_CONTROLLER_AXIS_MAX)
            continue;
        if (axis_y < 0 || axis_y >= SDL_CONTROLLER_AXIS_MAX)
            continue;
        auto axis_x_index = static_cast<size_t>(axis_x);
        auto axis_y_index = static_cast<size_t>(axis_y);
        glm::vec2 value(controller.axes[axis_x_index], controller.axes[axis_y_index]);
        float len = glm::length(value);
        if (len > best_length) {
            best_length = len;
            best = value;
        }
    }
    return best;
}

glm::vec2 decode_mouse_axis(int axis_code_x, int axis_code_y, const DeviceState& state) {
    if (axis_code_x == kMouseAxisX && axis_code_y == kMouseAxisY)
        return normalized_mouse_coords(state);
    return glm::vec2(0.0f);
}

} // namespace

int encode_device_button(DeviceInputKind kind, int device_id, int code) {
    int encoded_id = (device_id == kAnyDeviceId) ? kAnyDeviceEncoded
                                                 : (device_id & kDeviceMask);
    return kExtendedBindingFlag |
           (static_cast<int>(kind) << kKindShift) |
           (encoded_id << kDeviceShift) |
           (code & kCodeMask);
}

int encode_device_analog_1d(DeviceInputKind kind, int device_id, int axis_code) {
    int encoded_id = (device_id == kAnyDeviceId) ? kAnyDeviceEncoded
                                                 : (device_id & kDeviceMask);
    return kExtendedBindingFlag |
           (static_cast<int>(kind) << kKindShift) |
           (encoded_id << kDeviceShift) |
           (axis_code & kCodeMask);
}

int encode_device_analog_2d(DeviceInputKind kind, int device_id, int axis_x_code, int axis_y_code) {
    int encoded_id = (device_id == kAnyDeviceId) ? kAnyDeviceEncoded
                                                 : (device_id & kDeviceMask);
    int packed_axes = ((axis_y_code & 0xFF) << 8) | (axis_x_code & 0xFF);
    return kExtendedBindingFlag |
           (static_cast<int>(kind) << kKindShift) |
           (encoded_id << kDeviceShift) |
           (packed_axes & kCodeMask);
}

bool device_button_is_down(const DeviceState& state, int encoded_button) {
    DeviceInputKind kind = DeviceInputKind::Keyboard;
    int device_id = kAnyDeviceId;
    int code = 0;
    if (!decode_extended_button(encoded_button, kind, device_id, code)) {
        auto it = kLegacyButtonMap.find(encoded_button);
        if (it == kLegacyButtonMap.end())
            return false;
        kind = it->second.kind;
        code = it->second.code;
        device_id = kAnyDeviceId;
    }

    switch (kind) {
        case DeviceInputKind::Keyboard:
            if (imgui_want_capture_keyboard() || layout_editor_is_active())
                return false;
            if (code >= 0 && code < SDL_NUM_SCANCODES)
                return state.keyboard[static_cast<std::size_t>(code)] != 0;
            return false;
        case DeviceInputKind::Mouse:
            if (imgui_want_capture_mouse() || layout_editor_is_active())
                return false;
            return (state.mouse_buttons & static_cast<uint32_t>(code)) != 0;
        case DeviceInputKind::Gamepad:
            if (layout_editor_is_active())
                return false;
            if (code < 0 || code >= SDL_CONTROLLER_BUTTON_MAX)
                return false;
            for (const auto& controller : state.controllers) {
                if (device_id != kAnyDeviceId && controller.device_id != device_id)
                    continue;
                if (controller.buttons[static_cast<std::size_t>(code)])
                    return true;
            }
            return false;
    }
    return false;
}

float sample_analog_1d(const DeviceState& state, int encoded_axis) {
    DeviceInputKind kind = DeviceInputKind::Keyboard;
    int device_id = kAnyDeviceId;
    int axis = 0;
    if (!decode_extended_analog_1d(encoded_axis, kind, device_id, axis)) {
        auto legacy = static_cast<Gubsy1DAnalog>(encoded_axis);
        switch (legacy) {
            case Gubsy1DAnalog::GP_LEFT_TRIGGER:
                kind = DeviceInputKind::Gamepad;
                axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
                break;
            case Gubsy1DAnalog::GP_RIGHT_TRIGGER:
                kind = DeviceInputKind::Gamepad;
                axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
                break;
            case Gubsy1DAnalog::MOUSE_WHEEL:
                kind = DeviceInputKind::Mouse;
                axis = kMouseWheelAxis;
                break;
            default:
                return 0.0f;
        }
    }

    switch (kind) {
        case DeviceInputKind::Keyboard:
            if (imgui_want_capture_keyboard() || layout_editor_is_active())
                return 0.0f;
            return 0.0f;
        case DeviceInputKind::Mouse:
            if (imgui_want_capture_mouse() || layout_editor_is_active())
                return 0.0f;
            if (axis == kMouseWheelAxis) {
                float value = static_cast<float>(state.mouse_wheel) * 0.1f;
                return std::clamp(value, -1.0f, 1.0f);
            }
            return 0.0f;
        case DeviceInputKind::Gamepad:
            if (layout_editor_is_active())
                return 0.0f;
            return controller_axis(state, static_cast<SDL_GameControllerAxis>(axis), device_id);
    }
    return 0.0f;
}

glm::vec2 sample_analog_2d(const DeviceState& state, int encoded_axis) {
    DeviceInputKind kind = DeviceInputKind::Keyboard;
    int device_id = kAnyDeviceId;
    int axis_x = 0;
    int axis_y = 0;
    if (!decode_extended_analog_2d(encoded_axis, kind, device_id, axis_x, axis_y)) {
        auto legacy = static_cast<Gubsy2DAnalog>(encoded_axis);
        switch (legacy) {
            case Gubsy2DAnalog::GP_LEFT_STICK:
                kind = DeviceInputKind::Gamepad;
                axis_x = SDL_CONTROLLER_AXIS_LEFTX;
                axis_y = SDL_CONTROLLER_AXIS_LEFTY;
                break;
            case Gubsy2DAnalog::GP_RIGHT_STICK:
                kind = DeviceInputKind::Gamepad;
                axis_x = SDL_CONTROLLER_AXIS_RIGHTX;
                axis_y = SDL_CONTROLLER_AXIS_RIGHTY;
                break;
            case Gubsy2DAnalog::MOUSE_XY:
                kind = DeviceInputKind::Mouse;
                axis_x = kMouseAxisX;
                axis_y = kMouseAxisY;
                break;
            default:
                return glm::vec2(0.0f);
        }
    }

    switch (kind) {
        case DeviceInputKind::Keyboard:
            if (imgui_want_capture_keyboard() || layout_editor_is_active())
                return glm::vec2(0.0f);
            return glm::vec2(0.0f);
        case DeviceInputKind::Mouse:
            if (imgui_want_capture_mouse() || layout_editor_is_active())
                return glm::vec2(0.0f);
            return decode_mouse_axis(axis_x, axis_y, state);
        case DeviceInputKind::Gamepad:
            if (layout_editor_is_active())
                return glm::vec2(0.0f);
            return controller_stick(state,
                                    static_cast<SDL_GameControllerAxis>(axis_x),
                                    static_cast<SDL_GameControllerAxis>(axis_y),
                                    device_id);
    }
    return glm::vec2(0.0f);
}

glm::vec2 normalized_mouse_coords(const DeviceState& state) {
    if (!gg || !gg->renderer)
        return glm::vec2(0.0f);
    glm::ivec2 dims = get_window_dimensions();
    int width = std::max(dims.x, 2);
    int height = std::max(dims.y, 2);
    if (width <= 1)
        width = 2;
    if (height <= 1)
        height = 2;
    float norm_x = (static_cast<float>(state.mouse_x) / static_cast<float>(width - 1)) * 2.0f - 1.0f;
    float norm_y = (static_cast<float>(state.mouse_y) / static_cast<float>(height - 1)) * 2.0f - 1.0f;
    return glm::vec2(std::clamp(norm_x, -1.0f, 1.0f),
                     std::clamp(norm_y, -1.0f, 1.0f));
}

bool mouse_render_position(const DeviceState& state,
                           float render_width,
                           float render_height,
                           float& out_x,
                           float& out_y) {
    if (!gg || !gg->renderer)
        return false;
    if (render_width <= 1.0f || render_height <= 1.0f)
        return false;
    if (gg->render_scale_mode == RenderScaleMode::Stretch) {
        glm::ivec2 dims = get_window_dimensions();
        int width = std::max(dims.x, 2);
        int height = std::max(dims.y, 2);
        float local_x = static_cast<float>(state.mouse_x) / static_cast<float>(width);
        float local_y = static_cast<float>(state.mouse_y) / static_cast<float>(height);
        out_x = std::clamp(local_x, 0.0f, 1.0f) * (render_width - 1.0f);
        out_y = std::clamp(local_y, 0.0f, 1.0f) * (render_height - 1.0f);
        return true;
    }

    int window_w = static_cast<int>(gg->window_dims.x);
    int window_h = static_cast<int>(gg->window_dims.y);
    if (window_w <= 1 || window_h <= 1)
        return false;

    float src_w = render_width;
    float src_h = render_height;
    float src_aspect = src_w / src_h;
    float dst_aspect = static_cast<float>(window_w) / static_cast<float>(window_h);

    float draw_x = 0.0f;
    float draw_y = 0.0f;
    float draw_w = static_cast<float>(window_w);
    float draw_h = static_cast<float>(window_h);

    if (dst_aspect >= src_aspect) {
        draw_h = static_cast<float>(window_h);
        draw_w = draw_h * src_aspect;
        draw_x = (static_cast<float>(window_w) - draw_w) * 0.5f;
    } else {
        draw_w = static_cast<float>(window_w);
        draw_h = draw_w / src_aspect;
        draw_y = (static_cast<float>(window_h) - draw_h) * 0.5f;
    }

    float pad_left = std::clamp(gg->safe_area.x, 0.0f, 0.45f) * static_cast<float>(window_w);
    float pad_right = std::clamp(gg->safe_area.y, 0.0f, 0.45f) * static_cast<float>(window_w);
    float pad_top = std::clamp(gg->safe_area.z, 0.0f, 0.45f) * static_cast<float>(window_h);
    float pad_bottom = std::clamp(gg->safe_area.w, 0.0f, 0.45f) * static_cast<float>(window_h);

    draw_x += pad_left;
    draw_y += pad_top;
    draw_w = std::max(4.0f, draw_w - (pad_left + pad_right));
    draw_h = std::max(4.0f, draw_h - (pad_top + pad_bottom));

    float zoom = std::max(0.1f, gg->preview_zoom);
    float center_x = draw_x + draw_w * 0.5f;
    float center_y = draw_y + draw_h * 0.5f;
    draw_w *= zoom;
    draw_h *= zoom;
    draw_x = center_x - draw_w * 0.5f + gg->preview_pan.x;
    draw_y = center_y - draw_h * 0.5f + gg->preview_pan.y;

    float mouse_x = static_cast<float>(state.mouse_x);
    float mouse_y = static_cast<float>(state.mouse_y);

    if (mouse_x < draw_x || mouse_y < draw_y ||
        mouse_x > draw_x + draw_w || mouse_y > draw_y + draw_h)
        return false;

    float local_x = (mouse_x - draw_x) / draw_w;
    float local_y = (mouse_y - draw_y) / draw_h;

    out_x = std::clamp(local_x, 0.0f, 1.0f) * (render_width - 1.0f);
    out_y = std::clamp(local_y, 0.0f, 1.0f) * (render_height - 1.0f);
    return true;
}

glm::vec2 normalized_mouse_coords_in_render(const DeviceState& state) {
    if (!gg || !gg->renderer)
        return glm::vec2(0.0f);
    float render_w = static_cast<float>(gg->render_dims.x);
    float render_h = static_cast<float>(gg->render_dims.y);
    if (render_w <= 1.0f || render_h <= 1.0f)
        return glm::vec2(0.0f);
    float px = 0.0f;
    float py = 0.0f;
    if (!mouse_render_position(state, render_w, render_h, px, py))
        return glm::vec2(0.0f);
    float norm_x = (px / (render_w - 1.0f)) * 2.0f - 1.0f;
    float norm_y = (py / (render_h - 1.0f)) * 2.0f - 1.0f;
    return glm::vec2(std::clamp(norm_x, -1.0f, 1.0f),
                     std::clamp(norm_y, -1.0f, 1.0f));
}
