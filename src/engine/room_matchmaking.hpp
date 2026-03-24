#pragma once

#include "engine/matchmaking.hpp"

class RoomServerMatchmaking final : public IMatchmaking {
public:
    bool create_room(const std::string& server_url,
                     const MatchmakingRoom& room,
                     MatchmakingCreateResult& out,
                     std::string& err) override;
    bool join_room(const std::string& server_url,
                   const std::string& room_code,
                   const std::string& display_name,
                   std::string& member_id_out,
                   std::string& err) override;
    bool leave_room(const std::string& server_url,
                    const std::string& room_code,
                    const std::string& member_id,
                    const std::string& host_secret,
                    std::string& err) override;
    bool heartbeat_room(const std::string& server_url,
                        const std::string& room_code,
                        const std::string& member_id,
                        const std::string& display_name,
                        const std::string& host_secret,
                        const MatchmakingRoom* room_update,
                        std::string& err) override;
    bool fetch_room(const std::string& server_url,
                    const std::string& room_code,
                    MatchmakingRoom& out,
                    std::string& err) override;
    bool list_rooms(const std::string& server_url,
                    std::vector<MatchmakingRoom>& out,
                    std::string& err) override;
};
