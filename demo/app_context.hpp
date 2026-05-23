#pragma once

#include "src/engine_state.hpp"
#include "demo/state.hpp"

struct GameAppContext {
    EngineState* engine{nullptr};
    State state{};
};

bool init_game_app_context(GameAppContext& app);
void shutdown_game_app_context(GameAppContext& app);

State* game_state_from_app_context(void* app_context);
const State* game_state_from_app_context(const void* app_context);
EngineState* engine_state_from_app_context(void* app_context);
const EngineState* engine_state_from_app_context(const void* app_context);
