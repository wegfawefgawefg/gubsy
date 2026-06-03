#include "gubsy/lobby/connection_cascade.hpp"

#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/room_matchmaking.hpp"
#include "src/user_profiles.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>

namespace {

constexpr double kRoomRefreshIntervalSec = 2.0;
constexpr double kRoomHeartbeatIntervalSec = 1.0;

RoomServerMatchmaking g_matchmaking;

IMatchmaking& matchmaking(EngineState& engine) {
    if (engine.lobby_matchmaking != nullptr)
        return *engine.lobby_matchmaking;
    return g_matchmaking;
}

AsyncMatchmakingClient& async_matchmaking(EngineState& engine) {
    if (!engine.async_matchmaking)
        engine.async_matchmaking = std::make_unique<AsyncMatchmakingClient>();
    return *engine.async_matchmaking;
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
    engine.lobby.room_publish_in_flight = false;
    engine.lobby.room_lookup_in_flight = false;
    engine.lobby.join_attempt_in_flight = false;
    engine.lobby.room_join_finalize_in_flight = false;
    engine.lobby.room_remove_in_flight = false;
    engine.lobby.pending_room_code.clear();
    engine.lobby.pending_direct_join_endpoint.clear();
    engine.lobby.pending_join_attempt_id.clear();
    engine.lobby.pending_join_token.clear();
    engine.lobby.pending_punch_secret.clear();
    engine.lobby.pending_relay_allocation_id.clear();
    engine.lobby.pending_relay_secret.clear();
    engine.lobby.connect_phase = ConnectPhase::Idle;
    engine.lobby.selected_transport.clear();
    engine.lobby.pending_join_room = MatchmakingRoom{};
}

SessionContract build_lobby_contract(EngineState& engine) {
    SessionContract contract = engine.lobby.contract;
    if (contract.net_protocol.empty())
        contract.net_protocol = session_contract_default_net_protocol();
    if (contract.session_phase.empty())
        contract.session_phase = "lobby";
    contract.realtime_endpoint = engine.lobby.advertised_endpoint;
    contract.connection_candidates.clear();
    if (!engine.lobby.advertised_endpoint.empty()) {
        ConnectionCandidate candidate;
        candidate.kind = ConnectionCandidateKind::LanDirect;
        candidate.priority = 100;
        candidate.endpoint = engine.lobby.advertised_endpoint;
        candidate.label = "Direct UDP";
        contract.connection_candidates.push_back(std::move(candidate));
    }
    if (engine.lobby.visibility == GubsyLobbyVisibility::Public) {
        ConnectionCandidate candidate;
        candidate.kind = ConnectionCandidateKind::NatPunch;
        candidate.priority = 200;
        candidate.label = "NAT traversal";
        contract.connection_candidates.push_back(std::move(candidate));
    }
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
    room.current_players =
        static_cast<int>(engine.lobby.local_players.size() + engine.lobby.game_members.size());
    room.contract = build_lobby_contract(engine);
    return room;
}

void apply_room_to_lobby(EngineState& engine, const MatchmakingRoom& room) {
    engine.lobby.room_code = room.room_code;
    engine.lobby.lobby_name =
        room.session_name.empty() ? engine.lobby.lobby_name : room.session_name;
    engine.lobby.max_players = std::max(1, room.max_players);
    engine.lobby.contract = room.contract;
    engine.lobby.room_members = room.members;
    engine.lobby.room_current_players = std::max(0, room.current_players);
    engine.lobby.advertised_endpoint = room.contract.realtime_endpoint;
    if (engine.lobby.advertised_endpoint.empty()) {
        for (const ConnectionCandidate& candidate : gubsy_sorted_connection_candidates(room)) {
            if (!candidate.endpoint.empty()) {
                engine.lobby.advertised_endpoint = candidate.endpoint;
                break;
            }
        }
    }
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

void update_members(EngineState& engine, std::vector<MatchmakingMember>& current_members,
                    const std::vector<MatchmakingMember>& next_members, bool alert_changes) {
    if (alert_changes) {
        std::vector<const MatchmakingMember*> joined_members;
        std::vector<const MatchmakingMember*> left_members;
        for (const MatchmakingMember& next : next_members) {
            if (next.member_id.empty())
                continue;
            if (!find_member_by_id(current_members, next.member_id))
                joined_members.push_back(&next);
        }
        for (const MatchmakingMember& old : current_members) {
            if (old.member_id.empty())
                continue;
            if (!find_member_by_id(next_members, old.member_id))
                left_members.push_back(&old);
        }
        auto alert_grouped = [&](const std::vector<const MatchmakingMember*>& members,
                                 const char* verb) {
            std::vector<bool> consumed(members.size(), false);
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (consumed[i])
                    continue;
                const MatchmakingMember& member = *members[i];
                if (member.client_label.empty()) {
                    add_alert(engine, std::string(verb) == "joined"
                                          ? member_joined_alert(member)
                                          : member_left_alert(member));
                    consumed[i] = true;
                    continue;
                }
                std::size_t group_count = 1;
                for (std::size_t j = i + 1; j < members.size(); ++j) {
                    if (!consumed[j] && members[j]->client_label == member.client_label) {
                        consumed[j] = true;
                        ++group_count;
                    }
                }
                consumed[i] = true;
                if (group_count == 1) {
                    add_alert(engine, std::string(verb) == "joined"
                                          ? member_joined_alert(member)
                                          : member_left_alert(member));
                } else {
                    add_alert(engine,
                              std::to_string(group_count) + " players " + verb +
                                  " from client " + member.client_label);
                }
            }
        };
        alert_grouped(joined_members, "joined");
        alert_grouped(left_members, "left");
    }
    current_members = next_members;
}

void update_room_members(EngineState& engine, const std::vector<MatchmakingMember>& next_members,
                         bool alert_changes) {
    if (engine.lobby.game_members_authoritative)
        alert_changes = false;
    update_members(engine, engine.lobby.room_members, next_members, alert_changes);
}

void update_game_members(EngineState& engine, const std::vector<MatchmakingMember>& next_members,
                         bool alert_changes) {
    update_members(engine, engine.lobby.game_members, next_members, alert_changes);
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

void force_leave_online_session(EngineState& engine, const std::string& status);
void disconnect_game_transport(EngineState& engine);

void apply_heartbeat_room_result(EngineState& engine, const MatchmakingRoom& room) {
    if (!engine.lobby.is_host && !engine.lobby.member_id.empty() &&
        !find_member_by_id(room.members, engine.lobby.member_id)) {
        force_leave_online_session(engine, "Removed from online room");
        engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
        return;
    }
    engine.lobby.max_players = std::max(1, room.max_players);
    engine.lobby.contract = room.contract;
    engine.lobby.advertised_endpoint = room.contract.realtime_endpoint;
    engine.lobby.room_current_players = std::max(0, room.current_players);
    update_room_members(engine, room.members, true);
}

void apply_heartbeat_failure(EngineState& engine, const std::string& err) {
    if (!engine.lobby.is_host && err.find("member not found") != std::string::npos) {
        force_leave_online_session(engine, "Removed from online room");
        engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
        return;
    }
    engine.lobby.last_error = err.empty() ? "Room heartbeat failed" : err;
}

void apply_heartbeat_fetch_error(EngineState& engine, const std::string& err) {
    if (!err.empty())
        engine.lobby.last_error = err;
}

void apply_room_list(EngineState& engine, std::vector<MatchmakingRoom> rooms) {
    engine.lobby.discovered_rooms = std::move(rooms);
    engine.lobby.browse_room_codes.clear();
    for (const MatchmakingRoom& room : engine.lobby.discovered_rooms)
        engine.lobby.browse_room_codes.push_back(room.room_code);
    clear_lobby_error(engine,
                      std::to_string(engine.lobby.discovered_rooms.size()) + " rooms visible");
}

void apply_room_list_failure(EngineState& engine, const std::string& err) {
    const std::string message = err.empty() ? "Cannot refresh rooms: failed to list rooms"
                                            : with_prefix("Cannot refresh rooms", err);
    set_lobby_error(engine, message);
}

void clear_online_room_identity(EngineState& engine) {
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.room_members.clear();
}

void fail_pending_room_publish(EngineState& engine, const std::string& err) {
    disconnect_game_transport(engine);
    engine.lobby.online = false;
    engine.lobby.is_host = false;
    engine.lobby.room_publish_in_flight = false;
    clear_online_room_identity(engine);
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
    const std::string message = err.empty() ? "Cannot host room: failed to publish room"
                                            : with_prefix("Cannot host room", err);
    set_lobby_error(engine, message);
    add_alert(engine, message, AlertSeverity::Error);
}

void complete_pending_room_publish(EngineState& engine, const AsyncCreateRoomResult& result) {
    engine.lobby.room_publish_in_flight = false;
    engine.lobby.online = true;
    engine.lobby.is_host = true;
    engine.lobby.room_code = result.create.room_code;
    engine.lobby.host_secret = result.create.host_secret;
    engine.lobby.member_id = result.create.member_id;
    if (result.has_room)
        apply_room_to_lobby(engine, result.room);
    clear_lobby_error(engine, "Hosting room " + engine.lobby.room_code);
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
    add_alert(engine, engine.lobby.status_message, AlertSeverity::Success);
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
    engine.lobby.room_members.clear();
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
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

bool queue_or_complete_room_join(EngineState& engine,
                                 const MatchmakingRoom& room_for_join,
                                 const std::string& join_token,
                                 std::string& message) {
    if (engine.lobby_matchmaking == nullptr) {
        AsyncJoinRoomRequest request;
        request.request_id = ++engine.lobby.room_join_finalize_request_id;
        request.server_url = engine.lobby.room_server_url;
        request.room_code = room_for_join.room_code;
        request.display_name = local_player_name(engine);
        request.join_token = join_token;
        engine.lobby.pending_join_room = room_for_join;
        engine.lobby.room_join_finalize_in_flight = true;
        engine.lobby.room_join_pending = true;
        clear_lobby_error(engine, "Joining room " + room_for_join.room_code);
        message = engine.lobby.status_message;
        async_matchmaking(engine).enqueue_join_room(std::move(request));
        return true;
    }

    std::string member_id;
    std::string err;
    if (!matchmaking(engine).join_room(engine.lobby.room_server_url,
                                       room_for_join.room_code,
                                       local_player_name(engine),
                                       join_token,
                                       member_id,
                                       err)) {
        disconnect_game_transport(engine);
        engine.lobby.connect_phase = ConnectPhase::Failed;
        message = err.empty() ? "Cannot join room: room service rejected join"
                              : with_prefix("Cannot join room", err);
        set_lobby_error(engine, message);
        return false;
    }

    apply_room_to_lobby(engine, room_for_join);
    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.member_id = member_id;
    engine.lobby.host_secret.clear();
    if (auto current_room = fetch_current_room(engine, err))
        apply_room_to_lobby(engine, *current_room);
    engine.lobby.connect_phase = ConnectPhase::Connected;
    clear_direct_join_pending(engine);
    clear_lobby_error(engine, "Joined room " + room_for_join.room_code);
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
    message = engine.lobby.status_message;
    return true;
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

bool continue_room_join_after_attempt(EngineState& engine,
                                      const MatchmakingRoom& requested_room,
                                      const MatchmakingJoinAttemptResult& join_attempt,
                                      std::string& message) {
    const MatchmakingRoom room_for_join =
        join_attempt.room.room_code.empty() ? requested_room : join_attempt.room;
    if (!validate_room_joinable(engine, room_for_join, message))
        return false;
    if (!validate_room_contract(engine, room_for_join, message))
        return false;
    std::optional<DirectConnectionCandidate> selected_candidate =
        gubsy_first_direct_connection_candidate(room_for_join);
    if (!selected_candidate.has_value()) {
        engine.lobby.connect_phase = ConnectPhase::Failed;
        message = "Room has no supported connection candidate";
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.connect_phase =
        gubsy_connect_phase_for_candidate(selected_candidate->candidate.kind);
    engine.lobby.selected_transport =
        connection_candidate_kind_id(selected_candidate->candidate.kind);
    engine.lobby.pending_join_attempt_id = join_attempt.join_attempt_id;
    engine.lobby.pending_join_token = join_attempt.join_token;
    engine.lobby.pending_punch_secret = join_attempt.punch_secret;
    engine.lobby.pending_relay_allocation_id = join_attempt.relay_allocation_id;
    engine.lobby.pending_relay_secret = join_attempt.relay_secret;
    engine.lobby.pending_join_room = room_for_join;
    GubsyLobbyJoinResult join_result = engine.lobby_commands.join(
        engine.lobby_commands.join_user_data,
        engine.lobby,
        selected_candidate->host.c_str(),
        selected_candidate->port);
    if (!join_result.ok) {
        engine.lobby.connect_phase = ConnectPhase::Failed;
        message = join_result.status.empty() ? "Cannot join room: failed to reach host transport"
                                             : with_prefix("Cannot join room", join_result.status);
        set_lobby_error(engine, message);
        return false;
    }

    engine.lobby.join_host = selected_candidate->host;
    engine.lobby.network_port = static_cast<int>(selected_candidate->port);
    engine.lobby.advertised_endpoint = selected_candidate->candidate.endpoint.empty()
                                           ? room_for_join.contract.realtime_endpoint
                                           : selected_candidate->candidate.endpoint;
    engine.lobby.contract = room_for_join.contract;
    if (join_result.pending) {
        engine.lobby.online = false;
        engine.lobby.is_host = false;
        clear_online_room_identity(engine);
        engine.lobby.game_members.clear();
        engine.lobby.game_members_authoritative = false;
        engine.lobby.direct_join_pending = true;
        engine.lobby.room_join_pending = true;
        engine.lobby.pending_direct_join_endpoint = engine.lobby.advertised_endpoint;
        clear_lobby_error(engine, join_result.status.empty()
                                      ? "Joining room " + room_for_join.room_code
                                      : join_result.status);
        message = engine.lobby.status_message;
        return true;
    }

    return queue_or_complete_room_join(engine, room_for_join, join_attempt.join_token, message);
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

    MatchmakingRoom room = build_room_metadata(engine);
    if (engine.lobby_matchmaking == nullptr) {
        AsyncCreateRoomRequest request;
        request.request_id = ++engine.lobby.room_publish_request_id;
        request.server_url = engine.lobby.room_server_url;
        request.room = std::move(room);
        engine.lobby.online = true;
        engine.lobby.is_host = true;
        engine.lobby.visibility = GubsyLobbyVisibility::Public;
        engine.lobby.room_publish_in_flight = true;
        clear_online_room_identity(engine);
        engine.lobby.game_members.clear();
        engine.lobby.game_members_authoritative = false;
        clear_lobby_error(engine, "Publishing room...");
        message = engine.lobby.status_message;
        async_matchmaking(engine).enqueue_create_room(std::move(request));
        return true;
    }

    MatchmakingCreateResult create_result;
    std::string err;
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
    engine.lobby.room_members.clear();
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
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
        engine.lobby.room_members.clear();
        engine.lobby.game_members.clear();
        engine.lobby.game_members_authoritative = false;
        engine.lobby.direct_join_pending = true;
        engine.lobby.room_join_pending = false;
        engine.lobby.pending_direct_join_endpoint = engine.lobby.advertised_endpoint;
        engine.lobby.connect_phase = ConnectPhase::TryingLanDirect;
        engine.lobby.selected_transport = connection_candidate_kind_id(ConnectionCandidateKind::LanDirect);
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
    engine.lobby.room_members.clear();
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
    engine.lobby.connect_phase = ConnectPhase::Connected;
    engine.lobby.selected_transport = connection_candidate_kind_id(ConnectionCandidateKind::LanDirect);
    clear_lobby_error(engine, "Joined direct " + engine.lobby.advertised_endpoint);
    message = engine.lobby.status_message;
    return true;
}

void gubsy_lobby_confirm_direct_join(EngineState& engine, const std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    if (!engine.lobby.direct_join_pending)
        return;

    if (engine.lobby.room_join_pending) {
        if (engine.lobby.room_join_finalize_in_flight)
            return;
        const MatchmakingRoom room = engine.lobby.pending_join_room;
        (void)message;
        std::string join_message;
        if (!queue_or_complete_room_join(engine, room, engine.lobby.pending_join_token,
                                         join_message)) {
            add_alert(engine, join_message, AlertSeverity::Error);
        }
        return;
    }

    engine.lobby.online = true;
    engine.lobby.is_host = false;
    engine.lobby.room_code.clear();
    engine.lobby.member_id.clear();
    engine.lobby.host_secret.clear();
    engine.lobby.room_current_players = 0;
    engine.lobby.room_members.clear();
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
    clear_direct_join_pending(engine);
    engine.lobby.connect_phase = ConnectPhase::Connected;
    clear_lobby_error(engine, message.empty() ? "Joined direct " + engine.lobby.advertised_endpoint
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
    engine.lobby.room_members.clear();
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
    clear_direct_join_pending(engine);
    engine.lobby.connect_phase = ConnectPhase::Failed;
    set_lobby_error(engine, message.empty() ? "No server found" : message);
    add_alert(engine, engine.lobby.status_message, AlertSeverity::Error);
}

bool gubsy_lobby_join_room(EngineState& engine, const MatchmakingRoom& room, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    engine.lobby.connect_phase = ConnectPhase::CheckingCompatibility;
    if (!validate_room_joinable(engine, room, message))
        return false;
    if (!validate_room_contract(engine, room, message))
        return false;

    std::optional<DirectConnectionCandidate> selected_candidate =
        gubsy_first_direct_connection_candidate(room);
    if (!selected_candidate.has_value()) {
        engine.lobby.connect_phase = ConnectPhase::Failed;
        message = "Room has no supported connection candidate";
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

    engine.lobby.room_lookup_in_flight = false;
    engine.lobby.pending_room_code.clear();
    engine.lobby.connect_phase = ConnectPhase::ResolvingRoom;
    engine.lobby.pending_join_room = room;
    if (engine.lobby_matchmaking == nullptr) {
        AsyncCreateJoinAttemptRequest request;
        request.request_id = ++engine.lobby.join_attempt_request_id;
        request.server_url = engine.lobby.room_server_url;
        request.room_code = room.room_code;
        request.display_name = local_player_name(engine);
        engine.lobby.join_attempt_in_flight = true;
        clear_lobby_error(engine, "Resolving room " + room.room_code);
        message = engine.lobby.status_message;
        async_matchmaking(engine).enqueue_create_join_attempt(std::move(request));
        return true;
    }

    MatchmakingJoinAttemptResult join_attempt;
    std::string err;
    if (!matchmaking(engine).create_join_attempt(engine.lobby.room_server_url,
                                                 room.room_code,
                                                 local_player_name(engine),
                                                 join_attempt,
                                                 err)) {
        engine.lobby.connect_phase = ConnectPhase::Failed;
        message = err.empty() ? "Cannot join room: room service rejected join attempt"
                              : with_prefix("Cannot join room", err);
        set_lobby_error(engine, message);
        return false;
    }
    return continue_room_join_after_attempt(engine, room, join_attempt, message);
}

bool gubsy_lobby_join_room_code(EngineState& engine, const std::string& room_code,
                                std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (engine.lobby_matchmaking == nullptr) {
        AsyncFetchRoomRequest request;
        request.request_id = ++engine.lobby.room_lookup_request_id;
        request.server_url = engine.lobby.room_server_url;
        request.room_code = room_code;
        engine.lobby.pending_room_code = room_code;
        engine.lobby.room_lookup_in_flight = true;
        engine.lobby.connect_phase = ConnectPhase::ResolvingRoom;
        clear_lobby_error(engine, "Resolving room " + room_code);
        message = engine.lobby.status_message;
        async_matchmaking(engine).enqueue_fetch_room(std::move(request));
        return true;
    }

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
        engine.lobby.room_members.clear();
        engine.lobby.game_members.clear();
        engine.lobby.game_members_authoritative = false;
        clear_direct_join_pending(engine);
        clear_lobby_error(engine, "Left direct session");
        message = engine.lobby.status_message;
        return true;
    }
    if (engine.lobby_matchmaking != nullptr) {
        std::string err;
        if (!matchmaking(engine).leave_room(engine.lobby.room_server_url, engine.lobby.room_code,
                                            engine.lobby.member_id, engine.lobby.host_secret, err)) {
            message = err.empty() ? "Cannot leave room: room service rejected leave"
                                  : with_prefix("Cannot leave room", err);
            set_lobby_error(engine, message);
            return false;
        }
    } else {
        AsyncLeaveRoomRequest request;
        request.request_id = ++engine.lobby.room_leave_request_id;
        request.server_url = engine.lobby.room_server_url;
        request.room_code = engine.lobby.room_code;
        request.member_id = engine.lobby.member_id;
        request.host_secret = engine.lobby.host_secret;
        async_matchmaking(engine).enqueue_leave_room(std::move(request));
    }
    disconnect_game_transport(engine);

    engine.lobby.online = false;
    engine.lobby.is_host = false;
    clear_online_room_identity(engine);
    engine.lobby.game_members.clear();
    engine.lobby.game_members_authoritative = false;
    clear_direct_join_pending(engine);
    clear_lobby_error(engine, "Left online room");
    message = engine.lobby.status_message;
    return true;
}

bool gubsy_lobby_refresh_rooms(EngineState& engine, bool force, std::string& message) {
    gubsy_lobby_ensure_ready(engine);
    ensure_room_defaults(engine);
    if (engine.async_matchmaking) {
        for (const AsyncRoomListResult& result :
             engine.async_matchmaking->drain_room_list_results()) {
            if (result.request_id != engine.lobby.room_refresh_request_id)
                continue;
            engine.lobby.room_refresh_in_flight = false;
            if (result.ok)
                apply_room_list(engine, result.rooms);
            else
                apply_room_list_failure(engine, result.err);
        }
    }
    if (!force && engine.now < engine.lobby.next_room_refresh_at) {
        message = engine.lobby.status_message;
        return true;
    }
    if (engine.lobby.room_refresh_in_flight) {
        message = engine.lobby.status_message;
        return true;
    }

    if (engine.lobby_matchmaking == nullptr) {
        AsyncRoomListRequest request;
        request.request_id = ++engine.lobby.room_refresh_request_id;
        request.server_url = engine.lobby.room_server_url;
        engine.lobby.room_refresh_in_flight = true;
        engine.lobby.next_room_refresh_at = engine.now + kRoomRefreshIntervalSec;
        clear_lobby_error(engine, "Refreshing rooms...");
        message = engine.lobby.status_message;
        async_matchmaking(engine).enqueue_room_list(std::move(request));
        return true;
    }

    std::vector<MatchmakingRoom> rooms;
    std::string err;
    if (!matchmaking(engine).list_rooms(engine.lobby.room_server_url, rooms, err)) {
        apply_room_list_failure(engine, err);
        message = engine.lobby.status_message;
        return false;
    }

    apply_room_list(engine, std::move(rooms));
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

    const MatchmakingMember* target = find_member_by_id(engine.lobby.room_members, member_id);
    const std::string target_name = target ? member_name(*target) : member_id;

    if (engine.lobby_matchmaking == nullptr) {
        AsyncRemoveMemberRequest request;
        request.request_id = ++engine.lobby.room_remove_request_id;
        request.server_url = engine.lobby.room_server_url;
        request.room_code = engine.lobby.room_code;
        request.host_secret = engine.lobby.host_secret;
        request.target_member_id = member_id;
        request.target_name = target_name;
        engine.lobby.room_remove_in_flight = true;
        message = "Removing " + target_name;
        clear_lobby_error(engine, message);
        async_matchmaking(engine).enqueue_remove_member(std::move(request));
        return true;
    }

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
        update_room_members(engine, current_room->members, false);
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
    if (member.member_id.empty() || member.member_id == engine.lobby.member_id || member.is_host) {
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
    engine.lobby.game_members_authoritative = true;
    update_game_members(engine, members, alert_changes);
    engine.lobby.room_current_players =
        static_cast<int>(engine.lobby.local_players.size() + engine.lobby.game_members.size());
}

void run_sync_heartbeat(EngineState& engine) {
    MatchmakingRoom room_update = build_room_metadata(engine);
    MatchmakingRoom* update_ptr = engine.lobby.is_host ? &room_update : nullptr;
    std::string err;
    if (!matchmaking(engine).heartbeat_room(engine.lobby.room_server_url, engine.lobby.room_code,
                                            engine.lobby.member_id, local_player_name(engine),
                                            engine.lobby.host_secret, update_ptr, err)) {
        apply_heartbeat_failure(engine, err);
    } else {
        engine.lobby.last_error.clear();
        if (auto current_room = fetch_current_room(engine, err))
            apply_heartbeat_room_result(engine, *current_room);
        else
            apply_heartbeat_fetch_error(engine, err);
    }
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
}

void apply_async_heartbeat_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncHeartbeatResult& result :
         engine.async_matchmaking->drain_heartbeat_results()) {
        if (result.request_id != engine.lobby.heartbeat_request_id)
            continue;
        engine.lobby.heartbeat_in_flight = false;
        if (!engine.lobby.online || engine.lobby.room_code.empty())
            continue;
        if (!result.ok) {
            apply_heartbeat_failure(engine, result.err);
            continue;
        }
        engine.lobby.last_error.clear();
        if (result.has_room)
            apply_heartbeat_room_result(engine, result.room);
        else
            apply_heartbeat_fetch_error(engine, result.err);
    }
}

void apply_async_create_room_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncCreateRoomResult& result :
         engine.async_matchmaking->drain_create_room_results()) {
        if (result.request_id != engine.lobby.room_publish_request_id)
            continue;
        if (!engine.lobby.room_publish_in_flight)
            continue;
        if (result.ok)
            complete_pending_room_publish(engine, result);
        else
            fail_pending_room_publish(engine, result.err);
    }
}

void apply_async_fetch_room_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncFetchRoomResult& result : engine.async_matchmaking->drain_fetch_room_results()) {
        if (result.request_id != engine.lobby.room_lookup_request_id)
            continue;
        if (!engine.lobby.room_lookup_in_flight)
            continue;
        engine.lobby.room_lookup_in_flight = false;
        if (!result.ok) {
            engine.lobby.connect_phase = ConnectPhase::Failed;
            const std::string message =
                result.err.empty() ? "Cannot join room: room code not found"
                                   : with_prefix("Cannot join room", result.err);
            set_lobby_error(engine, message);
            add_alert(engine, message, AlertSeverity::Error);
            continue;
        }
        std::string message;
        if (gubsy_lobby_join_room(engine, result.room, message)) {
            add_alert(engine, message, AlertSeverity::Info);
        } else {
            add_alert(engine, message, AlertSeverity::Error);
        }
    }
}

void apply_async_join_attempt_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncCreateJoinAttemptResult& result :
         engine.async_matchmaking->drain_create_join_attempt_results()) {
        if (result.request_id != engine.lobby.join_attempt_request_id)
            continue;
        if (!engine.lobby.join_attempt_in_flight)
            continue;
        engine.lobby.join_attempt_in_flight = false;
        if (!result.ok) {
            engine.lobby.connect_phase = ConnectPhase::Failed;
            const std::string message =
                result.err.empty() ? "Cannot join room: room service rejected join attempt"
                                   : with_prefix("Cannot join room", result.err);
            set_lobby_error(engine, message);
            add_alert(engine, message, AlertSeverity::Error);
            continue;
        }
        std::string message;
        if (continue_room_join_after_attempt(engine,
                                             engine.lobby.pending_join_room,
                                             result.join_attempt,
                                             message)) {
            add_alert(engine, message, engine.lobby.direct_join_pending ? AlertSeverity::Info
                                                                        : AlertSeverity::Success);
        } else {
            add_alert(engine, message, AlertSeverity::Error);
        }
    }
}

void apply_async_join_room_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncJoinRoomResult& result : engine.async_matchmaking->drain_join_room_results()) {
        if (result.request_id != engine.lobby.room_join_finalize_request_id)
            continue;
        if (!engine.lobby.room_join_finalize_in_flight)
            continue;
        engine.lobby.room_join_finalize_in_flight = false;
        if (!result.ok) {
            disconnect_game_transport(engine);
            clear_direct_join_pending(engine);
            engine.lobby.room_join_pending = false;
            engine.lobby.connect_phase = ConnectPhase::Failed;
            const std::string message =
                result.err.empty() ? "Cannot join room: room service rejected join"
                                   : with_prefix("Cannot join room", result.err);
            set_lobby_error(engine, message);
            add_alert(engine, message, AlertSeverity::Error);
            continue;
        }

        const MatchmakingRoom room =
            result.has_room ? result.room : engine.lobby.pending_join_room;
        apply_room_to_lobby(engine, room);
        engine.lobby.online = true;
        engine.lobby.is_host = false;
        engine.lobby.member_id = result.member_id;
        engine.lobby.host_secret.clear();
        clear_direct_join_pending(engine);
        engine.lobby.connect_phase = ConnectPhase::Connected;
        clear_lobby_error(engine, "Joined room " + room.room_code);
        engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
        add_alert(engine, engine.lobby.status_message, AlertSeverity::Success);
    }
}

void apply_async_leave_room_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncLeaveRoomResult& result :
         engine.async_matchmaking->drain_leave_room_results()) {
        if (result.request_id != engine.lobby.room_leave_request_id)
            continue;
        if (!result.ok) {
            const std::string message = result.err.empty()
                                            ? "Room leave cleanup failed"
                                            : with_prefix("Room leave cleanup failed", result.err);
            engine.lobby.last_error = message;
            add_alert(engine, message, AlertSeverity::Warning);
        }
    }
}

void apply_async_remove_member_results(EngineState& engine) {
    if (!engine.async_matchmaking)
        return;
    for (const AsyncRemoveMemberResult& result :
         engine.async_matchmaking->drain_remove_member_results()) {
        if (result.request_id != engine.lobby.room_remove_request_id)
            continue;
        engine.lobby.room_remove_in_flight = false;
        if (!result.ok) {
            const std::string message =
                result.err.empty() ? "Cannot kick player: room service rejected removal"
                                   : with_prefix("Cannot kick player", result.err);
            set_lobby_error(engine, message);
            add_alert(engine, message, AlertSeverity::Error);
            continue;
        }
        if (result.has_room) {
            engine.lobby.room_current_players = std::max(0, result.room.current_players);
            update_room_members(engine, result.room.members, false);
        } else if (!result.err.empty()) {
            engine.lobby.last_error = result.err;
        }
        const std::string message = "Kicked " + result.target_name;
        clear_lobby_error(engine, message);
        add_alert(engine, message);
    }
}

void enqueue_async_heartbeat(EngineState& engine) {
    MatchmakingRoom room_update = build_room_metadata(engine);
    AsyncHeartbeatRequest request;
    request.request_id = ++engine.lobby.heartbeat_request_id;
    request.server_url = engine.lobby.room_server_url;
    request.room_code = engine.lobby.room_code;
    request.member_id = engine.lobby.member_id;
    request.display_name = local_player_name(engine);
    request.host_secret = engine.lobby.host_secret;
    if (engine.lobby.is_host)
        request.room_update = std::move(room_update);
    engine.lobby.heartbeat_in_flight = true;
    engine.lobby.next_heartbeat_at = engine.now + kRoomHeartbeatIntervalSec;
    async_matchmaking(engine).enqueue_heartbeat(std::move(request));
}

void gubsy_lobby_tick_online(EngineState& engine) {
    ensure_room_defaults(engine);
    apply_async_create_room_results(engine);
    apply_async_fetch_room_results(engine);
    apply_async_join_attempt_results(engine);
    apply_async_join_room_results(engine);
    apply_async_leave_room_results(engine);
    apply_async_remove_member_results(engine);
    apply_async_heartbeat_results(engine);
    if (!engine.lobby.online || engine.lobby.room_code.empty()) {
        engine.lobby.heartbeat_in_flight = false;
        return;
    }
    if (engine.now < engine.lobby.next_heartbeat_at || engine.lobby.heartbeat_in_flight)
        return;

    if (engine.lobby_matchmaking != nullptr) {
        run_sync_heartbeat(engine);
        return;
    }
    enqueue_async_heartbeat(engine);
}

void gubsy_lobby_force_online_tick(EngineState& engine) {
    engine.lobby.next_heartbeat_at = 0.0;
    if (engine.lobby_matchmaking == nullptr)
        engine.lobby.heartbeat_in_flight = false;
    gubsy_lobby_tick_online(engine);
}
