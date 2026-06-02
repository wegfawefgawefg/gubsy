#pragma once

#include "gubsy/lobby/matchmaking.hpp"

struct RoomServerRendezvousUdpCapabilities {
    bool enabled{false};
    std::string host;
    int port{0};
    std::string protocol;
};

struct RoomServerCapabilities {
    bool ok{false};
    RoomServerRendezvousUdpCapabilities rendezvous_udp;
};

class RoomServerMatchmaking final : public IMatchmaking {
public:
    bool fetch_capabilities(const std::string& server_url,
                            RoomServerCapabilities& out,
                            std::string& err);
    bool create_room(const std::string& server_url,
                     const MatchmakingRoom& room,
                     MatchmakingCreateResult& out,
                     std::string& err) override;
    bool join_room(const std::string& server_url,
                   const std::string& room_code,
                   const std::string& display_name,
                   const std::string& join_token,
                   std::string& member_id_out,
                   std::string& err) override;
    bool create_join_attempt(const std::string& server_url,
                             const std::string& room_code,
                             const std::string& display_name,
                             MatchmakingJoinAttemptResult& out,
                             std::string& err) override;
    bool leave_room(const std::string& server_url,
                    const std::string& room_code,
                    const std::string& member_id,
                    const std::string& host_secret,
                    std::string& err) override;
    bool remove_member(const std::string& server_url,
                       const std::string& room_code,
                       const std::string& host_secret,
                       const std::string& target_member_id,
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
