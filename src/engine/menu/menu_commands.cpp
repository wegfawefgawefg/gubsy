#include "engine/menu/menu_commands.hpp"

MenuCommandId MenuCommandRegistry::register_command(MenuCommandFn fn) {
    if (!fn)
        return kMenuIdInvalid;
    handlers_.push_back(fn);
    return static_cast<MenuCommandId>(handlers_.size());
}

void MenuCommandRegistry::invoke(MenuContext& ctx, MenuCommandId id, std::int32_t payload) const {
    if (id == kMenuIdInvalid)
        return;
    std::size_t index = static_cast<std::size_t>(id);
    if (index == 0 || index > handlers_.size())
        return;
    if (MenuCommandFn fn = handlers_[index - 1])
        fn(ctx, payload);
}
