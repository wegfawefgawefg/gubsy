#pragma once

#include <string>

struct ModeDesc {
    std::string name;
    void (*step_fn)();
    void (*process_inputs_fn)();
    void (*render_fn)();
};

void register_mode(
    const std::string& name, 
    void (*step_fn)(), 
    void (*process_inputs_fn)(),
    void (*render_fn)()
);

const ModeDesc* find_mode(const std::string& name);
