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
#include "engine/ui_layouts.hpp"
#include "player.hpp"
#include "game/events.hpp"


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

    // Input state
    Uint8 keystate[SDL_NUM_SCANCODES];
    Uint8 last_keystate[SDL_NUM_SCANCODES];

    // Event queue
    std::vector<InputEvent> input_event_queue;

    // Game Controller state
    struct GamepadState {
        float axes[SDL_CONTROLLER_AXIS_MAX];
        float last_axes[SDL_CONTROLLER_AXIS_MAX];
        // We could add buttons here too, but they are handled by the keystate arrays for now
    };
    std::unordered_map<int, SDL_GameController*> open_controllers;
    std::unordered_map<int, GamepadState> gamepad_states;

    std::vector<Alert> alerts{};

    // UI layout pool loaded from disk
    std::vector<UILayout> ui_layouts_pool;

    struct AudioSettings {
        float vol_master{1.0f};
        float vol_music{1.0f};
        float vol_sfx{1.0f};
    } audio_settings;
};

bool init_engine_state();
void cleanup_engine_state();
