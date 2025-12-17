#include "engine/globals.hpp"
#include "state.hpp"

#include "engine/user_profiles.hpp"
#include "engine/binds_profiles.hpp"

bool init_state() {
    ss->bonk.pos = glm::vec2{2.5f, 0.0f};
    ss->bonk.cooldown = 0.0f;
    ss->bonk.enabled = true;

    // Load existing profiles from disk
    load_user_profiles_pool();
    load_binds_profiles_pool();
    
    return true;
}

void cleanup_state() {
    delete ss;
    ss = nullptr;
}
