#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "gubsy/lobby/connection_cascade.hpp"

#include "src/matchmaking.hpp"
#include "src/room_matchmaking.hpp"
#include "src/session_contract.hpp"

int main(int argc, char** argv) {
    std::string server_url = "http://127.0.0.1:8788";
    if (argc > 1)
        server_url = argv[1];

    try {
        RoomServerMatchmaking matchmaking;
        std::string err;
        MatchmakingRoom room;
        room.session_name = "Smoke Lobby";
        room.host_name = "Host";
        room.privacy = 2;
        room.max_players = 4;
        room.contract.game_version = "0.1.0";
        room.contract.net_protocol = session_contract_default_net_protocol();
        room.contract.mod_hash = "aaaabbbb";
        room.contract.required_mod_ids = {"base", "smoke_a"};
        room.contract.authority_mode = RoomAuthorityMode::PlayerHost;
        ConnectionCandidate smoke_candidate;
        smoke_candidate.kind = ConnectionCandidateKind::LanDirect;
        smoke_candidate.priority = 100;
        smoke_candidate.endpoint = "127.0.0.1:9000";
        smoke_candidate.label = "Smoke Direct";
        room.contract.connection_candidates.push_back(smoke_candidate);

        MatchmakingCreateResult created;
        if (!matchmaking.create_room(server_url, room, created, err))
            throw std::runtime_error(err);
        std::string room_code = created.room_code;
        std::string host_secret = created.host_secret;
        std::string host_member_id = created.member_id;

        MatchmakingRoom unlisted = room;
        unlisted.session_name = "Unlisted Smoke Lobby";
        unlisted.privacy = 0;
        MatchmakingCreateResult unlisted_created;
        if (!matchmaking.create_room(server_url, unlisted, unlisted_created, err))
            throw std::runtime_error(err);

        std::vector<MatchmakingRoom> listed;
        if (!matchmaking.list_rooms(server_url, listed, err))
            throw std::runtime_error(err);
        bool found_room = false;
        bool found_unlisted = false;
        for (const auto& listed_room : listed) {
            if (listed_room.room_code == room_code)
                found_room = true;
            if (listed_room.room_code == unlisted_created.room_code)
                found_unlisted = true;
        }
        if (!found_room)
            throw std::runtime_error("created room missing from room list");
        if (found_unlisted)
            throw std::runtime_error("unlisted room appeared in room list");
        MatchmakingRoom fetched_unlisted;
        if (!matchmaking.fetch_room(server_url, unlisted_created.room_code, fetched_unlisted, err))
            throw std::runtime_error("unlisted room could not be fetched directly");
        if (!matchmaking.leave_room(server_url,
                                    unlisted_created.room_code,
                                    unlisted_created.member_id,
                                    unlisted_created.host_secret,
                                    err)) {
            throw std::runtime_error(err);
        }

        MatchmakingJoinAttemptResult guest_attempt;
        if (!matchmaking.create_join_attempt(server_url, room_code, "Guest", guest_attempt, err))
            throw std::runtime_error(err);
        if (guest_attempt.join_attempt_id.empty() || guest_attempt.join_token.empty() ||
            guest_attempt.punch_secret.empty())
            throw std::runtime_error("join attempt did not return token");
        if (guest_attempt.room.room_code != room_code)
            throw std::runtime_error("join attempt did not return room");
        if (guest_attempt.room.contract.connection_candidates.empty())
            throw std::runtime_error("join attempt did not return connection candidates");
        auto selected_candidate = gubsy_first_direct_connection_candidate(guest_attempt.room);
        if (!selected_candidate.has_value())
            throw std::runtime_error("connection cascade did not select direct candidate");
        if (selected_candidate->host != "127.0.0.1" || selected_candidate->port != 9000)
            throw std::runtime_error("connection cascade selected wrong endpoint");
        if (gubsy_connect_phase_for_candidate(selected_candidate->candidate.kind) !=
            ConnectPhase::TryingLanDirect) {
            throw std::runtime_error("connection cascade selected wrong phase");
        }

        std::string guest_member_id;
        if (!matchmaking.join_room(server_url,
                                   room_code,
                                   "Guest",
                                   guest_attempt.join_token,
                                   guest_member_id,
                                   err))
            throw std::runtime_error(err);
        std::string second_guest_member_id;
        if (!matchmaking.join_room(server_url, room_code, "Guest Two", "", second_guest_member_id, err))
            throw std::runtime_error(err);

        MatchmakingRoom updated = room;
        updated.room_code = room_code;
        updated.session_name = "Smoke Lobby Updated";
        updated.privacy = 4;
        updated.max_players = 6;
        updated.contract.game_version = "0.1.1";
        updated.contract.mod_hash = "ccccdddd";
        updated.contract.required_mod_ids = {"base", "smoke_b"};
        updated.contract.realtime_endpoint = "udp://127.0.0.1:9000";
        updated.contract.session_phase = "in_game";
        if (!matchmaking.heartbeat_room(server_url,
                                        room_code,
                                        host_member_id,
                                        "Host",
                                        host_secret,
                                        &updated,
                                        err)) {
            throw std::runtime_error(err);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        MatchmakingRoom fetched;
        if (!matchmaking.fetch_room(server_url, room_code, fetched, err))
            throw std::runtime_error(err);
        if (fetched.session_name != "Smoke Lobby Updated")
            throw std::runtime_error("room session_name did not update");
        if (fetched.current_players != 3)
            throw std::runtime_error("room player count mismatch");
        if (fetched.contract.mod_hash != "ccccdddd")
            throw std::runtime_error("room mod hash did not update");
        if (fetched.contract.required_mod_ids != std::vector<std::string>({"base", "smoke_b"}))
            throw std::runtime_error("room required_mod_ids did not update");
        if (fetched.contract.realtime_endpoint != "udp://127.0.0.1:9000")
            throw std::runtime_error("room realtime endpoint did not update");
        if (fetched.contract.session_phase != "in_game")
            throw std::runtime_error("room session_phase did not update");
        if (fetched.contract.authority_mode != RoomAuthorityMode::PlayerHost)
            throw std::runtime_error("room authority mode changed unexpectedly");
        if (fetched.contract.connection_candidates.empty())
            throw std::runtime_error("room connection candidates missing");
        if (fetched.members.empty() || fetched.members.front().last_seen_seconds_ago < 0)
            throw std::runtime_error("room members missing last-seen freshness");

        listed.clear();
        if (!matchmaking.list_rooms(server_url, listed, err))
            throw std::runtime_error(err);
        bool found_in_game_room = false;
        for (const auto& listed_room : listed) {
            if (listed_room.room_code == room_code &&
                listed_room.contract.session_phase == "in_game") {
                found_in_game_room = true;
                break;
            }
        }
        if (!found_in_game_room)
            throw std::runtime_error("public in-game room missing from room list");

        if (!matchmaking.remove_member(server_url, room_code, host_secret, second_guest_member_id, err))
            throw std::runtime_error(err);
        MatchmakingRoom after_remove;
        if (!matchmaking.fetch_room(server_url, room_code, after_remove, err))
            throw std::runtime_error(err);
        if (after_remove.current_players != 2)
            throw std::runtime_error("remove_member did not remove guest");

        if (!matchmaking.leave_room(server_url, room_code, guest_member_id, {}, err))
            throw std::runtime_error(err);

        MatchmakingRoom after_leave;
        if (!matchmaking.fetch_room(server_url, room_code, after_leave, err))
            throw std::runtime_error(err);
        if (after_leave.current_players != 1)
            throw std::runtime_error("leave did not remove guest");

        if (!matchmaking.leave_room(server_url, room_code, host_member_id, host_secret, err))
            throw std::runtime_error(err);

        MatchmakingRoom should_fail;
        if (matchmaking.fetch_room(server_url, room_code, should_fail, err))
            throw std::runtime_error("host leave should remove room");

        std::cout << "[room_smoke] ok\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[room_smoke] " << e.what() << "\n";
        return 1;
    }
}
