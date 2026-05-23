#include "demo/lobby_config.hpp"

#include <algorithm>
#include <array>

#include "demo/menu/lobby_state.hpp"

namespace {

constexpr std::array<const char*, 4> kScenarioLabels = {
    "Classic Yard",
    "Relay Field",
    "Night Shift",
    "Sandbox",
};

} // namespace

nlohmann::json capture_game_lobby_config(const LobbySession& lobby) {
    return {
        {"scenario_index", lobby.scenario_index},
        {"seed", lobby.seed},
        {"seed_randomized", lobby.seed_randomized},
    };
}

void apply_game_lobby_config(const nlohmann::json& config, LobbySession& lobby) {
    if (!config.is_object())
        return;
    lobby.scenario_index = std::clamp(config.value("scenario_index", lobby.scenario_index),
                                      0,
                                      static_cast<int>(kScenarioLabels.size()) - 1);
    lobby.seed = config.value("seed", lobby.seed);
    lobby.seed_randomized = config.value("seed_randomized", lobby.seed_randomized);
}

std::string describe_game_lobby_config(const LobbySession& lobby) {
    std::string text = std::string(lobby_scenario_label(lobby.scenario_index));
    text += " | ";
    if (lobby.seed_randomized) {
        text += "Seed Auto";
    } else if (!lobby.seed.empty()) {
        text += "Seed " + lobby.seed;
    } else {
        text += "Seed Fixed";
    }
    return text;
}

const char* lobby_scenario_label(int scenario_index) {
    const int clamped = std::clamp(scenario_index, 0, static_cast<int>(kScenarioLabels.size()) - 1);
    return kScenarioLabels[static_cast<std::size_t>(clamped)];
}
