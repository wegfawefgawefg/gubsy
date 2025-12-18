/*
    State.hpp/.cpp is for game state. 
    State unique to this game, not the engine.
*/ 

#pragma once

#include "modes.hpp"
#include "settings.hpp"
#include "engine/alerts.hpp"

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

struct DemoPlayer {
    glm::vec2 pos{0.0f, 0.0f};
    glm::vec2 half_size{PLAYER_HALF_SIZE_UNITS, PLAYER_HALF_SIZE_UNITS};
    float speed_units_per_sec{PLAYER_MOVE_SPEED_UNITS};
};
struct BonkTarget {
    glm::vec2 pos{2.5f, 0.0f};
    glm::vec2 half_size{BONK_HALF_SIZE_UNITS, BONK_HALF_SIZE_UNITS};
    float cooldown{0.0f};
    std::string sound_key{"base:ui_confirm"};
    bool enabled{true};
};

struct State {
    std::vector<DemoPlayer> players;
    BonkTarget bonk{};
    float bar_height{0.5f};            // 0.0 to 1.0, normalized
    glm::vec2 reticle_pos{0.0f, 0.0f}; // Combined reticle (legacy/merged)
    glm::vec2 reticle_pos_gamepad{0.0f, 0.0f}; // Raw gamepad stick [-1, 1]
    glm::vec2 reticle_pos_mouse{0.0f, 0.0f};   // Raw mouse normalized [-1, 1]
};

bool init_state();
void cleanup_state();



