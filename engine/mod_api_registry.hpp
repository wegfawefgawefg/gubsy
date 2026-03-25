#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sol {
class state;
}

struct ModContext;

struct ModApiDescriptor {
    std::string name;
    std::function<void(sol::state&, ModContext&)> bind_lua;
    std::function<void(ModContext&)> on_mod_loaded;
    std::function<void(ModContext&)> on_mod_unloaded;
    std::function<void(ModContext&)> on_mod_reloaded;
    std::function<void(ModContext&)> save_mod_state;
    std::function<void(ModContext&)> load_mod_state;
};

class ModApiRegistry {
public:
    static ModApiRegistry& instance();

    void register_api(ModApiDescriptor desc);
    const ModApiDescriptor* find(std::string_view name) const;
    const std::vector<ModApiDescriptor>& apis() const { return apis_; }

private:
    std::vector<ModApiDescriptor> apis_;
};
