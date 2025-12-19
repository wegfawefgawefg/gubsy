#pragma once

#include "engine/menu/menu_screen.hpp"

#include <vector>

using MenuCommandFn = void (*)(MenuContext&, std::int32_t payload);

struct MenuCommandRegistry {
    MenuCommandId register_command(MenuCommandFn fn);
    void invoke(MenuContext& ctx, MenuCommandId id, std::int32_t payload) const;

private:
    std::vector<MenuCommandFn> handlers_;
};
