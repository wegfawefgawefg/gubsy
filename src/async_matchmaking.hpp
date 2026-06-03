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

struct AsyncFetchRoomRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    std::string room_code;
};

struct AsyncFetchRoomResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    MatchmakingRoom room;
};

struct AsyncCreateRoomRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    MatchmakingRoom room;
};

struct AsyncCreateRoomResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    MatchmakingCreateResult create;
    bool has_room{false};
    MatchmakingRoom room;
};

struct AsyncCreateJoinAttemptRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    std::string room_code;
    std::string display_name;
};

struct AsyncCreateJoinAttemptResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    MatchmakingJoinAttemptResult join_attempt;
};

struct AsyncJoinRoomRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    std::string room_code;
    std::string display_name;
    std::string join_token;
};

struct AsyncJoinRoomResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    std::string member_id;
    bool has_room{false};
    MatchmakingRoom room;
};

struct AsyncLeaveRoomRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    std::string room_code;
    std::string member_id;
    std::string host_secret;
};

struct AsyncLeaveRoomResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
};

struct AsyncRemoveMemberRequest {
    std::uint64_t request_id{0};
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string target_member_id;
    std::string target_name;
};

struct AsyncRemoveMemberResult {
    std::uint64_t request_id{0};
    bool ok{false};
    std::string err;
    std::string target_name;
    bool has_room{false};
    MatchmakingRoom room;
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
    void enqueue_fetch_room(AsyncFetchRoomRequest request);
    std::vector<AsyncFetchRoomResult> drain_fetch_room_results();
    void enqueue_create_room(AsyncCreateRoomRequest request);
    std::vector<AsyncCreateRoomResult> drain_create_room_results();
    void enqueue_create_join_attempt(AsyncCreateJoinAttemptRequest request);
    std::vector<AsyncCreateJoinAttemptResult> drain_create_join_attempt_results();
    void enqueue_join_room(AsyncJoinRoomRequest request);
    std::vector<AsyncJoinRoomResult> drain_join_room_results();
    void enqueue_leave_room(AsyncLeaveRoomRequest request);
    std::vector<AsyncLeaveRoomResult> drain_leave_room_results();
    void enqueue_remove_member(AsyncRemoveMemberRequest request);
    std::vector<AsyncRemoveMemberResult> drain_remove_member_results();
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
