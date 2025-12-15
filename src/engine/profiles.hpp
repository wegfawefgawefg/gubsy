#pragma once

#include <string>

struct UserProfile {
    int id;
    std::string name;
    int last_mouse_input_profile_id; // -1 if none
    int last_keyboard_input_profile_id; // -1 if none
    int last_gamepad_input_profile_id; // -1 if none
};