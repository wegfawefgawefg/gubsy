#pragma once

#include "gubsy/menu/screen.hpp"

#include <vector>

using MenuCommandFn = void (*)(MenuContext&, std::int32_t payload);
using GubsyHostMenuCommandFn = void (*)(void* user_data, std::int32_t payload);

struct MenuCommandRegistry {
    MenuCommandId register_command(MenuCommandFn fn);
    MenuCommandId register_host_command(GubsyHostMenuCommandFn fn, void* user_data);
    void invoke(MenuContext& ctx, MenuCommandId id, std::int32_t payload) const;

private:
    struct Handler {
        MenuCommandFn menu_fn{nullptr};
        GubsyHostMenuCommandFn host_fn{nullptr};
        void* user_data{nullptr};
    };

    std::vector<Handler> handlers_;
};
