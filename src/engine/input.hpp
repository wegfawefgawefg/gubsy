#pragma once

#include <SDL2/SDL.h>
#include <unordered_map>

// Map from abstract Gubsy buttons to SDL scancodes (keyboard only for now).
extern const std::unordered_map<int, SDL_Scancode> gubsy_to_sdl_scancode;

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

    COUNT
};

enum class Gubsy1DAnalog {
    GP_LEFT_TRIGGER,
    GP_RIGHT_TRIGGER,
    MOUSE_WHEEL,
    COUNT
};

enum class Gubsy2DAnalog {
    GP_LEFT_STICK,
    GP_RIGHT_STICK,
    MOUSE_XY,
    COUNT
};
