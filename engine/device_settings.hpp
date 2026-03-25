struct MouseDeviceSettings {
    float x_sensitivity{1.0f};
    float y_sensitivity{1.0f};
    float scroll_speed{1.0f};
};

struct GamepadDeviceSettings {
    float stick_deadzone{0.1f};
    float trigger_threshold{0.5f};
};

struct KeyboardDeviceSettings {
    float key_repeat_delay{0.5f};
    float key_repeat_rate{0.1f};
};