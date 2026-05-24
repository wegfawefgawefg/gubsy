#include "src/player.hpp"

#include "src/engine_state.hpp"
#include "src/binds_profiles.hpp" // Needed for profile creation logic
#include "src/lobby_state.hpp"

int add_player(EngineState& engine, int player_index) {
    if (player_index >= 0 && player_index < static_cast<int>(engine.players.size()))
        return player_index;
    return gubsy_lobby_add_local_player(engine);
}

void remove_player(EngineState& engine, int player_index) {
    gubsy_lobby_remove_local_player(engine, player_index);
}

UserProfile* get_player_user_profile(EngineState& engine, int player_index) {
    if (player_index < 0 || player_index >= static_cast<int>(engine.players.size())) {
        return nullptr;
    }
    Player& player = engine.players[static_cast<std::size_t>(player_index)];
    return player.has_active_profile ? &player.profile : nullptr;
}

void set_user_profile_for_player(EngineState& engine, int player_index, int user_profile_id) {
    (void)gubsy_lobby_set_user_profile(engine, player_index, user_profile_id);
}

BindsProfile* get_player_binds_profile(EngineState& engine, int player_index) {
    UserProfile* user_profile = get_player_user_profile(engine, player_index);
    if (!user_profile) {
        return nullptr;
    }

    for (auto& bp : get_binds_profiles_pool(engine)) {
        if (bp.id == user_profile->last_binds_profile_id) {
            return &bp;
        }
    }

    return nullptr;
}
