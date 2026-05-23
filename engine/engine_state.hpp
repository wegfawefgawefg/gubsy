#pragma once

#include "engine/alerts.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/device_state.hpp"
#include "engine/game_settings.hpp"
#include "engine/input_settings_profiles.hpp"
#include "engine/input_sources.hpp"
#include "engine/layout_editor/layout_editor_state.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_runtime_state.hpp"
#include "engine/top_level_game_settings.hpp"
#include "engine/ui_layouts.hpp"
#include "mode_registry.hpp"
#include "player.hpp"
#include "user_profiles.hpp"

#include <SDL2/SDL.h>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct ModManager;
struct Audio;
struct Graphics;

struct EngineState {
    bool running{true};
    void* app_context{nullptr};

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
    int selected_binds_profile_id{-1};
    int selected_binds_action_type{0};
    int selected_binds_action_id{-1};
    int selected_binds_mapping_index{-1};
    // input settings profiles pool
    std::vector<InputSettingsProfile> input_settings_profiles;
    // game settings pool
    std::vector<GameSettings> game_settings_pool;
    // top-level game settings (singleton, global)
    TopLevelGameSettings top_level_game_settings;

    bool draw_input_device_overlay{false};

    DeviceState device_state{};

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
    glayout::LayoutStore ui_layouts;

    struct AudioSettings {
        float vol_master{1.0f};
        float vol_music{1.0f};
        float vol_sfx{1.0f};
    } audio_settings;

    MenuManager menu_manager;
    MenuCommandRegistry menu_commands;
    menu_system_internal::MenuRuntimeState menu_runtime{};
    layout_editor_internal::LayoutEditorState layout_editor{};
    ModManager* mod_manager{nullptr};
    Audio* audio{nullptr};
    Graphics* graphics{nullptr};
};

bool init_engine_state(EngineState& engine);
void cleanup_engine_state(EngineState& engine);
