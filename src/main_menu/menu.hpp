#pragma once

#include <string>
#include <vector>

struct RectNdc {
    float x{0}, y{0}, w{0}, h{0};
};

enum struct ButtonKind { Button, Toggle, Slider, OptionCycle };

struct ButtonDesc {
    int id{0};
    RectNdc rect{};
    std::string label;
    ButtonKind kind{ButtonKind::Button};
    float value{0.0f};
    bool enabled{true};
};

struct NavNode { int up{-1}, down{-1}, left{-1}, right{-1}; };

// Layout helpers (normalized)
std::vector<RectNdc> layout_vlist(RectNdc top_left, float item_w, float item_h, float vgap, int count);

// Build buttons for current page (transient per-frame)
std::vector<ButtonDesc> build_menu_buttons(int width, int height);

// Render the menu for current state
void render_menu(int width, int height);

// Step logic for menu interaction
void step_menu_logic(int width, int height);

// Input profiles helpers used outside menu module
bool ensure_active_binds_profile_writeable();
void autosave_active_binds_profile();
bool binds_profile_is_read_only(const std::string& name);
void refresh_binds_profiles();
bool menu_is_text_input_active();
void menu_text_input_append(char c);
void menu_text_input_backspace();
void menu_text_input_submit();
void menu_text_input_cancel();
