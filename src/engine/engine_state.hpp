#pragma once

#include "engine/alerts.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "mode_registry.hpp"
#include "user_profiles.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/input_settings_profiles.hpp"
#include "engine/game_settings.hpp"
#include "engine/top_level_game_settings.hpp"
#include "player.hpp"


struct EngineState {
    bool running{true};
    
    double now{0.0};
    float dt{0.0f};
    float accumulator{0.0f};
    std::uint64_t frame{0};
    
    std::string mode{"none"};
    std::vector<ModeDesc> modes;
    std::unordered_map<std::string, std::size_t> mode_lookup;

    // active players
    std::vector<Player> players;

    // user profiles pool
    std::vector<UserProfile> user_profiles_pool;

    // input devices
    std::vector<InputSource> input_sources;

    // binds profiles pool
    std::vector<BindsProfile> binds_profiles;
    // input settings profiles pool
    std::vector<InputSettingsProfile> input_settings_profiles;
    // game settings pool
    std::vector<GameSettings> game_settings_pool;
    // top-level game settings (singleton, global)
    TopLevelGameSettings top_level_game_settings;

    bool draw_input_device_overlay {false};

    std::vector<Alert> alerts{};

    struct AudioSettings {
        float vol_master{1.0f};
        float vol_music{1.0f};
        float vol_sfx{1.0f};
    } audio_settings;
};

bool init_engine_state();
void cleanup_engine_state();
