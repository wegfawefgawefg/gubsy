#include "src/player.hpp"

#include "src/engine_state.hpp"
#include "src/binds_profiles.hpp" // Needed for profile creation logic

int add_player(EngineState& engine, int player_index) {
    // For now, we ignore player_index and just add to the end.
    // A more robust implementation would handle resizing the vector if needed,
    // and ensure we don't have duplicate player indices.
    if (!engine.players.empty() && player_index >= 0 &&
        static_cast<std::size_t>(player_index) < engine.players.size()) {
        // Player at this index already exists
        return player_index;
    }
    
    Player player;

    // Find a user profile to assign.
    if (engine.user_profiles_pool.empty()) {
        // If no profiles exist at all, create a default one.
        engine.user_profiles_pool.push_back(create_default_user_profile());
    }

    // Assign the first available user profile. A more robust implementation
    // might allow choosing which profile to assign.
    player.profile = engine.user_profiles_pool[0];
    player.has_active_profile = true;

    // Now, ensure this user profile has a valid binds profile.
    BindsProfile* assigned_binds_profile = nullptr;
    if (player.profile.last_binds_profile_id != -1) {
        for (auto& bp : engine.binds_profiles) {
            if (bp.id == player.profile.last_binds_profile_id) {
                assigned_binds_profile = &bp;
                break;
            }
        }
    }

    // If no valid binds profile was found, reuse or create the default.
    if (!assigned_binds_profile) {
        if (engine.binds_profiles.empty()) {
            engine.binds_profiles.push_back(create_default_binds_profile());
        }
        assigned_binds_profile = &engine.binds_profiles.front();
        
        // Link it to the user profile and save the user profile.
        // We need to update both the player's copy of the profile
        // and the canonical version in the pool.
        player.profile.last_binds_profile_id = assigned_binds_profile->id;
        for (auto& up : engine.user_profiles_pool) {
            if (up.id == player.profile.id) {
                up.last_binds_profile_id = assigned_binds_profile->id;
                save_user_profile(up);
                break;
            }
        }
    }

    // Add the fully configured player.
    engine.players.push_back(player);
    return static_cast<int>(engine.players.size()) - 1;
}

void remove_player(EngineState& engine, int player_index) {
    if (player_index < 0 || player_index >= static_cast<int>(engine.players.size()))
        return;
    engine.players.erase(engine.players.begin() + player_index);
}

UserProfile* get_player_user_profile(EngineState& engine, int player_index) {
    if (player_index < 0 || player_index >= static_cast<int>(engine.players.size())) {
        return nullptr;
    }
    Player& player = engine.players[static_cast<std::size_t>(player_index)];
    return player.has_active_profile ? &player.profile : nullptr;
}

void set_user_profile_for_player(EngineState& engine, int player_index, int user_profile_id) {
    if (player_index < 0 || player_index >= static_cast<int>(engine.players.size())) {
        return;
    }

    // Find the target user profile in the pool
    UserProfile* target_profile = nullptr;
    for (auto& up : engine.user_profiles_pool) {
        if (up.id == user_profile_id) {
            target_profile = &up;
            break;
        }
    }

    if (target_profile) {
        engine.players[static_cast<size_t>(player_index)].profile = *target_profile;
        engine.players[static_cast<size_t>(player_index)].has_active_profile = true;
    }
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
