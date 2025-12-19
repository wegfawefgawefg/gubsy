#include "engine_state.hpp"
#include "globals.hpp"

bool init_engine_state() {
    if (es)
        return true;
    es = new EngineState{};
    es->menu_manager.set_command_registry(&es->menu_commands);
    return true;
}

void cleanup_engine_state() {
    if (!es)
        return;
    delete es;
    es = nullptr;
}
