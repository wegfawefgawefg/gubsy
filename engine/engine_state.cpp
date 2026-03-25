#include "engine_state.hpp"
#include "globals.hpp"
#include "engine/settings_defaults.hpp"
#include "engine/menu/screens/settings_hub_screen.hpp"
#include "engine/menu/screens/profiles_screen.hpp"
#include "engine/menu/screens/binds_profiles_screen.hpp"
#include "engine/menu/screens/binds_profile_editor_screen.hpp"
#include "engine/menu/screens/binds_action_editor_screen.hpp"
#include "engine/menu/screens/binds_choose_input_screen.hpp"
#include "engine/menu/settings_category_registry.hpp"
#include "engine/menu/screens/mods_screen.hpp"

bool init_engine_state() {
    if (es)
        return true;
    es = new EngineState{};
    es->menu_manager.set_command_registry(&es->menu_commands);
    register_engine_settings_schema_entries();
    register_settings_category_screens();
    register_settings_hub_screen();
    register_profiles_screen();
    register_binds_profiles_screen();
    register_binds_profile_editor_screen();
    register_binds_action_editor_screen();
    register_binds_choose_input_screen();
    register_mods_menu_screen();
    return true;
}

void cleanup_engine_state() {
    if (!es)
        return;
    delete es;
    es = nullptr;
}
