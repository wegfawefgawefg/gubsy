#include "engine_state.hpp"
#include "src/audio.hpp"
#include "src/graphics.hpp"
#include "src/mods.hpp"
#include "src/settings_defaults.hpp"
#include "src/menu/screens/settings_hub_screen.hpp"
#include "src/menu/screens/profiles_screen.hpp"
#include "src/menu/screens/binds_profiles_screen.hpp"
#include "src/menu/screens/binds_profile_editor_screen.hpp"
#include "src/menu/screens/binds_action_editor_screen.hpp"
#include "src/menu/screens/binds_choose_input_screen.hpp"
#include "src/menu/settings_category_registry.hpp"
#include "src/menu/screens/mods_screen.hpp"
#include "src/gubsy_runtime_internal.hpp"

GubsyRuntime::GubsyRuntime()
    : engine_(new EngineState()) {
}

GubsyRuntime::~GubsyRuntime() {
    if (engine_)
        cleanup_engine_state(*engine_);
    delete engine_;
}

GubsyRuntime::GubsyRuntime(GubsyRuntime&& other) noexcept
    : engine_(other.engine_) {
    other.engine_ = nullptr;
}

GubsyRuntime& GubsyRuntime::operator=(GubsyRuntime&& other) noexcept {
    if (this == &other)
        return *this;
    if (engine_)
        cleanup_engine_state(*engine_);
    delete engine_;
    engine_ = other.engine_;
    other.engine_ = nullptr;
    return *this;
}

EngineState& gubsy_runtime_engine(GubsyRuntime& runtime) {
    return *runtime.engine_;
}

const EngineState& gubsy_runtime_engine(const GubsyRuntime& runtime) {
    return *runtime.engine_;
}

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

bool init_gubsy_runtime(GubsyRuntime& runtime, const GubsyAppConfig& config) {
    return init_engine_state(gubsy_runtime_engine(runtime), config);
}

void cleanup_gubsy_runtime(GubsyRuntime& runtime) {
    cleanup_engine_state(gubsy_runtime_engine(runtime));
}

const GubsyAppConfig& gubsy_runtime_config(const GubsyRuntime& runtime) {
    return gubsy_runtime_engine(runtime).app_config;
}

bool gubsy_runtime_has_menu_screen(const GubsyRuntime& runtime, MenuScreenId screen_id) {
    return gubsy_runtime_engine(runtime).menu_manager.find_screen(screen_id) != nullptr;
}
