#pragma once

#include <cstdint>
#include <string>
#include <SDL2/SDL.h>
inline constexpr std::uint32_t kMenuIdInvalid = 0;
inline constexpr int kMenuMaxQuickValues = 8;

using MenuScreenId = std::uint32_t;
using WidgetId = std::uint32_t;
using UILayoutId = std::uint32_t;
using UILayoutObjectId = std::uint32_t;
using MenuCommandId = std::uint32_t;

struct MenuStyle {
    std::uint8_t bg_r{24}, bg_g{24}, bg_b{32}, bg_a{255};
    std::uint8_t fg_r{220}, fg_g{220}, fg_b{230}, fg_a{255};
    std::uint8_t focus_r{120}, focus_g{170}, focus_b{255}, focus_a{255};
};

enum class MenuActionType : std::uint8_t {
    None = 0,
    PushScreen,
    PopScreen,
    RequestFocus,
    BeginTextCapture,
    EndTextCapture,
    ToggleBool,
    SetFloat,
    DeltaFloat,
    CycleOption,
    RunCommand,
};

struct MenuAction {
    MenuActionType type{MenuActionType::None};
    std::int32_t a{0};
    std::int32_t b{0};
    float f{0.0f};
    void* ptr{nullptr};

    static MenuAction none() { return {}; }
    static MenuAction push(MenuScreenId id) {
        MenuAction act;
        act.type = MenuActionType::PushScreen;
        act.a = static_cast<std::int32_t>(id);
        return act;
    }
    static MenuAction pop() {
        MenuAction act;
        act.type = MenuActionType::PopScreen;
        return act;
    }
    static MenuAction toggle_bool(bool* value_ptr) {
        MenuAction act;
        act.type = MenuActionType::ToggleBool;
        act.ptr = value_ptr;
        return act;
    }
    static MenuAction delta_float(float* value_ptr, float delta) {
        MenuAction act;
        act.type = MenuActionType::DeltaFloat;
        act.ptr = value_ptr;
        act.f = delta;
        return act;
    }
    static MenuAction set_float(float* value_ptr, float value) {
        MenuAction act;
        act.type = MenuActionType::SetFloat;
        act.ptr = value_ptr;
        act.f = value;
        return act;
    }
    static MenuAction run_command(MenuCommandId cmd, std::int32_t payload = 0) {
        MenuAction act;
        act.type = MenuActionType::RunCommand;
        act.a = static_cast<std::int32_t>(cmd);
        act.b = payload;
        return act;
    }
    static MenuAction request_focus(WidgetId id) {
        MenuAction act;
        act.type = MenuActionType::RequestFocus;
        act.a = static_cast<std::int32_t>(id);
        return act;
    }
};

enum class WidgetType : std::uint8_t {
    Label = 0,
    Button,
    Toggle,
    Slider1D,
    OptionCycle,
    TextInput,
    Card,
};

struct MenuWidget {
    WidgetId id{kMenuIdInvalid};
    WidgetType type{WidgetType::Label};
    UILayoutObjectId slot{kMenuIdInvalid};
    MenuStyle style{};
    const char* label{nullptr};
    const char* secondary{nullptr};
    const char* tertiary{nullptr};
    const char* badge{nullptr};
    SDL_Color badge_color{180, 180, 200, 255};
    const char* placeholder{nullptr};
    void* bind_ptr{nullptr};
    float min{0.0f};
    float max{1.0f};
    int option_count{0};
    int quick_value_count{0};
    float quick_values[kMenuMaxQuickValues]{};
    const char* quick_labels[kMenuMaxQuickValues]{};
    std::string* text_buffer{nullptr};
    int text_max_len{0};

    MenuAction on_select{};
    MenuAction on_left{};
    MenuAction on_right{};
    MenuAction on_back{};

    WidgetId nav_up{kMenuIdInvalid};
    WidgetId nav_down{kMenuIdInvalid};
    WidgetId nav_left{kMenuIdInvalid};
    WidgetId nav_right{kMenuIdInvalid};

    float step{0.0f};
    float display_scale{1.0f};
    float display_offset{0.0f};
    int display_precision{2};
};
