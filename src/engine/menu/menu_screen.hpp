#pragma once

#include "engine/menu/menu_types.hpp"

#include <cstddef>
#include <functional>
#include <span>
#include <type_traits>
#include <vector>

struct EngineState;
struct State;
struct MenuManager;

struct MenuActionList {
    std::span<MenuAction> items;
};

struct MenuWidgetList {
    std::span<MenuWidget> items;
};

struct BuiltScreen {
    UILayoutId layout{kMenuIdInvalid};
    MenuWidgetList widgets{};
    MenuActionList frame_actions{};
    WidgetId default_focus{kMenuIdInvalid};
};

struct ScreenStateOps {
    std::uint32_t size{0};
    std::uint32_t align{0};
    void (*init)(void*){nullptr};
    void (*destroy)(void*){nullptr};
};

template <typename T>
constexpr ScreenStateOps screen_state_ops() {
    static_assert(std::is_trivially_copyable_v<T> == false || std::is_move_constructible_v<T>,
                  "Menu state types should be movable");
    ScreenStateOps ops;
    ops.size = static_cast<std::uint32_t>(sizeof(T));
    ops.align = static_cast<std::uint32_t>(alignof(T));
    ops.init = [](void* mem) {
        new (mem) T();
    };
    ops.destroy = [](void* mem) {
        reinterpret_cast<T*>(mem)->~T();
    };
    return ops;
}

struct MenuContext {
    EngineState& engine;
    State& game;
    MenuManager& manager;
    int screen_width{0};
    int screen_height{0};
    void* screen_state{nullptr};

    template <typename T>
    T& state() {
        return *reinterpret_cast<T*>(screen_state);
    }
};

using MenuBuildFn = BuiltScreen (*)(MenuContext&);
using MenuTickFn = void (*)(MenuContext&, float);

struct MenuScreenDef {
    MenuScreenId id{kMenuIdInvalid};
    UILayoutId layout{kMenuIdInvalid};
    ScreenStateOps state_ops{};
    MenuBuildFn build{nullptr};
    MenuTickFn tick{nullptr};
};
