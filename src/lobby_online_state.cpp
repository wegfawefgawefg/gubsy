#include "src/engine_state.hpp"
#include "src/alerts.hpp"
#include "src/lobby_state.hpp"
#include "src/room_matchmaking.hpp"
#include "src/user_profiles.hpp"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {

constexpr double kRoomRefreshIntervalSec = 2.0;
constexpr double kRoomHeartbeatIntervalSec = 1.0;

RoomServerMatchmaking g_matchmaking;

IMatchmaking& matchmaking(EngineState& engine) {
    if (engine.lobby_matchmaking != nullptr)
        return *engine.lobby_matchmaking;
    return g_matchmaking;
}

std::string default_room_server_url() {
    if (const char* value = std::getenv("GUB_ROOM_SERVER_URL")) {
        if (*value != '\0')
            return value;
    }
    return "http://127.0.0.1:8788";
}

std::string local_player_name(EngineState& engine) {
    gubsy_lobby_ensure_ready(engine);
    if (UserProfile* profile = gubsy_lobby_user_profile(engine, 0)) {
        if (!profile->name.empty())
            return profile->name;
    }
    return "Player";
}

int lobby_visibility_value(GubsyLobbyVisibility visibility) {
    return visibility == GubsyLobbyVisibility::Public ? 1 : 0;
}

std::string with_prefix(const char* prefix, const std::string& detail) {
    if (detail.empty())
        return prefix;
    return std::string(prefix) + ": " + detail;
}

void set_lobby_error(EngineState& engine, const std::string& message) {
    engine.lobby.status_message = message;
    engine.lobby.last_error = message;
}

void clear_lobby_error(EngineState& engine, const std::string& status) {
    engine.lobby.status_message = status;
    engine.lobby.last_error.clear();
}

void clear_direct_join_pending(EngineState& engine) {
    engine.lobby.direct_join_pending = false;
    engine.lobby.room_join_pending = false;
    engine.lobby.pending_direct_join_endpoint.clear();
    engine.lobby.pending_join_room = MatchmakingRoom{};
}

SessionContract build_lobby_contract(EngineState& engine) {
    SessionContract contract = engine.lobby.contract;
    if (contract.net_protocol.empty())
        contract.net_protocol = session_contract_default_net_protocol();
    if (contract.session_phase.empty())
        contract.session_phase = "lobby";
    contract.realtime_endpoint = engine.lobby.advertised_endpoint;
    GubsyLobbyConfigProvider& provider = engine.lobby_config_provider;
    if (provider.ensure_defaults)
        provider.ensure_defaults(provider.user_data, engine.lobby);
    if (provider.serialize)
        contract.game_config = provider.serialize(provider.user_data, engine.lobby);
    engine.lobby.contract = contract;
    return contract;
}

MatchmakingRoom build_room_metadata(EngineState& engine) {
    gubsy_lobby_ensure_ready(engine);
    MatchmakingRoom room;
    room.room_code = engine.lobby.room_code;
    room.session_name = engine.lobby.lobby_name;
    room.host_name = local_player_name(engine);
    room.privacy = lobby_visibility_value(engine.lobby.visibility);
    room.max_players = std::max(1, engine.lobby.max_players);
    room.current_players = static_cast<int>(engine.lobby.local_players.size());
    room.contract = build_lobby_contract(engine);
    return room;
}

bool parse_endpoint(std::string_view endpoint, std::string& host, std::uint16_t& port) {
    std::size_t colon = endpoint.rfind(':');
    if (colon == std::string_view::npos || colon + 1 >= endpoint.size())
        return false;
    host = std::string(endpoint.substr(0, colon));
    if (host.empty())
        return false;
    try {
        int parsed = std::stoi(std::string(endpoint.substr(colon + 1)));
        if (parsed <= 0 || parsed > 65535)
            return false;
        port = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

void apply_room_to_lobby(EngineState& engine, const MatchmakingRoom& room) {
    engine.lobby.room_code = room.room_code;
    engine.lobby.lobby_name =
        room.session_name.empty() ? engine.lobby.lobby_name : room.session_name;
    engine.lobby.max_players = std::max(1, room.max_players);
    engine.lobby.contract = room.contract;
    engine.lobby.members = room.members;
    engine.lobby.room_current_players = std::max(0, room.current_players);
    engine.lobby.advertised_endpoint = room.contract.realtime_endpoint;
}

std::string member_name(const MatchmakingMember& member) {
    if (!member.display_name.empty())
        return member.display_name;
    if (!member.member_id.empty())
        return member.member_id;
    return "Remote player";
}

std::string member_joined_alert(const MatchmakingMember& member) {
    std::string message = member_name(member) + " joined";
    if (!member.client_label.empty()) {
        message += " from client ";
        message += member.client_label;
    }
    return message;
}

std::string member_left_alert(const MatchmakingMember& member) {
    std::string message = member_name(member) + " left";
    if (!member.client_label.empty()) {
        message += " from client ";
        message += member.client_label;
    }
    return message;
}

const MatchmakingMember* find_member_by_id(const std::vector<MatchmakingMember>& members,
                                           const std::string& member_id) {
    auto it = std::find_if(members.begin(), members.end(), [&](const MatchmakingMember& member) {
        return member.member_id == member_id;
    });
    return it == members.end() ? nullptr : &*it;
}

bool has_direct_members(const std::vector<MatchmakingMember>& members) {
    return std::any_of(members.begin(), members.end(), [](const MatchmakingMember& member) {
        return member.member_id.rfind("direct:", 0) == 0;
    });
}

void update_lobby_members(EngineState& engine, const std::vector<MatchmakingMember>& next_members,
                          bool alert_changes) {
    if (alert_changes) {
        for (const MatchmakingMember& next : next_members) {
            if (next.member_id.empty())
                continue;
            if (!find_member_by_id(engine.lobby.members, next.member_id))
                add_alert(engine, member_joined_alert(next));
        }
        for (const MatchmakingMember& old : engine.lobby.members) {
            if (old.member_id.empty())
                continue;
            if (!find_member_by_id(next_members, old.member_id))
                add_alert(engine, member_left_alert(old));
        }
    }
    engine.lobby.members = next_members;
}

std::optional<MatchmakingRoom> fetch_current_room(EngineState& engine, std::string& err) {
    if (engine.lobby.room_code.empty())
        return std::nullopt;
    MatchmakingRoom room;
    if (!matchmaking(engine).fetch_room(engine.lobby.room_server_url, engine.lobby.room_code, room,
                                        err))
        return std::nullopt;
    return room;
}

void ensure_room_defaults(EngineState& engine) {
    if (engine.lobby.room_server_url.empty())
        engine.lobby.room_server_url = default_room_server_url();
    if (engine.lobby.contract.net_protocol.empty())
        engine.lobby.contract.net_protocol = session_contract_default_net_protocol();
    if (engine.lobby.contract.session_phase.empty())
        engine.lobby.contract.session_phase = "lobby";
}

void disconnect_game_transport(EngineState& engine) {
    if (!engine.lobby_commands.leave)
        return;
    (void)engine.lobby_commands.leave(engine.lobby_commands.leave_user_data, engine.lobby);
}

void force_leave_online_session(EngineState& engine, const std::string& status) {
    disconnect_game_transport(engine);
    engine.lobby.online = false;
    engine.lobby.is_host = false;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.members.clear();
    clear_lobby_error(engine, status);
    add_alert(engine, status);
}

bool validate_room_contract(EngineState& engine, const MatchmakingRoom& room,
                            std::string& message) {
    const SessionContract local = build_lobby_contract(engine);
    const SessionCompatibility compatibility =
        session_contract_check_compatibility(room.contract, local);
    if (compatibility != SessionCompatibility::Compatible) {
        message =
            with_prefix("Cannot join room", session_contract_compatibility_text(compatibility));
        set_lobby_error(engine, message);
        return false;
    }

    GubsyLobbyConfigProvider& provider = engine.lobby_config_provider;
    if (provider.validate_remote &&
        !provider.validate_remote(provider.user_data, engine.lobby, room.contract, message)) {
        message = with_prefix("Cannot join room", message);
        set_lobby_error(engine, message);
        return false;
    }
    return true;
}

bool apply_remote_room_config(EngineState& engine, const MatchmakingRoom& room,
                              std::string& message) {
    GubsyLobbyConfigProvider& provider = engine.lobby_config_provider;
    if (!provider.apply_remote)
        return true;
    if (provider.apply_remote(provider.user_data, engine.lobby, room.contract, message))
        return true;
    message = with_prefix("Cannot apply room config", message);
    set_lobby_error(engine, message);
    return false;
}

bool validate_room_joinable(EngineState& engine, const MatchmakingRoom& room,
                            std::string& message) {
    if (engine.lobby.online && engine.lobby.is_host && !engine.lobby.room_code.empty() &&
        room.room_code == engine.lobby.room_code) {
        message = "Cannot join room: already hosting this room";
        set_lobby_error(engine, message);
        return false;
    }
    if (room.max_players > 0 && room.current_players >= room.max_players) {
        message = "Cannot join room: room is full";
        set_lobby_error(engine, message);
        return false;
    }
    return true;
}

bool leave_existing_session_before_host(EngineState& engine, std::string& message) {
    clear_direct_join_pending(engine);
    if (!engine.lobby.online)
        return true;
    if (gubsy_lobby_leave_room(engine, message))
        return true;
    message = with_prefix("Cannot replace hosted session", message);
    set_lobby_error(engine, message);
    return false;
}

bool leave_existing_session_before_join(EngineState& engine, std::string& message) {
    if (engine.lobby.direct_join_pending) {
        disconnect_game_transport(engine);
        clear_direct_join_pending(engine);
    }
    if (!engine.lobby.online)
        return true;
    if (gubsy_lobby_leave_room(engine, message))
        return true;
    message = with_prefix("Cannot join while leaving current session", message);
    set_lobby_error(engine, message);
    return false;
}

} // namespace

bool gubsy_lobby_host_room(EngineState& engine, std::uint16_t port, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!gubsy_lobby_validate_start(engine, message))
        return false;
    if (!engine.lobby_commands.host) {
        message = "Cannot host room: no host callback registered";
        set_lobby_error(engine, message);
        return false;
    }
    if (!leave_existing_session_before_host(engine, message))
        return false;

    engine.lobby.contract = build_lobby_contract(engine);
    GubsyLobbyHostResult result =
        engine.lobby_commands.host(engine.lobby_commands.host_user_data, engine.lobby, port);
    if (!result.ok) {
        message = result.status.empty() ? "Cannot host room: failed to start transport"
                                        : with_prefix("Cannot host room", result.status);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.advertised_endpoint = result.advertised_endpoint;
    clear_direct_join_pending(engine);
    engine.lobby.network_port = static_cast<int>(port);
    engine.lobby.contract.session_phase = "lobby";
    engine.lobby.contract.realtime_endpoint = engine.lobby.advertised_endpoint;

    MatchmakingCreateResult create_result;
    std::string err;
    MatchmakingRoom room = build_room_metadata(engine);
    if (!matchmaking(engine).create_room(engine.lobby.room_server_url, room, create_result, err)) {
        disconnect_game_transport(engine);
        message = err.empty() ? "Cannot host room: failed to publish room"
                              : with_prefix("Cannot host room", err);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.online = true;
    engine.lobby.is_host = true;
    engine.lobby.room_code = create_result.room_code;
    engine.lobby.host_secret = create_result.host_secret;
    engine.lobby.member_id = create_result.member_id;
    if (auto current_room = fetch_current_room(engine, err))
        apply_room_to_lobby(engine, *current_room);
    clear_lobby_error(engine, "Hosting room " + engine.lobby.room_code);
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
    message = engine.lobby.status_message;
    return true;
}

bool gubsy_lobby_host_direct(EngineState& engine, std::uint16_t port, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!gubsy_lobby_validate_start(engine, message))
        return false;
    if (!engine.lobby_commands.host) {
        message = "Cannot host direct game: no host callback registered";
        set_lobby_error(engine, message);
        return false;
    }
    if (!leave_existing_session_before_host(engine, message))
        return false;

    engine.lobby.contract = build_lobby_contract(engine);
    GubsyLobbyHostResult result =
        engine.lobby_commands.host(engine.lobby_commands.host_user_data, engine.lobby, port);
    if (!result.ok) {
        message = result.status.empty() ? "Cannot host direct game: failed to start transport"
                                        : with_prefix("Cannot host direct game", result.status);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.advertised_endpoint = result.advertised_endpoint;
    clear_direct_join_pending(engine);
    engine.lobby.network_port = static_cast<int>(port);
    engine.lobby.contract.session_phase = "lobby";
    engine.lobby.contract.realtime_endpoint = engine.lobby.advertised_endpoint;
    engine.lobby.visibility = GubsyLobbyVisibility::Private;
    engine.lobby.online = true;
    engine.lobby.is_host = true;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.members.clear();
    clear_lobby_error(engine, "Hosting direct " + engine.lobby.advertised_endpoint);
    message = engine.lobby.status_message;
    return true;
}

bool gubsy_lobby_join_direct(EngineState& engine, const std::string& host, std::uint16_t port,
                             std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (host.empty() || port == 0) {
        message = "Cannot join direct game: invalid host or port";
        set_lobby_error(engine, message);
        return false;
    }
    if (!engine.lobby_commands.join) {
        message = "Cannot join direct game: no join callback registered";
        set_lobby_error(engine, message);
        return false;
    }
    if (!leave_existing_session_before_join(engine, message))
        return false;

    engine.lobby.contract = build_lobby_contract(engine);
    GubsyLobbyJoinResult join_result = engine.lobby_commands.join(
        engine.lobby_commands.join_user_data, engine.lobby, host.c_str(), port);
    if (!join_result.ok) {
        message = join_result.status.empty()
                      ? "Cannot join direct game: failed to reach host"
                      : with_prefix("Cannot join direct game", join_result.status);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.join_host = host;
    engine.lobby.network_port = static_cast<int>(port);
    engine.lobby.advertised_endpoint = host + ":" + std::to_string(port);
    engine.lobby.contract.realtime_endpoint = engine.lobby.advertised_endpoint;
    if (join_result.pending) {
        engine.lobby.online = false;
        engine.lobby.is_host = false;
        engine.lobby.room_code.clear();
        engine.lobby.member_id.clear();
        engine.lobby.host_secret.clear();
        engine.lobby.room_current_players = 0;
        engine.lobby.members.clear();
        engine.lobby.direct_join_pending = true;
        engine.lobby.room_join_pending = false;
        engine.lobby.pending_direct_join_endpoint = engine.lobby.advertised_endpoint;
        clear_lobby_error(engine, join_result.status.empty()
                                      ? "Joining direct " + engine.lobby.advertised_endpoint
                                      : join_result.status);
        message = engine.lobby.status_message;
        return true;
    }

    clear_direct_join_pending(engine);
    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.members.clear();
    clear_lobby_error(engine, "Joined direct " + engine.lobby.advertised_endpoint);
    message = engine.lobby.status_message;
    return true;
}

void gubsy_lobby_confirm_direct_join(EngineState& engine, const std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    if (!engine.lobby.direct_join_pending)
        return;

    if (engine.lobby.room_join_pending) {
        const MatchmakingRoom room = engine.lobby.pending_join_room;
        std::string member_id;
        std::string err;
        if (!matchmaking(engine).join_room(engine.lobby.room_server_url, room.room_code,
                                           local_player_name(engine), member_id, err)) {
            disconnect_game_transport(engine);
            clear_direct_join_pending(engine);
            set_lobby_error(engine, err.empty() ? "Cannot join room: room service rejected join"
                                                : with_prefix("Cannot join room", err));
            add_alert(engine, engine.lobby.status_message, AlertSeverity::Error);
            return;
        }

        apply_room_to_lobby(engine, room);
        engine.lobby.online = true;
        engine.lobby.is_host = false;
        engine.lobby.member_id = member_id;
        engine.lobby.host_secret.clear();
        if (auto current_room = fetch_current_room(engine, err))
            apply_room_to_lobby(engine, *current_room);
        clear_direct_join_pending(engine);
        (void)message;
        clear_lobby_error(engine, "Joined room " + room.room_code);
        engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
        add_alert(engine, engine.lobby.status_message, AlertSeverity::Success);
        return;
    }

    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.members.clear();
    clear_direct_join_pending(engine);
    clear_lobby_error(engine, message.empty()
                                  ? "Joined direct " + engine.lobby.advertised_endpoint
                                  : message);
    add_alert(engine, engine.lobby.status_message, AlertSeverity::Success);
}

void gubsy_lobby_fail_direct_join(EngineState& engine, const std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    if (!engine.lobby.direct_join_pending)
        return;
    engine.lobby.online = false;
    engine.lobby.is_host = false;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.members.clear();
    clear_direct_join_pending(engine);
    set_lobby_error(engine, message.empty() ? "No server found" : message);
    add_alert(engine, engine.lobby.status_message, AlertSeverity::Error);
}

bool gubsy_lobby_join_room(EngineState& engine, const MatchmakingRoom& room, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!validate_room_joinable(engine, room, message))
        return false;
    if (!validate_room_contract(engine, room, message))
        return false;

    std::string host;
    std::uint16_t port = 0;
    if (!parse_endpoint(room.contract.realtime_endpoint, host, port)) {
        message = "Room has no joinable realtime endpoint";
        set_lobby_error(engine, message);
        return false;
    }
    if (!engine.lobby_commands.join) {
        message = "Cannot join room: no join callback registered";
        set_lobby_error(engine, message);
        return false;
    }
    if (!leave_existing_session_before_join(engine, message))
        return false;
    if (!apply_remote_room_config(engine, room, message))
        return false;

    GubsyLobbyJoinResult join_result = engine.lobby_commands.join(
        engine.lobby_commands.join_user_data, engine.lobby, host.c_str(), port);
    if (!join_result.ok) {
        message = join_result.status.empty() ? "Cannot join room: failed to reach host transport"
                                             : with_prefix("Cannot join room", join_result.status);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.join_host = host;
    engine.lobby.network_port = static_cast<int>(port);
    engine.lobby.advertised_endpoint = room.contract.realtime_endpoint;
    engine.lobby.contract = room.contract;
    if (join_result.pending) {
        engine.lobby.online = false;
        engine.lobby.is_host = false;
        engine.lobby.room_code.clear();
        engine.lobby.member_id.clear();
        engine.lobby.host_secret.clear();
        engine.lobby.room_current_players = 0;
        engine.lobby.members.clear();
        engine.lobby.direct_join_pending = true;
        engine.lobby.room_join_pending = true;
        engine.lobby.pending_direct_join_endpoint = engine.lobby.advertised_endpoint;
        engine.lobby.pending_join_room = room;
        clear_lobby_error(engine, join_result.status.empty() ? "Joining room " + room.room_code
                                                             : join_result.status);
        message = engine.lobby.status_message;
        return true;
    }

    std::string member_id;
    std::string err;
    if (!matchmaking(engine).join_room(engine.lobby.room_server_url, room.room_code,
                                       local_player_name(engine), member_id, err)) {
        disconnect_game_transport(engine);
        message = err.empty() ? "Cannot join room: room service rejected join"
                              : with_prefix("Cannot join room", err);
        set_lobby_error(engine, message);
        return false;
    }

    apply_room_to_lobby(engine, room);
    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.member_id = member_id;
    engine.lobby.host_secret.clear();
    if (auto current_room = fetch_current_room(engine, err))
        apply_room_to_lobby(engine, *current_room);
    clear_lobby_error(engine, "Joined room " + room.room_code);
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
    message = engine.lobby.status_message;
    return true;
}

bool gubsy_lobby_join_room_code(EngineState& engine, const std::string& room_code,
                                std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    MatchmakingRoom room;
    std::string err;
    if (!matchmaking(engine).fetch_room(engine.lobby.room_server_url, room_code, room, err)) {
        message = err.empty() ? "Cannot join room: room code not found"
                              : with_prefix("Cannot join room", err);
        set_lobby_error(engine, message);
        return false;
    }
    return gubsy_lobby_join_room(engine, room, message);
}

bool gubsy_lobby_leave_room(EngineState& engine, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!engine.lobby.online) {
        message = "Not in an online session";
        engine.lobby.status_message = message;
        return true;
    }
    if (engine.lobby.room_code.empty()) {
        disconnect_game_transport(engine);
        engine.lobby.online = false;
        engine.lobby.is_host = false;
        engine.lobby.member_id.clear();
        engine.lobby.host_secret.clear();
        engine.lobby.room_current_players = 0;
        engine.lobby.members.clear();
        clear_direct_join_pending(engine);
        clear_lobby_error(engine, "Left direct session");
        message = engine.lobby.status_message;
        return true;
    }
    std::string err;
    if (!matchmaking(engine).leave_room(engine.lobby.room_server_url, engine.lobby.room_code,
                                        engine.lobby.member_id, engine.lobby.host_secret, err)) {
        message = err.empty() ? "Cannot leave room: room service rejected leave"
                              : with_prefix("Cannot leave room", err);
        set_lobby_error(engine, message);
        return false;
    }
    disconnect_game_transport(engine);

    engine.lobby.online = false;
    engine.lobby.is_host = false;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.members.clear();
    clear_direct_join_pending(engine);
    clear_lobby_error(engine, "Left online room");
    message = engine.lobby.status_message;
    return true;
}

bool gubsy_lobby_refresh_rooms(EngineState& engine, bool force, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!force && engine.now < engine.lobby.next_room_refresh_at) {
        message = engine.lobby.status_message;
        return true;
    }

    std::vector<MatchmakingRoom> rooms;
    std::string err;
    if (!matchmaking(engine).list_rooms(engine.lobby.room_server_url, rooms, err)) {
        message = err.empty() ? "Cannot refresh rooms: failed to list rooms"
                              : with_prefix("Cannot refresh rooms", err);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.discovered_rooms = std::move(rooms);
    engine.lobby.browse_room_codes.clear();
    for (const MatchmakingRoom& room : engine.lobby.discovered_rooms)
        engine.lobby.browse_room_codes.push_back(room.room_code);
    clear_lobby_error(engine,
                      std::to_string(engine.lobby.discovered_rooms.size()) + " rooms visible");
    engine.lobby.next_room_refresh_at = engine.now + kRoomRefreshIntervalSec;
    message = engine.lobby.status_message;
    return true;
}

bool gubsy_lobby_remove_room_member(EngineState& engine, const std::string& member_id,
                                    std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!engine.lobby.online || engine.lobby.room_code.empty() || !engine.lobby.is_host ||
        engine.lobby.host_secret.empty()) {
        message = "Only the public room host can kick players";
        set_lobby_error(engine, message);
        return false;
    }
    if (member_id.empty() || member_id == engine.lobby.member_id) {
        message = "Cannot kick that player";
        set_lobby_error(engine, message);
        return false;
    }

    const MatchmakingMember* target = find_member_by_id(engine.lobby.members, member_id);
    const std::string target_name = target ? member_name(*target) : member_id;

    std::string err;
    if (!matchmaking(engine).remove_member(engine.lobby.room_server_url, engine.lobby.room_code,
                                           engine.lobby.host_secret, member_id, err)) {
        message = err.empty() ? "Cannot kick player: room service rejected removal"
                              : with_prefix("Cannot kick player", err);
        set_lobby_error(engine, message);
        return false;
    }

    if (auto current_room = fetch_current_room(engine, err)) {
        engine.lobby.room_current_players = std::max(0, current_room->current_players);
        update_lobby_members(engine, current_room->members, false);
    } else if (!err.empty()) {
        engine.lobby.last_error = err;
    }

    message = "Kicked " + target_name;
    clear_lobby_error(engine, message);
    add_alert(engine, message);
    return true;
}

bool gubsy_lobby_kick_direct_member(EngineState& engine, const MatchmakingMember& member,
                                    std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (!engine.lobby.online || !engine.lobby.is_host) {
        message = "Only the host can kick direct players";
        set_lobby_error(engine, message);
        return false;
    }
    if (member.member_id.empty() || member.member_id == engine.lobby.member_id ||
        member.is_host) {
        message = "Cannot kick that player";
        set_lobby_error(engine, message);
        return false;
    }
    if (!engine.lobby_commands.kick_direct_member) {
        message = "Direct kick is not available for this game";
        set_lobby_error(engine, message);
        return false;
    }

    GubsyLobbyKickResult result = engine.lobby_commands.kick_direct_member(
        engine.lobby_commands.kick_direct_member_user_data, engine.lobby, member);
    if (!result.ok) {
        message = result.status.empty() ? "Cannot kick player" : result.status;
        set_lobby_error(engine, message);
        return false;
    }

    const std::string target_name = member_name(member);
    message = result.status.empty() ? "Kicked " + target_name : result.status;
    clear_lobby_error(engine, message);
    add_alert(engine, message);
    return true;
}

void gubsy_lobby_set_direct_members(EngineState& engine,
                                    const std::vector<MatchmakingMember>& members,
                                    bool alert_changes) {
    gubsy_lobby_ensure_ready(engine);
    if (!engine.lobby.online || (!engine.lobby.room_code.empty() && !engine.lobby.is_host)) {
        return;
    }
    update_lobby_members(engine, members, alert_changes);
    engine.lobby.room_current_players = engine.lobby.room_code.empty()
        ? static_cast<int>(engine.lobby.local_players.size() + engine.lobby.members.size())
        : static_cast<int>(engine.lobby.members.size());
}

void gubsy_lobby_tick_online(EngineState& engine) {
    ensure_room_defaults(engine);
    if (!engine.lobby.online || engine.lobby.room_code.empty())
        return;
    if (engine.now < engine.lobby.next_heartbeat_at)
        return;

    MatchmakingRoom room_update = build_room_metadata(engine);
    MatchmakingRoom* update_ptr = engine.lobby.is_host ? &room_update : nullptr;
    std::string err;
    if (!matchmaking(engine).heartbeat_room(engine.lobby.room_server_url, engine.lobby.room_code,
                                            engine.lobby.member_id, local_player_name(engine),
                                            engine.lobby.host_secret, update_ptr, err)) {
        if (!engine.lobby.is_host && err.find("member not found") != std::string::npos) {
            force_leave_online_session(engine, "Removed from online room");
            engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
            return;
        }
        engine.lobby.last_error = err.empty() ? "Room heartbeat failed" : err;
    } else {
        engine.lobby.last_error.clear();
        if (auto current_room = fetch_current_room(engine, err)) {
            if (!engine.lobby.is_host && !engine.lobby.member_id.empty() &&
                !find_member_by_id(current_room->members, engine.lobby.member_id)) {
                force_leave_online_session(engine, "Removed from online room");
                engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
                return;
            }
            engine.lobby.max_players = std::max(1, current_room->max_players);
            engine.lobby.contract = current_room->contract;
            engine.lobby.advertised_endpoint = current_room->contract.realtime_endpoint;
            engine.lobby.room_current_players = std::max(0, current_room->current_players);
            if (!engine.lobby.is_host || !has_direct_members(engine.lobby.members)) {
                update_lobby_members(engine, current_room->members, true);
            }
        } else if (!err.empty()) {
            engine.lobby.last_error = err;
        }
    }
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
}

void gubsy_lobby_force_online_tick(EngineState& engine) {
    engine.lobby.next_heartbeat_at = 0.0;
    gubsy_lobby_tick_online(engine);
}
