#pragma once

#include "gubsy/lobby/matchmaking.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct AsyncHeartbeatRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    std::string room_code;
    std::string member_id;
    std::string display_name;
    std::string host_secret;
    std::optional<MatchmakingRoom> room_update;
};

struct AsyncHeartbeatResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    bool has_room{false};
    MatchmakingRoom room;
};

struct AsyncRoomListRequest {
    std::uint64_t request_id{0};
    std::string server_url;
};

struct AsyncRoomListResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    std::vector<MatchmakingRoom> rooms;
};

class AsyncMatchmakingClient {
public:
    AsyncMatchmakingClient();
    ~AsyncMatchmakingClient();

    AsyncMatchmakingClient(const AsyncMatchmakingClient&) = delete;
    AsyncMatchmakingClient& operator=(const AsyncMatchmakingClient&) = delete;

    void enqueue_heartbeat(AsyncHeartbeatRequest request);
    std::vector<AsyncHeartbeatResult> drain_heartbeat_results();
    void enqueue_room_list(AsyncRoomListRequest request);
    std::vector<AsyncRoomListResult> drain_room_list_results();
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
