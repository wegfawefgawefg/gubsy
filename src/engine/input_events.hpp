/*
    each player has a list of sources
    sources can be keyboard/mouse/gamepad
    each source has its own last input state for edge detection/deltas

    each frame well grab events from sdl
    update each source's input state
    then for each player we can query their sources for is_down/was_pressed

    interface:
        is_down(player_idx, button)
        is_up(player_idx, button)
        was_pressed(player_idx, button)
        was_released(player_idx, button)

 */

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

// int for keyboard/mouse/gamepad 0 1
enum class InputSourceType {
    Keyboard,
    Mouse,
    Gamepad
};

struct DeviceIdentifier {
    int id;
};

struct InputSource {
    InputSourceType type;
    DeviceIdentifier device_id;
};

struct PlayerInputSources {
    // vector of input sources for this player
    std::vector<InputSource> sources;

    // where each source has a corresponding inputdata
    // probably a dict mapping source by identifier to inputdata
    std::unordered_map<DeviceIdentifier, PlayerInputData> source_input_data;
};

struct PlayerInputSourceProfilePair {
    InputSource source;
    InputProfile profile;
};

struct PlayerInputPackage {
    // has a list of source/profile pairs
    std::vector<PlayerInputSources> players;
};



// was_pressed(button)
//     takes in a gubsy input
//     does a primary mapping lookup
//     does a secondary mapping lookup

// is_down(button)
//     takes in a gubsy input
//     does a primary mapping lookup
//     does a secondary mapping lookup

// mouse stuff?
// mouse_motion()
// mouse_pos()
// mouse_window_pos()
