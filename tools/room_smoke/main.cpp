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

        MatchmakingCreateResult created;
        if (!matchmaking.create_room(server_url, room, created, err))
            throw std::runtime_error(err);
        std::string room_code = created.room_code;
        std::string host_secret = created.host_secret;
        std::string host_member_id = created.member_id;

        std::vector<MatchmakingRoom> listed;
        if (!matchmaking.list_rooms(server_url, listed, err))
            throw std::runtime_error(err);
        bool found_room = false;
        for (const auto& listed_room : listed) {
            if (listed_room.room_code == room_code)
                found_room = true;
        }
        if (!found_room)
            throw std::runtime_error("created room missing from room list");

        std::string guest_member_id;
        if (!matchmaking.join_room(server_url, room_code, "Guest", guest_member_id, err))
            throw std::runtime_error(err);
        std::string second_guest_member_id;
        if (!matchmaking.join_room(server_url, room_code, "Guest Two", second_guest_member_id, err))
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
