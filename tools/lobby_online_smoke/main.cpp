#include "gubsy/runtime.hpp"
#include "src/gubsy_runtime_internal.hpp"
#include "src/lobby_state.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

struct SmokeState {
    bool host_called{false};
    bool join_called{false};
    bool leave_called{false};
    bool validate_remote_called{false};
    bool apply_remote_called{false};
    std::string expected_host{"127.0.0.1"};
    std::uint16_t expected_port{45454};
};

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool has_alert_containing(const EngineState& engine, const std::string& needle) {
    return std::any_of(engine.alerts.begin(), engine.alerts.end(), [&](const Alert& alert) {
        return alert.text.find(needle) != std::string::npos;
    });
}

nlohmann::json serialize_config(void*, const GubsyLobbyState& lobby) {
    nlohmann::json players = nlohmann::json::array();
    for (int i = 0; i < static_cast<int>(lobby.local_players.size()); ++i)
        players.push_back({{"local_index", i}, {"character", "player"}});

    return nlohmann::json{
        {"mode", "campaign"},
        {"respawn_policy", "next_level"},
        {"players", players},
    };
}

bool validate_config(void*, const GubsyLobbyState&, std::string&) {
    return true;
}

bool validate_remote_config(void* user_data, const GubsyLobbyState&, const SessionContract& remote,
                            std::string& message) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->validate_remote_called = true;
    if (!remote.game_config.is_object()) {
        message = "remote config is not an object";
        return false;
    }
    if (remote.game_config.value("mode", "") != "campaign") {
        message = "remote mode is not campaign";
        return false;
    }
    if (remote.game_config.value("respawn_policy", "") != "next_level") {
        message = "remote respawn policy mismatch";
        return false;
    }
    return true;
}

bool apply_remote_config(void* user_data, GubsyLobbyState&, const SessionContract& remote,
                         std::string& message) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->apply_remote_called = true;
    return validate_remote_config(user_data, {}, remote, message);
}

GubsyLobbyHostResult host_transport(void* user_data, const GubsyLobbyState& lobby, std::uint16_t) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->host_called = true;
    require(lobby.contract.game_config.value("mode", "") == "campaign",
            "host saw missing game config");

    GubsyLobbyHostResult result;
    result.ok = true;
    result.status = "host transport started";
    result.advertised_endpoint = state->expected_host + ":" + std::to_string(state->expected_port);
    return result;
}

GubsyLobbyJoinResult join_transport(void* user_data, const GubsyLobbyState&, const char* host,
                                    std::uint16_t port) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->join_called = true;
    require(host != nullptr, "join host is null");
    require(state->expected_host == host, "join host mismatch");
    require(state->expected_port == port, "join port mismatch");

    GubsyLobbyJoinResult result;
    result.ok = true;
    result.status = "join transport connected";
    return result;
}

GubsyLobbyLeaveResult leave_transport(void* user_data, const GubsyLobbyState&) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->leave_called = true;

    GubsyLobbyLeaveResult result;
    result.ok = true;
    result.status = "left transport";
    return result;
}

void install_smoke_hooks(GubsyRuntime& runtime, SmokeState& state) {
    GubsyLobbyConfigProvider provider;
    provider.user_data = &state;
    provider.serialize = serialize_config;
    provider.validate = validate_config;
    provider.validate_remote = validate_remote_config;
    provider.apply_remote = apply_remote_config;
    gubsy_set_lobby_config_provider(runtime, provider);

    GubsyLobbyCommands commands;
    commands.host = host_transport;
    commands.host_user_data = &state;
    commands.join = join_transport;
    commands.join_user_data = &state;
    commands.leave = leave_transport;
    commands.leave_user_data = &state;
    gubsy_set_lobby_commands(runtime, commands);
}

void init_runtime(GubsyRuntime& runtime, const char* data_dir) {
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = std::filesystem::current_path().string();
    config.data_root = (std::filesystem::current_path() / "build" / data_dir).string();
    require(init_gubsy_runtime(runtime, config), "failed to init runtime");
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string server_url = "http://127.0.0.1:8788";
        if (argc > 1)
            server_url = argv[1];

        GubsyRuntime host_runtime;
        GubsyRuntime guest_runtime;
        init_runtime(host_runtime, "lobby_online_smoke_host_data");
        init_runtime(guest_runtime, "lobby_online_smoke_guest_data");

        EngineState& host_engine = gubsy_runtime_engine(host_runtime);
        EngineState& guest_engine = gubsy_runtime_engine(guest_runtime);
        host_engine.lobby.room_server_url = server_url;
        guest_engine.lobby.room_server_url = server_url;

        SmokeState host_state;
        SmokeState guest_state;
        install_smoke_hooks(host_runtime, host_state);
        install_smoke_hooks(guest_runtime, guest_state);

        gubsy_lobby_ensure_ready(host_engine);
        require(!host_engine.lobby.lobby_name.empty(), "default lobby name was not generated");
        require(host_engine.lobby.lobby_name != "Local Game",
                "default lobby name should not be Local Game");
        (void)gubsy_lobby_add_local_player(host_engine);
        std::string message;

        require(gubsy_lobby_host_direct(host_engine, host_state.expected_port, message),
                "host direct failed");
        require(host_state.host_called, "direct host transport was not called");
        require(host_engine.lobby.online, "direct host lobby is not online");
        require(host_engine.lobby.is_host, "direct host lobby is not marked as host");
        require(host_engine.lobby.room_code.empty(), "direct host should not have room code");
        require(host_engine.lobby.advertised_endpoint == "127.0.0.1:45454",
                "direct host advertised endpoint mismatch");

        require(gubsy_lobby_join_direct(guest_engine, guest_state.expected_host,
                                        guest_state.expected_port, message),
                "guest direct join failed");
        require(guest_state.join_called, "direct guest transport was not called");
        require(guest_engine.lobby.online, "direct guest lobby is not online");
        require(!guest_engine.lobby.is_host, "direct guest lobby is marked as host");
        require(guest_engine.lobby.room_code.empty(), "direct guest should not have room code");

        require(gubsy_lobby_leave_room(guest_engine, message), "direct guest leave failed");
        require(guest_state.leave_called, "direct guest leave transport was not called");
        require(gubsy_lobby_leave_room(host_engine, message), "direct host leave failed");
        require(host_state.leave_called, "direct host leave transport was not called");

        host_state.host_called = false;
        host_state.leave_called = false;
        guest_state.join_called = false;
        guest_state.leave_called = false;

        std::string active_room_name = host_engine.lobby.lobby_name;
        host_engine.lobby.visibility = GubsyLobbyVisibility::Public;
        require(gubsy_lobby_host_room(host_engine, host_state.expected_port, message),
                "host room failed");
        require(host_state.host_called, "host transport was not called");
        require(host_engine.lobby.online, "host lobby is not online");
        require(host_engine.lobby.is_host, "host lobby is not marked as host");
        require(!host_engine.lobby.room_code.empty(), "host room code missing");
        require(host_engine.lobby.contract.game_config.value("mode", "") == "campaign",
                "host contract missing game config");
        require(host_engine.lobby.members.size() == 1,
                "host did not fetch initial room membership");

        const std::string old_room_code = host_engine.lobby.room_code;
        host_engine.lobby.lobby_name = "Bright Rehosted Tunnel";
        active_room_name = host_engine.lobby.lobby_name;
        host_state.host_called = false;
        host_state.leave_called = false;
        require(gubsy_lobby_host_room(host_engine, host_state.expected_port, message),
                "rehost room failed");
        require(host_state.leave_called, "rehost did not leave previous room");
        require(host_state.host_called, "rehost did not restart host transport");
        require(host_engine.lobby.online, "rehost lobby is not online");
        require(host_engine.lobby.is_host, "rehost lobby is not marked as host");
        require(!host_engine.lobby.room_code.empty(), "rehost room code missing");
        require(host_engine.lobby.members.size() == 1,
                "rehost did not fetch initial room membership");

        require(gubsy_lobby_refresh_rooms(guest_engine, true, message),
                "guest public room refresh failed");
        if (old_room_code != host_engine.lobby.room_code) {
            auto old_listed =
                std::find_if(guest_engine.lobby.discovered_rooms.begin(),
                             guest_engine.lobby.discovered_rooms.end(),
                             [&](const MatchmakingRoom& room) {
                                 return room.room_code == old_room_code;
                             });
            require(old_listed == guest_engine.lobby.discovered_rooms.end(),
                    "rehost left stale old room listed");
        }
        auto listed_room =
            std::find_if(guest_engine.lobby.discovered_rooms.begin(),
                         guest_engine.lobby.discovered_rooms.end(),
                         [&](const MatchmakingRoom& room) {
                             return room.room_code == host_engine.lobby.room_code;
                         });
        require(listed_room != guest_engine.lobby.discovered_rooms.end(),
                "public hosted room was not listed");
        require(listed_room->session_name == active_room_name,
                "listed room did not keep generated room name");
        require(listed_room->privacy > 0, "listed room was not public");

        require(gubsy_lobby_join_room_code(guest_engine, host_engine.lobby.room_code, message),
                "guest join by room code failed");
        require(guest_state.validate_remote_called, "guest did not validate remote config");
        require(guest_state.apply_remote_called, "guest did not apply remote config");
        require(guest_state.join_called, "guest transport was not called");
        require(guest_engine.lobby.online, "guest lobby is not online");
        require(!guest_engine.lobby.is_host, "guest lobby is marked as host");
        require(guest_engine.lobby.room_code == host_engine.lobby.room_code,
                "guest joined wrong room");
        require(guest_engine.lobby.members.size() == 2,
                "guest did not fetch joined room membership");

        host_engine.now = host_engine.lobby.next_heartbeat_at + 0.1;
        gubsy_lobby_tick_online(host_engine);
        require(host_engine.lobby.members.size() == 2,
                "host did not refresh joined room membership");
        require(has_alert_containing(host_engine, "joined"), "host did not alert member join");

        require(gubsy_lobby_leave_room(guest_engine, message), "guest leave failed");
        require(guest_state.leave_called, "guest leave transport was not called");
        host_engine.now = host_engine.lobby.next_heartbeat_at + 0.1;
        gubsy_lobby_tick_online(host_engine);
        require(host_engine.lobby.members.size() == 1,
                "host did not refresh left room membership");
        require(has_alert_containing(host_engine, "left"), "host did not alert member leave");

        require(gubsy_lobby_leave_room(host_engine, message), "host leave failed");
        require(host_state.leave_called, "host leave transport was not called");

        cleanup_gubsy_runtime(guest_runtime);
        cleanup_gubsy_runtime(host_runtime);
        std::puts("[lobby_online_smoke] ok");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[lobby_online_smoke] %s\n", e.what());
        return 1;
    }
}
