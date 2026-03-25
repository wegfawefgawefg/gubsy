#include "engine/mode_registry.hpp"
#include "engine/globals.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

void register_mode(const std::string& name,
                   void (*step_fn)(),
                   void (*process_inputs_fn)(),
                   void (*render_fn)()) {
    auto it = es->mode_lookup.find(name);
    if (it != es->mode_lookup.end()) {
        es->modes[it->second] = ModeDesc{name, step_fn, process_inputs_fn, render_fn};
        return;
    }
    es->modes.push_back(ModeDesc{name, step_fn, process_inputs_fn, render_fn});
    es->mode_lookup[name] = es->modes.size() - 1;
}

const ModeDesc* find_mode(const std::string& name) {
    auto it = es->mode_lookup.find(name);
    if (it == es->mode_lookup.end())
        return nullptr;
    return &es->modes[it->second];
}
