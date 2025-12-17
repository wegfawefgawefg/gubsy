#pragma once

#include "engine/graphics.hpp"

#include <SDL2/SDL.h>
#include <unordered_map>

extern const std::unordered_map<int, SDL_Scancode> gubsy_to_sdl_scancode;

struct KeyEdge {
    bool prev = false;
    bool rise = false;
    bool fall = false;

    void update(bool now) {
        rise =  now && !prev;   // false -> true
        fall = !now &&  prev;   // true  -> false
        prev = now;             // commit implicitly
    }

    bool rising()  const { return rise; }
    bool falling() const { return fall; }
};



// enum of all possible inputs across all devices
enum class GubsyButton {
    // Keyboard keys
    KB_A, KB_B, KB_C, KB_D, KB_E, KB_F, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L, KB_M,
    KB_N, KB_O, KB_P, KB_Q, KB_R, KB_S, KB_T, KB_U, KB_V, KB_W, KB_X, KB_Y, KB_Z,
    KB_0, KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8, KB_9,
    KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8, KB_F9, KB_F10, KB_F11, KB_F12,
    
    KB_KP_0, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5, KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9,
    KB_KP_PERIOD, KB_KP_DIVIDE, KB_KP_MULTIPLY, KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_EQUALS,
    KB_UP, KB_DOWN, KB_LEFT, KB_RIGHT,
    KB_LSHIFT, KB_RSHIFT, KB_LCTRL, KB_RCTRL, KB_LALT, KB_RALT, KB_LGUI, KB_RGUI,
    KB_ESCAPE, KB_TAB, KB_CAPSLOCK, KB_SPACE, KB_BACKSPACE, KB_ENTER,
    KB_INSERT, KB_DELETE, KB_HOME, KB_END, KB_PAGEUP, KB_PAGEDOWN,
    KB_PRINTSCREEN, KB_SCROLLLOCK, KB_PAUSE,
    KB_MINUS, KB_EQUALS, KB_LEFTBRACKET, KB_RIGHTBRACKET, KB_BACKSLASH,
    KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_PERIOD, KB_SLASH,
    KB_NUMLOCK, KB_APPLICATION, KB_MENU,    

    // Mouse buttons
    MOUSE_LEFT, MOUSE_MIDDLE, MOUSE_RIGHT,
    MOUSE_X1, MOUSE_X2,
    MOUSE_WHEEL,

    // Gamepad buttons
    GP_LEFT_FACE_LEFT, GP_LEFT_FACE_RIGHT, GP_LEFT_FACE_UP, GP_LEFT_FACE_DOWN, // left right up down
    GP_RIGHT_FACE_LEFT, GP_RIGHT_FACE_RIGHT, GP_RIGHT_FACE_UP, GP_RIGHT_FACE_DOWN, // a b x y
    GP_LEFT_SHOULDER, GP_RIGHT_SHOULDER,
    GP_LEFT_TRIGGER, GP_RIGHT_TRIGGER,
    GP_LEFT_STICK_BUTTON, GP_RIGHT_STICK_BUTTON,
    GP_DPAD_UP, GP_DPAD_DOWN, GP_DPAD_LEFT, GP_DPAD_RIGHT,
    GP_START, GP_BACK, GP_GUIDE,

    COUNT // Sentinel for array sizing or iteration
};

// enum of 1d axes for analog inputs
enum class Gubsy1DAnalog {
    // Gamepad axes
    GP_LEFT_TRIGGER, GP_RIGHT_TRIGGER,

    // Mouse axes
    MOUSE_WHEEL,

    COUNT
};

// enum of axes for 2d analog inputs
enum class Gubsy2DAnalog {
    // Gamepad sticks
    GP_LEFT_STICK, GP_RIGHT_STICK,

    // Mouse
    MOUSE_XY,

    COUNT
};


// for edge detection
struct LastKBInputState {
    // Letters
    bool a, b, c, d, e, f, g, h, i, j, k, l, m;
    bool n, o, p, q, r, s, t, u, v, w, x, y, z;

    // Numbers (main keyboard)
    bool num0, num1, num2, num3, num4, num5, num6, num7, num8, num9;

    // Function keys
    bool f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12;
    bool f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24;

    // Keypad
    bool kp0, kp1, kp2, kp3, kp4, kp5, kp6, kp7, kp8, kp9;
    bool kp_period, kp_divide, kp_multiply, kp_minus, kp_plus, kp_enter, kp_equals;

    // Arrow keys
    bool up, down, left, right;

    // Modifiers
    bool lshift, rshift, lctrl, rctrl, lalt, ralt, lgui, rgui;

    // Special keys
    bool escape, tab, capslock, space, backspace, enter;
    bool insert, delete_key, home, end, pageup, pagedown;
    bool printscreen, scrolllock, pause;

    // Punctuation & symbols
    bool minus, equals, leftbracket, rightbracket, backslash;
    bool semicolon, apostrophe, grave, comma, period, slash;

    // Misc
    bool numlock, application, menu;
};

// for edge detection - mouse
struct LastMouseState {
    // Mouse buttons
    bool mouse_left, mouse_middle, mouse_right;
    bool mouse_x1, mouse_x2; // extra mouse buttons

    // Mouse position
    int mouse_x, mouse_y;
    int mouse_rel_x, mouse_rel_y; // relative motion

    // Mouse wheel
    int wheel_delta; // positive for up, negative for down
};

// for edge detection - gamepad
struct LastGamepadState {
    // Face buttons (Xbox/PlayStation/Nintendo layout agnostic)
    bool button_a, button_b, button_x, button_y; // ABXY / Cross/Circle/Square/Triangle

    // Shoulder buttons
    bool lb, rb; // left/right bumper

    // Triggers (digital state - pressed or not)
    bool lt, rt; // left/right trigger

    // Analog trigger values (0.0 to 1.0)
    float lt_analog, rt_analog;

    // Stick buttons (L3/R3)
    bool stick_left, stick_right;

    // D-pad
    bool dpad_up, dpad_down, dpad_left, dpad_right;

    // System buttons
    bool start, back, guide; // start/options, back/select, Xbox/PS/home button

    // Left stick
    float lstick_x, lstick_y; // -1.0 to 1.0

    // Right stick
    float rstick_x, rstick_y; // -1.0 to 1.0

    // Connection state
    bool connected;
};

// Identifies which type of input device
enum class InputSourceType {
    Keyboard,
    Mouse,
    Gamepad
};

// Identifies a specific device instance
struct DeviceIdentifier {
    int id; 
    // maybe a vidpid later or bluetooth id
};

// Reference to a specific device
struct InputSource {
    InputSourceType type;
    DeviceIdentifier device_id;
};

// Settings/config for a specific input source
struct InputProfile {
    // Gamepad settings
    float stick_deadzone = 0.1f;
    float trigger_threshold = 0.5f;

    // Mouse settings
    float mouse_sensitivity = 1.0f;

    // TODO: Key rebindings could go here too
};

// Holds the current and previous frame state for edge detection
struct SourceInputState {
    InputSourceType type;

    // Current frame state
    LastKBInputState current_kb;
    LastMouseState current_mouse;
    LastGamepadState current_gamepad;

    // Previous frame state for edge detection
    LastKBInputState previous_kb;
    LastMouseState previous_mouse;
    LastGamepadState previous_gamepad;
};

// A source + its profile + its runtime state
struct UserProfileInputProfilePair {
    int user_profile_id;
    int input_profile_id;
};

struct PlayerInputSources{
    std::vector<InputSource> sources;
};

/** dispatches to specific "process_inputs" per mode. */
void process_inputs();

