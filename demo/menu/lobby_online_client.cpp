#include "demo/menu/lobby_online.hpp"

#include <algorithm>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "src/engine_state.hpp"
#include "src/matchmaking.hpp"
#include "src/mod_host.hpp"
#include "src/mod_install.hpp"
#include "src/mod_server_config.hpp"
#include "src/room_matchmaking.hpp"
#include "src/session_contract.hpp"
#include "demo/coop_session.hpp"
#include "demo/lobby_config.hpp"
#include "demo/menu/lobby_state.hpp"

namespace {

constexpr double kRoomPollIntervalSec = 1.0;
constexpr double kRoomPublishIntervalSec = 1.0;
constexpr double kRoomsRefreshIntervalSec = 2.0;
constexpr double kContentRetryIntervalSec = 3.0;
constexpr double kRoomReconnectGraceSec = 8.0;

RoomServerMatchmaking g_matchmaking;

void normalize_required_mod_ids(std::vector<std::string>& ids) {
    ids.erase(std::remove_if(ids.begin(), ids.end(),
                             [](const std::string& id) { return id.empty(); }),
              ids.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

void clear_room_failure_state(LobbySession& lobby) {
    lobby.online.room_failure_count = 0;
    lobby.online.first_room_failure_at = 0.0;
    lobby.online.reconnecting = false;
}

void clear_local_room_state(LobbySession& lobby, const char* status_text) {
    lobby.online.in_room = false;
    lobby.online.is_host = false;
    lobby.online.compatibility = SessionCompatibility::Compatible;
    lobby.online.contract = SessionContract{};
    lobby.online.contract.net_protocol = session_contract_default_net_protocol();
    lobby.online.room_code.clear();
    lobby.online.host_secret.clear();
    lobby.online.member_id.clear();
    lobby.online.last_published_contract_key.clear();
    lobby.online.synced_content_revision = 0;
    lobby.online.failed_content_revision = 0;
    lobby.online.next_content_retry_at = 0.0;
    lobby.online.content_status_text.clear();
    clear_room_failure_state(lobby);
    lobby.online.session_closed = false;
    lobby.online.session_close_reason.clear();
    lobby.online.members.clear();
    lobby.online.status_text = status_text;
}

bool room_server_error_is_room_closed(const std::string& err) {
    return err.find("room not found") != std::string::npos ||
           err.find("(404)") != std::string::npos;
}

void mark_session_closed(LobbySession& lobby, const std::string& reason) {
    lobby.online.reconnecting = false;
    lobby.online.session_closed = true;
    lobby.online.session_close_reason = reason;
    lobby.online.status_text = reason;
    lobby.online.last_error = reason;
}

void note_room_service_failure(LobbySession& lobby, const std::string& err) {
    if (!lobby.online.in_room || lobby.online.session_closed)
        return;
    if (room_server_error_is_room_closed(err)) {
        const std::string reason = lobby.online.is_host
                                       ? "Online room closed."
                                       : "Host left. Host migration is not supported.";
        mark_session_closed(lobby, reason);
        return;
    }

    if (!lobby.online.reconnecting) {
        lobby.online.reconnecting = true;
        lobby.online.room_failure_count = 1;
        lobby.online.first_room_failure_at = lobby.engine ? lobby.engine->now : 0.0;
    } else {
        lobby.online.room_failure_count += 1;
    }

    lobby.online.status_text = "Reconnecting to room service...";
    if (!err.empty())
        lobby.online.last_error = err;

    if (!lobby.engine)
        return;
    if (lobby.engine->now - lobby.online.first_room_failure_at < kRoomReconnectGraceSec)
        return;

    const std::string reason = lobby.online.is_host
                                   ? "Lost room service. Online room closed."
                                   : "Lost room service. Host migration is not supported.";
    mark_session_closed(lobby, reason);
}

void note_room_service_recovered(LobbySession& lobby) {
    if (!lobby.online.reconnecting)
        return;
    clear_room_failure_state(lobby);
    if (lobby.online.last_error.find("room server") != std::string::npos ||
        lobby.online.last_error.find("Reconnecting") != std::string::npos ||
        lobby.online.last_error.find("Lost room service") != std::string::npos) {
        lobby.online.last_error.clear();
    }
}

std::string content_contract_key(const SessionContract& contract) {
    nlohmann::json key = {
        {"game_version", contract.game_version},
        {"net_protocol", contract.net_protocol},
        {"mod_hash", contract.mod_hash},
        {"required_mod_ids", contract.required_mod_ids},
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
    contract.required_mod_ids = lobby_enabled_mod_ids();
    normalize_required_mod_ids(contract.required_mod_ids);
    contract.allow_live_mod_reload = true;
    contract.game_config = capture_game_lobby_config(lobby);
    contract.realtime_endpoint = coop_session_advertised_endpoint();

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
    contract.required_mod_ids = lobby_enabled_mod_ids();
    normalize_required_mod_ids(contract.required_mod_ids);
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

bool should_retry_content_sync(const LobbySession& lobby, std::uint64_t revision) {
    if (revision == 0 || revision != lobby.online.failed_content_revision)
        return true;
    if (!lobby.engine)
        return true;
    return lobby.engine->now >= lobby.online.next_content_retry_at;
}

bool sync_remote_content_contract(LobbySession& lobby,
                                  const SessionContract& remote,
                                  std::string& err) {
    if (lobby.online.is_host)
        return true;
    if (remote.game_version != required_mod_game_version()) {
        err = "Host is running a different game version";
        return false;
    }
    if (!remote.net_protocol.empty() &&
        remote.net_protocol != session_contract_default_net_protocol()) {
        err = "Host is running a different network protocol";
        return false;
    }

    lobby.online.content_status_text = "Syncing host content...";
    if (!lobby.engine ||
        !sync_mod_selection_from_catalog(*lobby.engine,
                                         default_mod_server_url(),
                                         remote.required_mod_ids,
                                         err)) {
        lobby.online.failed_content_revision = remote.content_revision;
        if (lobby.engine)
            lobby.online.next_content_retry_at = lobby.engine->now + kContentRetryIntervalSec;
        return false;
    }

    lobby_refresh_mods();
    lobby.online.synced_content_revision = remote.content_revision;
    lobby.online.failed_content_revision = 0;
    lobby.online.next_content_retry_at = 0.0;
    lobby.online.last_error.clear();
    lobby.online.content_status_text =
        remote.required_mod_ids.empty() ? "Content synced" : "Host mods synced";
    return true;
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
    note_room_service_recovered(lobby);
    apply_room_to_lobby(room, lobby);
    if (room.contract.game_config.is_object())
        apply_game_lobby_config(room.contract.game_config, lobby);
    read_room_members(room, lobby);

    const bool revision_changed =
        room.contract.content_revision != 0 &&
        room.contract.content_revision != lobby.online.synced_content_revision;
    SessionCompatibility compatibility =
        session_contract_check_compatibility(room.contract, build_expected_local_contract());
    if (!lobby.online.is_host &&
        (revision_changed || compatibility == SessionCompatibility::NeedsContentReload)) {
        if (compatibility == SessionCompatibility::NeedsContentReload &&
            should_retry_content_sync(lobby, room.contract.content_revision)) {
            std::string sync_err;
            if (!sync_remote_content_contract(lobby, room.contract, sync_err)) {
                if (!sync_err.empty())
                    lobby.online.last_error = sync_err;
            }
            compatibility =
                session_contract_check_compatibility(room.contract, build_expected_local_contract());
        } else if (compatibility == SessionCompatibility::Compatible) {
            lobby.online.synced_content_revision = room.contract.content_revision;
            lobby.online.content_status_text = "Session content updated";
        } else if (!should_retry_content_sync(lobby, room.contract.content_revision)) {
            lobby.online.content_status_text = "Retrying content sync soon";
        }
    } else if (lobby.online.is_host || compatibility == SessionCompatibility::Compatible) {
        lobby.online.synced_content_revision = room.contract.content_revision;
        if (lobby.online.is_host)
            lobby.online.content_status_text.clear();
    }

    std::ostringstream status;
    status << (session_contract_is_in_game(room.contract) ? "In Game" : "Lobby")
           << " | Room " << room.room_code << " | " << room.current_players
           << "/" << room.max_players << " players";
    lobby.online.compatibility = compatibility;
    if (compatibility != SessionCompatibility::Compatible) {
        status << " | " << session_contract_compatibility_text(compatibility);
        if (compatibility == SessionCompatibility::WrongGameVersion ||
            compatibility == SessionCompatibility::WrongNetProtocol) {
            lobby.online.last_error = session_contract_compatibility_text(compatibility);
        }
    } else if (lobby.online.last_error == "Needs content reload" ||
               lobby.online.last_error == "Wrong game version" ||
               lobby.online.last_error == "Wrong network protocol") {
        lobby.online.last_error.clear();
    }
    if (!lobby.online.content_status_text.empty())
        status << " | " << lobby.online.content_status_text;
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
    lobby.online.compatibility = SessionCompatibility::Compatible;
    lobby.online.contract = room.contract;
    lobby.online.contract.session_phase = "lobby";
    lobby.online.contract.realtime_endpoint.clear();
    lobby.online.synced_content_revision = lobby.online.contract.content_revision;
    lobby.online.failed_content_revision = 0;
    lobby.online.next_content_retry_at = 0.0;
    lobby.online.content_status_text.clear();
    lobby.online.next_room_poll_at = 0.0;
    lobby.online.next_room_publish_at = 0.0;
    lobby.online.session_closed = false;
    lobby.online.session_close_reason.clear();
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
    lobby.online.compatibility = SessionCompatibility::Compatible;
    lobby.online.contract = SessionContract{};
    lobby.online.contract.net_protocol = session_contract_default_net_protocol();
    lobby.online.synced_content_revision = 0;
    lobby.online.failed_content_revision = 0;
    lobby.online.next_content_retry_at = 0.0;
    lobby.online.content_status_text = "Joining room...";
    lobby.online.next_room_poll_at = 0.0;
    lobby.online.next_room_publish_at = 0.0;
    lobby.online.session_closed = false;
    lobby.online.session_close_reason.clear();
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
    clear_local_room_state(lobby, "Offline lobby");
    lobby.online.last_error.clear();
    return true;
}

bool lobby_online_remove_member(LobbySession& lobby, const std::string& member_id, std::string& err) {
    if (!lobby.online.in_room || !lobby.online.is_host) {
        err = "Only the host can remove clients.";
        return false;
    }
    if (member_id.empty() || member_id == lobby.online.member_id) {
        err = "Cannot remove the local host.";
        return false;
    }
    if (!g_matchmaking.remove_member(lobby.online.server_url,
                                     lobby.online.room_code,
                                     lobby.online.host_secret,
                                     member_id,
                                     err)) {
        return false;
    }
    return refresh_room_state(lobby, err);
}

bool lobby_online_consume_session_close(LobbySession& lobby, std::string& reason_out) {
    if (!lobby.online.session_closed)
        return false;
    reason_out = lobby.online.session_close_reason.empty()
                     ? std::string("Online session closed.")
                     : lobby.online.session_close_reason;
    clear_local_room_state(lobby, "Offline lobby");
    return true;
}

bool lobby_online_refresh_rooms(LobbySession& lobby, bool force, std::string& err) {
    if (!force && lobby.engine && lobby.engine->now < lobby.online.next_rooms_refresh_at)
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
    lobby.online.next_rooms_refresh_at =
        lobby.engine ? lobby.engine->now + kRoomsRefreshIntervalSec : kRoomsRefreshIntervalSec;
    return true;
}

void lobby_online_tick(LobbySession& lobby) {
    if (!lobby.engine)
        return;
    if (lobby.online.session_closed)
        return;
    std::string err;
    if (lobby.online.in_room) {
        if (lobby.online.is_host && lobby.engine->now >= lobby.online.next_room_publish_at) {
            lobby_refresh_mods();
            if (publish_room_state(lobby, err)) {
                note_room_service_recovered(lobby);
                lobby.online.next_room_publish_at = lobby.engine->now + kRoomPublishIntervalSec;
            } else if (!err.empty()) {
                note_room_service_failure(lobby, err);
            }
        } else if (!lobby.online.is_host && lobby.engine->now >= lobby.online.next_room_publish_at) {
            if (heartbeat_member(lobby, err)) {
                note_room_service_recovered(lobby);
                lobby.online.next_room_publish_at = lobby.engine->now + kRoomPublishIntervalSec;
            } else if (!err.empty()) {
                note_room_service_failure(lobby, err);
            }
        }

        if (lobby.online.session_closed)
            return;
        if (lobby.engine->now >= lobby.online.next_room_poll_at) {
            if (refresh_room_state(lobby, err)) {
                lobby.online.next_room_poll_at = lobby.engine->now + kRoomPollIntervalSec;
            } else if (!err.empty()) {
                note_room_service_failure(lobby, err);
            }
        }
    }
}
