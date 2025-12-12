#include "engine/mode_registry.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {
std::vector<ModeDesc> g_modes;
std::unordered_map<std::string, std::size_t> g_mode_lookup;
}

void register_mode(const std::string& name,
                   void (*step_fn)(),
                   void (*render_fn)()) {
    auto it = g_mode_lookup.find(name);
    if (it != g_mode_lookup.end()) {
        g_modes[it->second] = ModeDesc{name, step_fn, render_fn};
        return;
    }
    g_modes.push_back(ModeDesc{name, step_fn, render_fn});
    g_mode_lookup[name] = g_modes.size() - 1;
}

const ModeDesc* find_mode(const std::string& name) {
    auto it = g_mode_lookup.find(name);
    if (it == g_mode_lookup.end())
        return nullptr;
    return &g_modes[it->second];
}
