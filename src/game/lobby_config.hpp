#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct LobbySession;

nlohmann::json capture_game_lobby_config(const LobbySession& lobby);
void apply_game_lobby_config(const nlohmann::json& config, LobbySession& lobby);
std::string describe_game_lobby_config(const LobbySession& lobby);
const char* lobby_scenario_label(int scenario_index);
