#include "game/menu/lobby_online.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include <httplib/httplib.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include <nlohmann/json.hpp>

#include "engine/globals.hpp"
#include "engine/matchmaking.hpp"
#include "engine/mod_host.hpp"
#include "engine/room_matchmaking.hpp"
#include "engine/session_contract.hpp"
#include "engine/sync_session.hpp"
#include "game/lobby_config.hpp"
#include "game/menu/lobby_state.hpp"

namespace {

constexpr double kRoomPollIntervalSec = 1.0;
constexpr double kRoomPublishIntervalSec = 1.0;
constexpr double kRoomsRefreshIntervalSec = 2.0;

RoomServerMatchmaking g_matchmaking;

std::string content_contract_key(const SessionContract& contract) {
    nlohmann::json key = {
        {"game_version", contract.game_version},
        {"net_protocol", contract.net_protocol},
        {"mod_hash", contract.mod_hash},
        {"allow_live_mod_reload", contract.allow_live_mod_reload},
        {"game_config", contract.game_config},
    };
    return key.dump();
}

SessionContract build_local_contract(LobbySession& lobby) {
    SessionContract contract = lobby.online.contract;
    contract.game_version = required_mod_game_version();
    contract.net_protocol = session_contract_default_net_protocol();
    contract.session_phase = lobby_session_phase(lobby);
    contract.mod_hash = lobby_enabled_mod_signature();
    contract.allow_live_mod_reload = true;
    contract.game_config = capture_game_lobby_config(lobby);
    contract.realtime_endpoint = sync_session_advertised_endpoint();

    const std::string key = content_contract_key(contract);
    if (lobby.online.last_published_contract_key.empty()) {
        if (contract.content_revision == 0)
            contract.content_revision = 1;
        lobby.online.last_published_contract_key = key;
    } else if (lobby.online.last_published_contract_key != key) {
        contract.content_revision = std::max<std::uint64_t>(contract.content_revision + 1, 1);
        lobby.online.last_published_contract_key = key;
    }
    lobby.online.contract = contract;
    return contract;
}

SessionContract build_expected_local_contract() {
    SessionContract contract;
    contract.game_version = required_mod_game_version();
    contract.net_protocol = session_contract_default_net_protocol();
    contract.mod_hash = lobby_enabled_mod_signature();
    contract.allow_live_mod_reload = true;
    return contract;
}

MatchmakingRoom build_room_metadata(LobbySession& lobby) {
    MatchmakingRoom room;
    room.room_code = lobby.online.room_code;
    room.session_name = lobby.session_name;
    room.host_name = lobby_local_player_name();
    room.privacy = lobby.privacy;
    room.max_players = lobby.max_players;
    room.contract = build_local_contract(lobby);
    return room;
}

void apply_room_to_lobby(const MatchmakingRoom& room, LobbySession& lobby) {
    lobby.session_name = room.session_name;
    lobby.privacy = room.privacy;
    lobby.max_players = std::max(1, room.max_players);
    lobby.online.contract = room.contract;
}

void read_room_members(const MatchmakingRoom& room, LobbySession& lobby) {
    lobby.online.members.clear();
    for (const auto& member_json : room.members) {
        LobbyOnlineMember member;
        member.member_id = member_json.member_id;
        member.display_name = member_json.display_name;
        member.is_host = member_json.is_host;
        member.is_local = member.member_id == lobby.online.member_id;
        if (!member.member_id.empty())
            lobby.online.members.push_back(std::move(member));
    }
}

bool refresh_room_state(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room || lobby.online.room_code.empty())
        return false;
    MatchmakingRoom room;
    if (!g_matchmaking.fetch_room(lobby.online.server_url, lobby.online.room_code, room, err))
        return false;
    apply_room_to_lobby(room, lobby);
    if (room.contract.game_config.is_object())
        apply_game_lobby_config(room.contract.game_config, lobby);
    read_room_members(room, lobby);
    std::ostringstream status;
    status << (session_contract_is_in_game(room.contract) ? "In Game" : "Lobby")
           << " | Room " << room.room_code << " | " << room.current_players
           << "/" << room.max_players << " players";
    const SessionCompatibility compatibility =
        session_contract_check_compatibility(room.contract, build_expected_local_contract());
    if (compatibility != SessionCompatibility::Compatible) {
        status << " | " << session_contract_compatibility_text(compatibility);
        if (compatibility == SessionCompatibility::WrongGameVersion ||
            compatibility == SessionCompatibility::WrongNetProtocol) {
            lobby.online.last_error = session_contract_compatibility_text(compatibility);
        }
    }
    lobby.online.status_text = status.str();
    return true;
}

bool publish_room_state(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room || !lobby.online.is_host)
        return false;
    MatchmakingRoom room = build_room_metadata(lobby);
    return g_matchmaking.heartbeat_room(lobby.online.server_url,
                                        lobby.online.room_code,
                                        lobby.online.member_id,
                                        lobby_local_player_name(),
                                        lobby.online.host_secret,
                                        &room,
                                        err);
}

bool heartbeat_member(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room || lobby.online.is_host)
        return false;
    return g_matchmaking.heartbeat_room(lobby.online.server_url,
                                        lobby.online.room_code,
                                        lobby.online.member_id,
                                        lobby_local_player_name(),
                                        {},
                                        nullptr,
                                        err);
}

} // namespace

bool lobby_online_host_current_room(LobbySession& lobby, std::string& err) {
    lobby.online.last_published_contract_key.clear();
    MatchmakingCreateResult created;
    MatchmakingRoom room = build_room_metadata(lobby);
    if (!g_matchmaking.create_room(lobby.online.server_url, room, created, err))
        return false;
    lobby.online.in_room = true;
    lobby.online.is_host = true;
    lobby.online.room_code = created.room_code;
    lobby.online.host_secret = created.host_secret;
    lobby.online.member_id = created.member_id;
    lobby.online.contract = room.contract;
    lobby.online.contract.session_phase = "lobby";
    lobby.online.contract.realtime_endpoint.clear();
    lobby.online.next_room_poll_at = 0.0;
    lobby.online.next_room_publish_at = 0.0;
    err.clear();
    return refresh_room_state(lobby, err);
}

bool lobby_online_join_room(LobbySession& lobby, const std::string& room_code, std::string& err) {
    std::string member_id;
    if (!g_matchmaking.join_room(lobby.online.server_url,
                                 room_code,
                                 lobby_local_player_name(),
                                 member_id,
                                 err)) {
        return false;
    }
    lobby.online.in_room = true;
    lobby.online.is_host = false;
    lobby.online.room_code = room_code;
    lobby.online.host_secret.clear();
    lobby.online.member_id = member_id;
    lobby.online.contract = SessionContract{};
    lobby.online.contract.net_protocol = session_contract_default_net_protocol();
    lobby.online.next_room_poll_at = 0.0;
    lobby.online.next_room_publish_at = 0.0;
    err.clear();
    return refresh_room_state(lobby, err);
}

bool lobby_online_leave_room(LobbySession& lobby, std::string& err) {
    if (!lobby.online.in_room)
        return true;
    g_matchmaking.leave_room(lobby.online.server_url,
                             lobby.online.room_code,
                             lobby.online.member_id,
                             lobby.online.is_host ? lobby.online.host_secret : std::string{},
                             err);
    lobby.online.in_room = false;
    lobby.online.is_host = false;
    lobby.online.contract = SessionContract{};
    lobby.online.contract.net_protocol = session_contract_default_net_protocol();
    lobby.online.room_code.clear();
    lobby.online.host_secret.clear();
    lobby.online.member_id.clear();
    lobby.online.last_published_contract_key.clear();
    lobby.online.members.clear();
    lobby.online.status_text = "Offline lobby";
    return true;
}

bool lobby_online_refresh_rooms(LobbySession& lobby, bool force, std::string& err) {
    if (!force && es && es->now < lobby.online.next_rooms_refresh_at)
        return true;
    std::vector<MatchmakingRoom> rooms;
    if (!g_matchmaking.list_rooms(lobby.online.server_url, rooms, err))
        return false;
    lobby.online.discovered_rooms.clear();
    for (const MatchmakingRoom& src : rooms) {
        LobbyDiscoveredRoom room;
        room.room_code = src.room_code;
        room.session_name = src.session_name;
        room.host_name = src.host_name;
        room.privacy = src.privacy;
        room.max_players = src.max_players;
        room.current_players = src.current_players;
        room.contract = src.contract;
        if (!room.room_code.empty())
            lobby.online.discovered_rooms.push_back(std::move(room));
    }
    lobby.online.next_rooms_refresh_at = es ? es->now + kRoomsRefreshIntervalSec : kRoomsRefreshIntervalSec;
    return true;
}

void lobby_online_tick(LobbySession& lobby) {
    if (!es)
        return;
    std::string err;
    if (lobby.online.in_room) {
        if (lobby.online.is_host && es->now >= lobby.online.next_room_publish_at) {
            lobby_refresh_mods();
            if (publish_room_state(lobby, err))
                lobby.online.next_room_publish_at = es->now + kRoomPublishIntervalSec;
            else if (!err.empty())
                lobby.online.last_error = err;
        } else if (!lobby.online.is_host && es->now >= lobby.online.next_room_publish_at) {
            if (heartbeat_member(lobby, err))
                lobby.online.next_room_publish_at = es->now + kRoomPublishIntervalSec;
            else if (!err.empty())
                lobby.online.last_error = err;
        }

        if (es->now >= lobby.online.next_room_poll_at) {
            if (refresh_room_state(lobby, err)) {
                lobby.online.next_room_poll_at = es->now + kRoomPollIntervalSec;
            } else if (!err.empty()) {
                lobby.online.last_error = err;
            }
        }
    }
}
