#pragma once

#include <string>

using ModeCallbackFn = void (*)(void* app_context);

struct ModeDesc {
    std::string name;
    ModeCallbackFn step_fn{nullptr};
    ModeCallbackFn process_inputs_fn{nullptr};
    ModeCallbackFn render_fn{nullptr};
};

void register_mode(
    const std::string& name, 
    ModeCallbackFn step_fn,
    ModeCallbackFn process_inputs_fn,
    ModeCallbackFn render_fn
);

const ModeDesc* find_mode(const std::string& name);
