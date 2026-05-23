#include "src/menu/menu_commands.hpp"

MenuCommandId MenuCommandRegistry::register_command(MenuCommandFn fn) {
    if (!fn)
        return kMenuIdInvalid;
    Handler handler;
    handler.menu_fn = fn;
    handlers_.push_back(handler);
    return static_cast<MenuCommandId>(handlers_.size());
}

MenuCommandId MenuCommandRegistry::register_host_command(GubsyHostMenuCommandFn fn,
                                                         void* user_data) {
    if (!fn)
        return kMenuIdInvalid;
    Handler handler;
    handler.host_fn = fn;
    handler.user_data = user_data;
    return static_cast<MenuCommandId>(handlers_.size());
}

void MenuCommandRegistry::invoke(MenuContext& ctx, MenuCommandId id, std::int32_t payload) const {
    if (id == kMenuIdInvalid)
        return;
    std::size_t index = static_cast<std::size_t>(id);
    if (index == 0 || index > handlers_.size())
        return;
    const Handler& handler = handlers_[index - 1];
    if (handler.menu_fn) {
        handler.menu_fn(ctx, payload);
        return;
    }
    if (handler.host_fn)
        handler.host_fn(handler.user_data, payload);
}
