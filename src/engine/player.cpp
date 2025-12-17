#include "engine/player.hpp"

#include "engine/globals.hpp"
#include "engine/binds_profiles.hpp" // Needed for profile creation logic

int add_player(int player_index) {
    if (!es) return -1;
    
    // For now, we ignore player_index and just add to the end.
    // A more robust implementation would handle resizing the vector if needed,
    // and ensure we don't have duplicate player indices.
    if (!es->players.empty() && player_index < es->players.size()) {
        // Player at this index already exists
        return player_index;
    }
    
    Player player;

    // Find a user profile to assign.
    if (es->user_profiles_pool.empty()) {
        // If no profiles exist at all, create a default one.
        es->user_profiles_pool.push_back(create_default_user_profile());
    }

    // Assign the first available user profile. A more robust implementation
    // might allow choosing which profile to assign.
    player.profile = es->user_profiles_pool[0];
    player.has_active_profile = true;

    // Now, ensure this user profile has a valid binds profile.
    BindsProfile* assigned_binds_profile = nullptr;
    if (player.profile.last_binds_profile_id != -1) {
        for (auto& bp : es->binds_profiles) {
            if (bp.id == player.profile.last_binds_profile_id) {
                assigned_binds_profile = &bp;
                break;
            }
        }
    }

    // If no valid binds profile was found, create one.
    if (!assigned_binds_profile) {
        BindsProfile new_binds = create_default_binds_profile();
        es->binds_profiles.push_back(new_binds);
        assigned_binds_profile = &es->binds_profiles.back();
        
        // Link it to the user profile and save the user profile.
        // We need to update both the player's copy of the profile
        // and the canonical version in the pool.
        player.profile.last_binds_profile_id = assigned_binds_profile->id;
        for (auto& up : es->user_profiles_pool) {
            if (up.id == player.profile.id) {
                up.last_binds_profile_id = assigned_binds_profile->id;
                save_user_profile(up);
                break;
            }
        }
    }

    // Add the fully configured player.
    es->players.push_back(player);
    return static_cast<int>(es->players.size()) - 1;
}

void remove_player(int player_index) {
    if (!es) return;
    if (player_index < 0 || player_index >= static_cast<int>(es->players.size())) return;
    es->players.erase(es->players.begin() + player_index);
}

UserProfile* get_player_user_profile(int player_index) {
    if (!es || player_index < 0 || player_index >= static_cast<int>(es->players.size())) {
        return nullptr;
    }
    Player& player = es->players[player_index];
    return player.has_active_profile ? &player.profile : nullptr;
}

void set_user_profile_for_player(int player_index, int user_profile_id) {
    if (!es || player_index < 0 || player_index >= static_cast<int>(es->players.size())) {
        return;
    }

    // Find the target user profile in the pool
    UserProfile* target_profile = nullptr;
    for (auto& up : es->user_profiles_pool) {
        if (up.id == user_profile_id) {
            target_profile = &up;
            break;
        }
    }

    if (target_profile) {
        es->players[player_index].profile = *target_profile;
        es->players[player_index].has_active_profile = true;
    }
}

BindsProfile* get_player_binds_profile(int player_index) {
    UserProfile* user_profile = get_player_user_profile(player_index);
    if (!user_profile) {
        return nullptr;
    }

    for (auto& bp : get_binds_profiles_pool()) {
        if (bp.id == user_profile->last_binds_profile_id) {
            return &bp;
        }
    }

    return nullptr;
}
