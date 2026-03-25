#pragma once

#include <string>
#include <vector>

#include "engine/session_contract.hpp"

struct MatchmakingMember {
    std::string member_id;
    std::string display_name;
    bool is_host{false};
};

struct MatchmakingRoom {
    std::string room_code;
    std::string session_name;
    std::string host_name;
    int privacy{0};
    int max_players{1};
    int current_players{0};
    SessionContract contract{};
    std::vector<MatchmakingMember> members;
};

struct MatchmakingCreateResult {
    std::string room_code;
    std::string host_secret;
    std::string member_id;
};

struct IMatchmaking {
    virtual ~IMatchmaking() = default;
    virtual bool create_room(const std::string& server_url,
                             const MatchmakingRoom& room,
                             MatchmakingCreateResult& out,
                             std::string& err) = 0;
    virtual bool join_room(const std::string& server_url,
                           const std::string& room_code,
                           const std::string& display_name,
                           std::string& member_id_out,
                           std::string& err) = 0;
    virtual bool leave_room(const std::string& server_url,
                            const std::string& room_code,
                            const std::string& member_id,
                            const std::string& host_secret,
                            std::string& err) = 0;
    virtual bool remove_member(const std::string& server_url,
                               const std::string& room_code,
                               const std::string& host_secret,
                               const std::string& target_member_id,
                               std::string& err) = 0;
    virtual bool heartbeat_room(const std::string& server_url,
                                const std::string& room_code,
                                const std::string& member_id,
                                const std::string& display_name,
                                const std::string& host_secret,
                                const MatchmakingRoom* room_update,
                                std::string& err) = 0;
    virtual bool fetch_room(const std::string& server_url,
                            const std::string& room_code,
                            MatchmakingRoom& out,
                            std::string& err) = 0;
    virtual bool list_rooms(const std::string& server_url,
                            std::vector<MatchmakingRoom>& out,
                            std::string& err) = 0;
};
