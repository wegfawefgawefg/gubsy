#include "game/menu/screens/lobby_screen.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/mod_host.hpp"
#include "engine/player.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/modes.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kMinLobbyPlayers = 1;
constexpr int kMaxLobbyPlayers = 64;

const std::array<const char*, 5> kPrivacyLabels = {
    "Solo",
    "Couch",
    "Friends",
    "Invite Only",
    "Anyone",
};

MenuCommandId g_cmd_privacy_delta = kMenuIdInvalid;
MenuCommandId g_cmd_max_players_delta = kMenuIdInvalid;
MenuCommandId g_cmd_open_mods = kMenuIdInvalid;
MenuCommandId g_cmd_browse_servers = kMenuIdInvalid;
MenuCommandId g_cmd_start_game = kMenuIdInvalid;
MenuCommandId g_cmd_open_game_settings = kMenuIdInvalid;
MenuCommandId g_cmd_open_local_players = kMenuIdInvalid;

void clear_drop_notice(LobbySession& lobby) {
    lobby.dropped_players_notice = false;
    lobby.dropped_players_count = 0;
}

void trim_players_to_max(LobbySession& lobby, int max_players, bool set_notice) {
    if (!es)
        return;
    int dropped = 0;
    while (es->players.size() > static_cast<std::size_t>(max_players)) {
        const Player& player = es->players.back();
        if (player.has_active_profile)
            lobby.cached_profile_ids.push_back(player.profile.id);
        remove_player(static_cast<int>(es->players.size()) - 1);
        ++dropped;
    }
    if (dropped > 0 && set_notice) {
        lobby.dropped_players_notice = true;
        lobby.dropped_players_count += dropped;
    }
    if (!es->players.empty())
        lobby.selected_player_index =
            std::clamp(lobby.selected_player_index, 0, static_cast<int>(es->players.size()) - 1);
}

void command_privacy_delta(MenuContext&, std::int32_t delta) {
    LobbySession& lobby = lobby_state();
    int count = static_cast<int>(kPrivacyLabels.size());
    if (count <= 0)
        return;
    lobby.privacy = (lobby.privacy + delta + count) % count;
    clear_drop_notice(lobby);
    if (lobby.privacy == 0 && es) {
        trim_players_to_max(lobby, 1, false);
        lobby.selected_player_index = 0;
    }
    if (lobby.privacy >= 2) {
        int local_count = lobby_local_player_count();
        if (lobby.max_players < local_count)
            lobby.max_players = local_count;
    }
}

void command_max_players_delta(MenuContext&, std::int32_t delta) {
    LobbySession& lobby = lobby_state();
    lobby.max_players = std::clamp(lobby.max_players + delta, kMinLobbyPlayers, kMaxLobbyPlayers);
    if (lobby.max_players < kMinLobbyPlayers)
        lobby.max_players = kMaxLobbyPlayers;
    clear_drop_notice(lobby);
    if (lobby.privacy >= 2) {
        int local_count = lobby_local_player_count();
        if (local_count > lobby.max_players)
            trim_players_to_max(lobby, lobby.max_players, true);
    }
}

void command_open_mods(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::LOBBY_MODS);
}

void command_browse_servers(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::SERVER_BROWSER);
}

void command_start_game(MenuContext& ctx, std::int32_t) {
    lobby_refresh_mods();
    auto enabled = lobby_enabled_mod_ids();
    set_active_mods(enabled);
    ctx.engine.mode = modes::PLAYING;
}

void command_open_game_settings(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::GAME_SETTINGS);
}

void command_open_local_players(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::LOCAL_PLAYERS);
}

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction on_select) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = on_select;
    return widget;
}

MenuWidget make_option_widget(WidgetId id,
                              UILayoutObjectId slot,
                              const char* label,
                              const char* badge,
                              MenuAction on_left,
                              MenuAction on_right) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::OptionCycle;
    widget.label = label;
    widget.badge = badge;
    widget.on_left = on_left;
    widget.on_right = on_right;
    return widget;
}

std::string default_lobby_name() {
    if (UserProfile* profile = get_player_user_profile(0)) {
        if (!profile->name.empty())
            return profile->name + "'s Lobby";
    }
    return "Local Lobby";
}

BuiltScreen build_lobby(MenuContext& ctx) {
    (void)ctx;
    LobbySession& lobby = lobby_state();
    lobby_refresh_mods();
    if (!lobby.name_initialized) {
        lobby.session_name = default_lobby_name();
        lobby.name_initialized = true;
    }

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(8);

    MenuWidget title;
    title.id = 500;
    title.slot = LobbyObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Session Lobby";
    widgets.push_back(title);

    MenuWidget browse = make_button(514, LobbyObjectID::BROWSE_SERVERS, "Browse Servers",
                                    MenuAction::run_command(g_cmd_browse_servers));
    widgets.push_back(browse);
    WidgetId default_focus = browse.id;

    int privacy_index = std::clamp(lobby.privacy, 0, static_cast<int>(kPrivacyLabels.size()) - 1);

    int local_count = lobby_local_player_count();
    std::string player_line;
    if (privacy_index == 0) {
        player_line = "1 / 1";
    } else if (privacy_index == 1) {
        player_line = std::to_string(local_count) + " / " + std::to_string(local_count);
    } else {
        player_line = std::to_string(local_count) + " / " + std::to_string(lobby.max_players);
    }
    text_cache.emplace_back(std::move(player_line));
    MenuWidget players_panel;
    players_panel.id = 509;
    players_panel.slot = LobbyObjectID::PLAYERS_PANEL;
    players_panel.type = WidgetType::Card;
    players_panel.label = "Players";
    players_panel.secondary = text_cache.back().c_str();
    if (lobby.dropped_players_notice && lobby.dropped_players_count > 0) {
        std::string notice = "Dropped " + std::to_string(lobby.dropped_players_count) +
                             (lobby.dropped_players_count == 1 ? " player" : " players");
        text_cache.emplace_back(std::move(notice));
        players_panel.badge = text_cache.back().c_str();
        players_panel.badge_color = SDL_Color{240, 205, 120, 255};
    }
    players_panel.nav_right = browse.id;
    widgets.push_back(players_panel);

    MenuWidget manage_mods = make_button(510,
                                         LobbyObjectID::MANAGE_MODS,
                                         "Manage Mods",
                                         MenuAction::run_command(g_cmd_open_mods));

    MenuWidget start_game = make_button(511,
                                        LobbyObjectID::START_GAME,
                                        "Start Game",
                                        MenuAction::run_command(g_cmd_start_game));

    MenuWidget back = make_button(512,
                                  LobbyObjectID::BACK,
                                  "Back",
                                  MenuAction::pop());

    MenuWidget lobby_name;
    lobby_name.id = 501;
    lobby_name.slot = LobbyObjectID::LOBBY_NAME;
    lobby_name.type = WidgetType::TextInput;
    lobby_name.label = "Lobby Name";
    lobby_name.text_buffer = &lobby.session_name;
    lobby_name.text_max_len = 48;
    lobby_name.placeholder = "Local Lobby";
    widgets.push_back(lobby_name);

    MenuWidget privacy = make_option_widget(502,
                                            LobbyObjectID::PRIVACY,
                                            "Privacy",
                                            kPrivacyLabels[static_cast<std::size_t>(privacy_index)],
                                            MenuAction::run_command(g_cmd_privacy_delta, -1),
                                            MenuAction::run_command(g_cmd_privacy_delta, +1));
    widgets.push_back(privacy);

    MenuWidget max_players;
    bool show_max_players = privacy_index > 1;
    if (show_max_players) {
        text_cache.emplace_back(std::to_string(lobby.max_players));
        max_players = make_option_widget(504,
                                         LobbyObjectID::MAX_PLAYERS,
                                         "Max Players",
                                         text_cache.back().c_str(),
                                         MenuAction::run_command(g_cmd_max_players_delta, -1),
                                         MenuAction::run_command(g_cmd_max_players_delta, +1));
        widgets.push_back(max_players);
    }

    MenuWidget local_players = make_button(506,
                                           LobbyObjectID::LOCAL_PLAYERS,
                                           "Player Settings",
                                           MenuAction::run_command(g_cmd_open_local_players));
    local_players.secondary = "Add Players / Player Settings";
    widgets.push_back(local_players);

    MenuWidget game_settings = make_button(503,
                                           LobbyObjectID::GAME_SETTINGS,
                                           "Game Settings",
                                           MenuAction::run_command(g_cmd_open_game_settings));
    widgets.push_back(game_settings);

    widgets.push_back(manage_mods);
    widgets.push_back(start_game);
    widgets.push_back(back);
    default_focus = lobby_name.id;

    lobby_name.nav_down = privacy.id;
    privacy.nav_up = lobby_name.id;
    if (show_max_players) {
        privacy.nav_down = max_players.id;
        max_players.nav_up = privacy.id;
        max_players.nav_down = local_players.id;
        local_players.nav_up = max_players.id;
    } else {
        privacy.nav_down = local_players.id;
        local_players.nav_up = privacy.id;
    }
    local_players.nav_down = game_settings.id;
    game_settings.nav_up = local_players.id;
    game_settings.nav_down = manage_mods.id;
    manage_mods.nav_up = game_settings.id;
    manage_mods.nav_down = start_game.id;
    start_game.nav_up = manage_mods.id;
    back.nav_up = start_game.id;
    start_game.nav_left = back.id;
    back.nav_right = start_game.id;

    lobby_name.nav_right = manage_mods.id;
    privacy.nav_right = manage_mods.id;
    local_players.nav_right = manage_mods.id;
    game_settings.nav_right = manage_mods.id;
    if (show_max_players)
        max_players.nav_right = manage_mods.id;

    browse.nav_down = manage_mods.id;
    browse.nav_left = players_panel.id;
    players_panel.nav_down = manage_mods.id;
    manage_mods.nav_up = browse.id;

    BuiltScreen built;
    built.layout = UILayoutID::LOBBY_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = default_focus;
    return built;
}

} // namespace

void register_lobby_screen() {
    if (!es)
        return;

    if (g_cmd_privacy_delta == kMenuIdInvalid)
        g_cmd_privacy_delta = es->menu_commands.register_command(command_privacy_delta);
    if (g_cmd_max_players_delta == kMenuIdInvalid)
        g_cmd_max_players_delta = es->menu_commands.register_command(command_max_players_delta);
    if (g_cmd_open_mods == kMenuIdInvalid)
        g_cmd_open_mods = es->menu_commands.register_command(command_open_mods);
    if (g_cmd_browse_servers == kMenuIdInvalid)
        g_cmd_browse_servers = es->menu_commands.register_command(command_browse_servers);
    if (g_cmd_start_game == kMenuIdInvalid)
        g_cmd_start_game = es->menu_commands.register_command(command_start_game);
    if (g_cmd_open_game_settings == kMenuIdInvalid)
        g_cmd_open_game_settings = es->menu_commands.register_command(command_open_game_settings);
    if (g_cmd_open_local_players == kMenuIdInvalid)
        g_cmd_open_local_players = es->menu_commands.register_command(command_open_local_players);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY;
    def.layout = UILayoutID::LOBBY_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_lobby;
    es->menu_manager.register_screen(def);
}
