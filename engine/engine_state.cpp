#include "engine_state.hpp"
#include "engine/audio.hpp"
#include "engine/graphics.hpp"
#include "engine/mods.hpp"
#include "engine/settings_defaults.hpp"
#include "engine/menu/screens/settings_hub_screen.hpp"
#include "engine/menu/screens/profiles_screen.hpp"
#include "engine/menu/screens/binds_profiles_screen.hpp"
#include "engine/menu/screens/binds_profile_editor_screen.hpp"
#include "engine/menu/screens/binds_action_editor_screen.hpp"
#include "engine/menu/screens/binds_choose_input_screen.hpp"
#include "engine/menu/settings_category_registry.hpp"
#include "engine/menu/screens/mods_screen.hpp"

bool init_engine_state(EngineState& engine, const GubsyAppConfig& config) {
    engine.app_config = normalize_gubsy_app_config(config);
    engine.menu_manager.set_command_registry(&engine.menu_commands);
    register_engine_settings_schema_entries(engine);
    register_settings_category_screens(engine);
    register_settings_hub_screen(engine);
    register_profiles_screen(engine);
    register_binds_profiles_screen(engine);
    register_binds_profile_editor_screen(engine);
    register_binds_action_editor_screen(engine);
    register_binds_choose_input_screen(engine);
    if (engine.app_config.enable_mod_browser)
        register_mods_menu_screen(engine);
    return true;
}

void cleanup_engine_state(EngineState& engine) {
    if (engine.mod_manager) {
        delete engine.mod_manager;
        engine.mod_manager = nullptr;
    }
    if (engine.audio)
        cleanup_audio(engine);
    if (engine.graphics)
        cleanup_graphics(engine);
}
