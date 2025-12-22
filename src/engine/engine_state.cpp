#include "engine_state.hpp"
#include "globals.hpp"
#include "engine/settings_defaults.hpp"

bool init_engine_state() {
    if (es)
        return true;
    es = new EngineState{};
    es->menu_manager.set_command_registry(&es->menu_commands);
    register_engine_settings_schema_entries();
    return true;
}

void cleanup_engine_state() {
    if (!es)
        return;
    delete es;
    es = nullptr;
}
