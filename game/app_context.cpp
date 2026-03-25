#include "game/app_context.hpp"

#include "engine/binds_profiles.hpp"
#include "engine/user_profiles.hpp"

bool init_game_app_context(GameAppContext& app) {
    reset_state(app.state);
    if (app.engine) {
        load_user_profiles_pool(*app.engine);
        load_binds_profiles_pool(*app.engine);
    }
    return true;
}

void shutdown_game_app_context(GameAppContext& app) {
    reset_state(app.state);
}

State* game_state_from_app_context(void* app_context) {
    if (!app_context)
        return nullptr;
    return &static_cast<GameAppContext*>(app_context)->state;
}

const State* game_state_from_app_context(const void* app_context) {
    if (!app_context)
        return nullptr;
    return &static_cast<const GameAppContext*>(app_context)->state;
}

EngineState* engine_state_from_app_context(void* app_context) {
    if (!app_context)
        return nullptr;
    return static_cast<GameAppContext*>(app_context)->engine;
}

const EngineState* engine_state_from_app_context(const void* app_context) {
    if (!app_context)
        return nullptr;
    return static_cast<const GameAppContext*>(app_context)->engine;
}
