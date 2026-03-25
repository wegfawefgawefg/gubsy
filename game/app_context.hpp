#pragma once

#include "game/state.hpp"

struct GameAppContext {
    State state{};
};

bool init_game_app_context(GameAppContext& app);
void shutdown_game_app_context(GameAppContext& app);

State* game_state_from_app_context(void* app_context);
const State* game_state_from_app_context(const void* app_context);
