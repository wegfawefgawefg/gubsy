#include "src/mode_registry.hpp"
#include "src/engine_state.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

void register_mode(EngineState& engine,
                   const std::string& name,
                   ModeCallbackFn step_fn,
                   ModeCallbackFn process_inputs_fn,
                   ModeCallbackFn render_fn) {
    auto it = engine.mode_lookup.find(name);
    if (it != engine.mode_lookup.end()) {
        engine.modes[it->second] = ModeDesc{name, step_fn, process_inputs_fn, render_fn};
        return;
    }
    engine.modes.push_back(ModeDesc{name, step_fn, process_inputs_fn, render_fn});
    engine.mode_lookup[name] = engine.modes.size() - 1;
}

const ModeDesc* find_mode(EngineState& engine, const std::string& name) {
    auto it = engine.mode_lookup.find(name);
    if (it == engine.mode_lookup.end())
        return nullptr;
    return &engine.modes[it->second];
}
