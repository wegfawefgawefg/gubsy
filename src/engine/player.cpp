#include "engine/player.hpp"

#include "engine/globals.hpp"

int add_player() {
    if (!es)
        return -1;
    Player player;
    player.has_active_profile = false;
    es->players.push_back(player);
    return static_cast<int>(es->players.size()) - 1;
}

void remove_player(int player_index) {
    if (!es)
        return;
    if (player_index < 0 || player_index >= static_cast<int>(es->players.size()))
        return;
    es->players.erase(es->players.begin() + player_index);
}
