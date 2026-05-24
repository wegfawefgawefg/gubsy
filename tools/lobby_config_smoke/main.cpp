#include "gubsy/runtime.hpp"
#include "src/gubsy_runtime_internal.hpp"
#include "src/lobby_state.hpp"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

struct SmokeState {
    bool reject_remote{true};
    bool validate_remote_called{false};
    bool apply_remote_called{false};
    bool join_called{false};
};

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool validate_remote_config(void* user_data,
                            const GubsyLobbyState&,
                            const SessionContract& remote,
                            std::string& message) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->validate_remote_called = true;
    require(remote.game_config.value("mode", "") == "campaign", "missing remote campaign config");
    if (state->reject_remote) {
        message = "remote config rejected by smoke";
        return false;
    }
    return true;
}

bool apply_remote_config(void* user_data,
                         GubsyLobbyState&,
                         const SessionContract& remote,
                         std::string&) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->apply_remote_called = true;
    require(remote.game_config.value("mode", "") == "campaign", "apply saw wrong config");
    return true;
}

GubsyLobbyJoinResult join_transport(void* user_data,
                                    const GubsyLobbyState&,
                                    const char*,
                                    std::uint16_t) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->join_called = true;
    GubsyLobbyJoinResult result;
    result.status = "smoke stops before real room service";
    return result;
}

MatchmakingRoom make_room() {
    MatchmakingRoom room;
    room.room_code = "SMOKE1";
    room.session_name = "Smoke";
    room.host_name = "Host";
    room.max_players = 4;
    room.current_players = 1;
    room.contract.net_protocol = session_contract_default_net_protocol();
    room.contract.session_phase = "lobby";
    room.contract.realtime_endpoint = "127.0.0.1:35355";
    room.contract.game_config = {
        {"mode", "campaign"},
        {"respawn_policy", "next_level"},
    };
    return room;
}

void install_smoke_hooks(GubsyRuntime& runtime, SmokeState& state) {
    GubsyLobbyConfigProvider provider;
    provider.user_data = &state;
    provider.validate_remote = validate_remote_config;
    provider.apply_remote = apply_remote_config;
    gubsy_set_lobby_config_provider(runtime, provider);

    GubsyLobbyCommands commands;
    commands.join = join_transport;
    commands.join_user_data = &state;
    gubsy_set_lobby_commands(runtime, commands);
}

} // namespace

int main() {
    try {
        GubsyRuntime runtime;
        GubsyAppConfig config;
        config.enable_mods = false;
        config.project_root = std::filesystem::current_path().string();
        config.data_root =
            (std::filesystem::current_path() / "build" / "lobby_config_smoke_data").string();
        require(init_gubsy_runtime(runtime, config), "failed to init runtime");
        EngineState& engine = gubsy_runtime_engine(runtime);

        SmokeState state;
        install_smoke_hooks(runtime, state);
        std::string message;
        require(!gubsy_lobby_join_room(engine, make_room(), message),
                "join should fail when remote config is rejected");
        require(state.validate_remote_called, "remote validator was not called");
        require(!state.apply_remote_called, "remote config applied after validation rejection");
        require(!state.join_called, "join transport called after validation rejection");
        require(engine.lobby.last_error == "Cannot join room: remote config rejected by smoke",
                "unexpected rejected-config error message");

        state.reject_remote = false;
        state.validate_remote_called = false;
        state.apply_remote_called = false;
        state.join_called = false;
        require(!gubsy_lobby_join_room(engine, make_room(), message),
                "smoke join should stop at fake transport");
        require(state.validate_remote_called, "remote validator was not called on accepted config");
        require(state.apply_remote_called, "accepted remote config was not applied");
        require(state.join_called, "join transport was not called after accepted config");
        require(engine.lobby.last_error == "Cannot join room: smoke stops before real room service",
                "unexpected accepted-config transport error message");

        cleanup_gubsy_runtime(runtime);
        std::puts("[lobby_config_smoke] ok");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[lobby_config_smoke] %s\n", e.what());
        return 1;
    }
}
