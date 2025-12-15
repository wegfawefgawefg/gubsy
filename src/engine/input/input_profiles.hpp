#pragma once

#include <string>
#include "device_settings.hpp"
#include "input.hpp"



struct InputProfiles{
    int id;
    std::string name;

    // int last_mouse_input_profile_id; // -1 if none
    // int last_keyboard_input_profile_id; // -1 if none
    // int last_gamepad_input_profile_id; // -1 if none

    // device settings
    MouseDeviceSettings mouse_settings;
    KeyboardDeviceSettings keyboard_settings;
    GamepadDeviceSettings gamepad_settings;

    void bind_button(int game_button, GubsyButton gubsy_button);
    void bind_analog(int game_axis, GubsyAnalog gubsy_axis);

    // maybe later
    // void bind_analog_threshold_as_button(int game_button, GubsyAnalog gubsy_axis, float threshold, bool above_or_below);
    // void bind_analog_delta_over_time_as_button(int game_button, GubsyAnalog gubsy_axis, float delta_threshold, float time_window);
};

