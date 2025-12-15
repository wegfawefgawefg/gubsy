#pragma once

#include <string>

struct InputProfiles {
    int id;
    std::string name;

    // settings
    // mouse settings
    // keyboard settings
    // gamepad settings

    // binds, mostly mappings of gamepad thing to a gubsy input


    // binds. which are mostly a dict of string to gubsy input type

    // int last_mouse_input_profile_id; // -1 if none
    // int last_keyboard_input_profile_id; // -1 if none
    // int last_gamepad_input_profile_id; // -1 if none
};