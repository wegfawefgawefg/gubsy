#include "engine/binds_ui_helpers.hpp"

#include <algorithm>
#include <string>

#include "engine/input.hpp"

namespace {

std::string keyboard_label(GubsyButton button) {
    int value = static_cast<int>(button);
    int kb_a = static_cast<int>(GubsyButton::KB_A);
    int kb_z = static_cast<int>(GubsyButton::KB_Z);
    if (value >= kb_a && value <= kb_z) {
        char letter = static_cast<char>('A' + (value - kb_a));
        return std::string("Keyboard ") + letter;
    }
    int kb_0 = static_cast<int>(GubsyButton::KB_0);
    int kb_9 = static_cast<int>(GubsyButton::KB_9);
    if (value >= kb_0 && value <= kb_9) {
        char digit = static_cast<char>('0' + (value - kb_0));
        return std::string("Keyboard ") + digit;
    }
    int f1 = static_cast<int>(GubsyButton::KB_F1);
    int f12 = static_cast<int>(GubsyButton::KB_F12);
    if (value >= f1 && value <= f12)
        return "Keyboard F" + std::to_string(value - f1 + 1);

    int kp0 = static_cast<int>(GubsyButton::KB_KP_0);
    int kp9 = static_cast<int>(GubsyButton::KB_KP_9);
    if (value >= kp0 && value <= kp9) {
        char digit = static_cast<char>('0' + (value - kp0));
        return std::string("Keypad ") + digit;
    }

    switch (button) {
        case GubsyButton::KB_KP_PERIOD: return "Keypad .";
        case GubsyButton::KB_KP_DIVIDE: return "Keypad /";
        case GubsyButton::KB_KP_MULTIPLY: return "Keypad *";
        case GubsyButton::KB_KP_MINUS: return "Keypad -";
        case GubsyButton::KB_KP_PLUS: return "Keypad +";
        case GubsyButton::KB_KP_ENTER: return "Keypad Enter";
        case GubsyButton::KB_KP_EQUALS: return "Keypad =";
        case GubsyButton::KB_UP: return "Keyboard Up";
        case GubsyButton::KB_DOWN: return "Keyboard Down";
        case GubsyButton::KB_LEFT: return "Keyboard Left";
        case GubsyButton::KB_RIGHT: return "Keyboard Right";
        case GubsyButton::KB_LSHIFT: return "Keyboard Left Shift";
        case GubsyButton::KB_RSHIFT: return "Keyboard Right Shift";
        case GubsyButton::KB_LCTRL: return "Keyboard Left Ctrl";
        case GubsyButton::KB_RCTRL: return "Keyboard Right Ctrl";
        case GubsyButton::KB_LALT: return "Keyboard Left Alt";
        case GubsyButton::KB_RALT: return "Keyboard Right Alt";
        case GubsyButton::KB_LGUI: return "Keyboard Left Meta";
        case GubsyButton::KB_RGUI: return "Keyboard Right Meta";
        case GubsyButton::KB_ESCAPE: return "Keyboard Escape";
        case GubsyButton::KB_TAB: return "Keyboard Tab";
        case GubsyButton::KB_CAPSLOCK: return "Keyboard Caps Lock";
        case GubsyButton::KB_SPACE: return "Keyboard Space";
        case GubsyButton::KB_BACKSPACE: return "Keyboard Backspace";
        case GubsyButton::KB_ENTER: return "Keyboard Enter";
        case GubsyButton::KB_INSERT: return "Keyboard Insert";
        case GubsyButton::KB_DELETE: return "Keyboard Delete";
        case GubsyButton::KB_HOME: return "Keyboard Home";
        case GubsyButton::KB_END: return "Keyboard End";
        case GubsyButton::KB_PAGEUP: return "Keyboard Page Up";
        case GubsyButton::KB_PAGEDOWN: return "Keyboard Page Down";
        case GubsyButton::KB_PRINTSCREEN: return "Keyboard Print Screen";
        case GubsyButton::KB_SCROLLLOCK: return "Keyboard Scroll Lock";
        case GubsyButton::KB_PAUSE: return "Keyboard Pause";
        case GubsyButton::KB_MINUS: return "Keyboard -";
        case GubsyButton::KB_EQUALS: return "Keyboard =";
        case GubsyButton::KB_LEFTBRACKET: return "Keyboard [";
        case GubsyButton::KB_RIGHTBRACKET: return "Keyboard ]";
        case GubsyButton::KB_BACKSLASH: return "Keyboard \\\\";
        case GubsyButton::KB_SEMICOLON: return "Keyboard ;";
        case GubsyButton::KB_APOSTROPHE: return "Keyboard '";
        case GubsyButton::KB_GRAVE: return "Keyboard `";
        case GubsyButton::KB_COMMA: return "Keyboard ,";
        case GubsyButton::KB_PERIOD: return "Keyboard .";
        case GubsyButton::KB_SLASH: return "Keyboard /";
        case GubsyButton::KB_NUMLOCK: return "Keyboard Num Lock";
        case GubsyButton::KB_APPLICATION: return "Keyboard Application";
        case GubsyButton::KB_MENU: return "Keyboard Menu";
        default: break;
    }
    return "Keyboard";
}

std::string mouse_label(GubsyButton button) {
    switch (button) {
        case GubsyButton::MOUSE_LEFT: return "Mouse Left";
        case GubsyButton::MOUSE_RIGHT: return "Mouse Right";
        case GubsyButton::MOUSE_MIDDLE: return "Mouse Middle";
        case GubsyButton::MOUSE_X1: return "Mouse X1";
        case GubsyButton::MOUSE_X2: return "Mouse X2";
        default: return "Mouse";
    }
}

std::string gamepad_label(GubsyButton button) {
    switch (button) {
        case GubsyButton::GP_A: return "Gamepad A";
        case GubsyButton::GP_B: return "Gamepad B";
        case GubsyButton::GP_X: return "Gamepad X";
        case GubsyButton::GP_Y: return "Gamepad Y";
        case GubsyButton::GP_BACK: return "Gamepad Back";
        case GubsyButton::GP_GUIDE: return "Gamepad Guide";
        case GubsyButton::GP_START: return "Gamepad Start";
        case GubsyButton::GP_LEFT_STICK_BUTTON: return "Gamepad L3";
        case GubsyButton::GP_RIGHT_STICK_BUTTON: return "Gamepad R3";
        case GubsyButton::GP_LEFT_SHOULDER: return "Gamepad LB";
        case GubsyButton::GP_RIGHT_SHOULDER: return "Gamepad RB";
        case GubsyButton::GP_DPAD_UP: return "D-Pad Up";
        case GubsyButton::GP_DPAD_DOWN: return "D-Pad Down";
        case GubsyButton::GP_DPAD_LEFT: return "D-Pad Left";
        case GubsyButton::GP_DPAD_RIGHT: return "D-Pad Right";
        case GubsyButton::GP_MISC1: return "Gamepad Misc";
        case GubsyButton::GP_PADDLE1: return "Gamepad Paddle 1";
        case GubsyButton::GP_PADDLE2: return "Gamepad Paddle 2";
        case GubsyButton::GP_PADDLE3: return "Gamepad Paddle 3";
        case GubsyButton::GP_PADDLE4: return "Gamepad Paddle 4";
        case GubsyButton::GP_TOUCHPAD: return "Gamepad Touchpad";
        default: return "Gamepad";
    }
}

std::string button_label(GubsyButton button) {
    int value = static_cast<int>(button);
    int kb_min = static_cast<int>(GubsyButton::KB_A);
    int kb_max = static_cast<int>(GubsyButton::KB_MENU);
    if (value >= kb_min && value <= kb_max)
        return keyboard_label(button);

    int mouse_min = static_cast<int>(GubsyButton::MOUSE_LEFT);
    int mouse_max = static_cast<int>(GubsyButton::MOUSE_X2);
    if (value >= mouse_min && value <= mouse_max)
        return mouse_label(button);

    return gamepad_label(button);
}

} // namespace

const std::vector<InputChoice>& binds_input_choices(BindsActionType type) {
    static std::vector<InputChoice> button_choices;
    static std::vector<std::string> button_labels;
    static std::vector<InputChoice> analog1d_choices;
    static std::vector<std::string> analog1d_labels;
    static std::vector<InputChoice> analog2d_choices;
    static std::vector<std::string> analog2d_labels;

    auto build_button_choices = []() {
        button_labels.clear();
        button_choices.clear();
        int count = static_cast<int>(GubsyButton::COUNT);
        button_labels.reserve(static_cast<std::size_t>(count));
        button_choices.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            auto button = static_cast<GubsyButton>(i);
            button_labels.push_back(button_label(button));
            button_choices.push_back(InputChoice{i, button_labels.back().c_str()});
        }
    };

    auto build_analog1d_choices = []() {
        analog1d_labels.clear();
        analog1d_choices.clear();
        int count = static_cast<int>(Gubsy1DAnalog::COUNT);
        analog1d_labels.reserve(static_cast<std::size_t>(count));
        analog1d_choices.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            auto axis = static_cast<Gubsy1DAnalog>(i);
            switch (axis) {
                case Gubsy1DAnalog::GP_LEFT_TRIGGER: analog1d_labels.emplace_back("Left Trigger"); break;
                case Gubsy1DAnalog::GP_RIGHT_TRIGGER: analog1d_labels.emplace_back("Right Trigger"); break;
                case Gubsy1DAnalog::MOUSE_WHEEL: analog1d_labels.emplace_back("Mouse Wheel"); break;
                default: analog1d_labels.emplace_back("Analog 1D"); break;
            }
            analog1d_choices.push_back(InputChoice{i, analog1d_labels.back().c_str()});
        }
    };

    auto build_analog2d_choices = []() {
        analog2d_labels.clear();
        analog2d_choices.clear();
        int count = static_cast<int>(Gubsy2DAnalog::COUNT);
        analog2d_labels.reserve(static_cast<std::size_t>(count));
        analog2d_choices.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            auto axis = static_cast<Gubsy2DAnalog>(i);
            switch (axis) {
                case Gubsy2DAnalog::GP_LEFT_STICK: analog2d_labels.emplace_back("Left Stick"); break;
                case Gubsy2DAnalog::GP_RIGHT_STICK: analog2d_labels.emplace_back("Right Stick"); break;
                case Gubsy2DAnalog::MOUSE_XY: analog2d_labels.emplace_back("Mouse XY"); break;
                default: analog2d_labels.emplace_back("Analog 2D"); break;
            }
            analog2d_choices.push_back(InputChoice{i, analog2d_labels.back().c_str()});
        }
    };

    switch (type) {
        case BindsActionType::Analog1D:
            if (analog1d_choices.empty())
                build_analog1d_choices();
            return analog1d_choices;
        case BindsActionType::Analog2D:
            if (analog2d_choices.empty())
                build_analog2d_choices();
            return analog2d_choices;
        case BindsActionType::Button:
        default:
            if (button_choices.empty())
                build_button_choices();
            return button_choices;
    }
}

std::string binds_input_label(BindsActionType type, int code) {
    switch (type) {
        case BindsActionType::Analog1D: {
            auto axis = static_cast<Gubsy1DAnalog>(code);
            switch (axis) {
                case Gubsy1DAnalog::GP_LEFT_TRIGGER: return "Left Trigger";
                case Gubsy1DAnalog::GP_RIGHT_TRIGGER: return "Right Trigger";
                case Gubsy1DAnalog::MOUSE_WHEEL: return "Mouse Wheel";
                default: return "Analog 1D";
            }
        }
        case BindsActionType::Analog2D: {
            auto axis = static_cast<Gubsy2DAnalog>(code);
            switch (axis) {
                case Gubsy2DAnalog::GP_LEFT_STICK: return "Left Stick";
                case Gubsy2DAnalog::GP_RIGHT_STICK: return "Right Stick";
                case Gubsy2DAnalog::MOUSE_XY: return "Mouse XY";
                default: return "Analog 2D";
            }
        }
        case BindsActionType::Button:
        default:
            return button_label(static_cast<GubsyButton>(code));
    }
}

const char* binds_action_type_label(BindsActionType type) {
    switch (type) {
        case BindsActionType::Analog1D: return "Analog 1D";
        case BindsActionType::Analog2D: return "Analog 2D";
        case BindsActionType::Button:
        default: return "Button";
    }
}
