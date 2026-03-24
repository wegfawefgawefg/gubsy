#pragma once

#include <string>

struct LobbySession;

bool lobby_online_host_current_room(LobbySession& lobby, std::string& err);
bool lobby_online_join_room(LobbySession& lobby, const std::string& room_code, std::string& err);
bool lobby_online_leave_room(LobbySession& lobby, std::string& err);
bool lobby_online_refresh_rooms(LobbySession& lobby, bool force, std::string& err);
void lobby_online_tick(LobbySession& lobby);

