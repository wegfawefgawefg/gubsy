#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "engine/matchmaking.hpp"
#include "engine/room_matchmaking.hpp"
#include "engine/session_contract.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

} // namespace

int main(int argc, char** argv) {
    std::string server_url = "http://127.0.0.1:8788";
    if (argc > 1)
        server_url = argv[1];

    try {
        RoomServerMatchmaking matchmaking;
        std::string err;
        MatchmakingRoom room;
        room.session_name = "Timeout Smoke";
        room.host_name = "Host";
        room.privacy = 2;
        room.max_players = 4;
        room.contract.game_version = "0.1.0";
        room.contract.net_protocol = session_contract_default_net_protocol();
        room.contract.mod_hash = "timeout";

        MatchmakingCreateResult created;
        require(matchmaking.create_room(server_url, room, created, err), err);

        MatchmakingRoom fetched;
        require(matchmaking.fetch_room(server_url, created.room_code, fetched, err), err);

        std::this_thread::sleep_for(std::chrono::seconds(13));

        MatchmakingRoom expired;
        require(!matchmaking.fetch_room(server_url, created.room_code, expired, err),
                "timed out room should not still exist");
        require(err.find("room not found") != std::string::npos,
                "expected room timeout to look like room closure");

        std::cout << "[room_timeout_smoke] ok\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[room_timeout_smoke] " << e.what() << "\n";
        return 1;
    }
}
