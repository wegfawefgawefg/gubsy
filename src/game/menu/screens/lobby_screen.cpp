#include "game/menu/screens/lobby_screen.hpp"

#include <algorithm>
#include <array>
#include <random>
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
constexpr int kMaxLocalPlayers = 4;

enum class LobbyPage {
    Session = 0,
    GameSettings = 1,
    LocalPlayers = 2,
};

const std::array<const char*, 5> kPrivacyLabels = {
    "Solo",
    "Couch",
    "Friends",
    "Invite Only",
    "Anyone",
};

MenuCommandId g_cmd_privacy_delta = kMenuIdInvalid;
MenuCommandId g_cmd_max_players_delta = kMenuIdInvalid;
MenuCommandId g_cmd_randomize_seed = kMenuIdInvalid;
MenuCommandId g_cmd_open_mods = kMenuIdInvalid;
MenuCommandId g_cmd_browse_servers = kMenuIdInvalid;
MenuCommandId g_cmd_start_game = kMenuIdInvalid;
MenuCommandId g_cmd_open_game_settings = kMenuIdInvalid;
MenuCommandId g_cmd_open_local_players = kMenuIdInvalid;
MenuCommandId g_cmd_open_session = kMenuIdInvalid;
MenuCommandId g_cmd_toggle_local_player = kMenuIdInvalid;

struct LobbyScreenState {
    int page = static_cast<int>(LobbyPage::Session);
};

void command_privacy_delta(MenuContext&, std::int32_t delta) {
    LobbySession& lobby = lobby_state();
    int count = static_cast<int>(kPrivacyLabels.size());
    if (count <= 0)
        return;
    lobby.privacy = (lobby.privacy + delta + count) % count;
}

void command_max_players_delta(MenuContext&, std::int32_t delta) {
    LobbySession& lobby = lobby_state();
    lobby.max_players = std::clamp(lobby.max_players + delta, kMinLobbyPlayers, kMaxLobbyPlayers);
    if (lobby.max_players < kMinLobbyPlayers)
        lobby.max_players = kMaxLobbyPlayers;
}

void command_randomize_seed(MenuContext&, std::int32_t) {
    LobbySession& lobby = lobby_state();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string seed;
    seed.reserve(8);
    for (int i = 0; i < 8; ++i) {
        int v = dist(rng);
        seed.push_back("0123456789ABCDEF"[v]);
    }
    lobby.seed = seed;
    lobby.seed_randomized = true;
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
    auto& st = ctx.state<LobbyScreenState>();
    st.page = static_cast<int>(LobbyPage::GameSettings);
}

void command_open_local_players(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<LobbyScreenState>();
    st.page = static_cast<int>(LobbyPage::LocalPlayers);
}

void command_open_session(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<LobbyScreenState>();
    st.page = static_cast<int>(LobbyPage::Session);
}

void command_toggle_local_player(MenuContext& ctx, std::int32_t index) {
    auto& st = ctx.state<LobbyScreenState>();
    (void)st;
    LobbySession& lobby = lobby_state();
    if (index <= 0 || index >= static_cast<int>(lobby.local_players.size()))
        return;
    bool next = !lobby.local_players[static_cast<std::size_t>(index)];
    if (!next) {
        int active = lobby_local_player_count();
        if (active <= 1)
            return;
    }
    lobby.local_players[static_cast<std::size_t>(index)] = next;
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
    LobbySession& lobby = lobby_state();
    auto& st = ctx.state<LobbyScreenState>();
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
        player_line = std::to_string(local_count) + " / " + std::to_string(kMaxLocalPlayers);
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
                                  (st.page == static_cast<int>(LobbyPage::Session))
                                      ? MenuAction::pop()
                                      : MenuAction::run_command(g_cmd_open_session));

    if (st.page == static_cast<int>(LobbyPage::Session)) {
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

        MenuWidget game_settings = make_button(503,
                                               LobbyObjectID::GAME_SETTINGS,
                                               "Game Settings",
                                               MenuAction::run_command(g_cmd_open_game_settings));
        widgets.push_back(game_settings);

        MenuWidget max_players;
        bool show_max_players = privacy_index != 0;
        if (show_max_players) {
            text_cache.emplace_back(std::to_string(lobby.max_players));
            max_players = make_option_widget(504,
                                             LobbyObjectID::MAX_PLAYERS,
                                             "Max Players",
                                             text_cache.back().c_str(),
                                             MenuAction::run_command(g_cmd_max_players_delta, -1),
                                             MenuAction::run_command(g_cmd_max_players_delta, +1));
            if (privacy_index == 1) {
                max_players.on_left = MenuAction::none();
                max_players.on_right = MenuAction::none();
                max_players.style.bg_r = 30;
                max_players.style.bg_g = 30;
                max_players.style.bg_b = 40;
                max_players.style.fg_r = 150;
                max_players.style.fg_g = 150;
                max_players.style.fg_b = 170;
            }
            widgets.push_back(max_players);
        }

        MenuWidget local_players = make_button(506,
                                               LobbyObjectID::LOCAL_PLAYERS,
                                               "Local Players",
                                               MenuAction::run_command(g_cmd_open_local_players));
        widgets.push_back(local_players);

        widgets.push_back(manage_mods);
        widgets.push_back(start_game);
        widgets.push_back(back);
        default_focus = lobby_name.id;

        // Navigation links (session page)
        lobby_name.nav_down = privacy.id;
        privacy.nav_up = lobby_name.id;
        if (show_max_players) {
            privacy.nav_down = max_players.id;
            max_players.nav_up = privacy.id;
            max_players.nav_down = game_settings.id;
            game_settings.nav_up = max_players.id;
        } else {
            privacy.nav_down = game_settings.id;
            game_settings.nav_up = privacy.id;
        }
        game_settings.nav_down = local_players.id;
        local_players.nav_up = game_settings.id;
        local_players.nav_down = manage_mods.id;
        manage_mods.nav_up = local_players.id;
        manage_mods.nav_down = start_game.id;
        start_game.nav_up = manage_mods.id;
        back.nav_up = start_game.id;
        start_game.nav_left = back.id;
        back.nav_right = start_game.id;

        lobby_name.nav_right = manage_mods.id;
        privacy.nav_right = manage_mods.id;
        game_settings.nav_right = manage_mods.id;
        local_players.nav_right = manage_mods.id;
        if (show_max_players)
            max_players.nav_right = manage_mods.id;

        browse.nav_down = manage_mods.id;
        browse.nav_left = players_panel.id;
        players_panel.nav_down = manage_mods.id;
        manage_mods.nav_up = browse.id;
    } else if (st.page == static_cast<int>(LobbyPage::GameSettings)) {
        MenuWidget seed;
        seed.id = 506;
        seed.slot = LobbyObjectID::SEED;
        seed.type = WidgetType::TextInput;
        seed.label = "Seed";
        seed.text_buffer = &lobby.seed;
        seed.text_max_len = 24;
        seed.placeholder = "Random";
        widgets.push_back(seed);

        MenuWidget randomize = make_button(507,
                                           LobbyObjectID::RANDOMIZE_SEED,
                                           "Randomize Seed",
                                           MenuAction::run_command(g_cmd_randomize_seed));
        widgets.push_back(randomize);

        MenuWidget session_btn = make_button(503,
                                             LobbyObjectID::GAME_SETTINGS,
                                             "Back to Lobby",
                                             MenuAction::run_command(g_cmd_open_session));
        widgets.push_back(session_btn);

        widgets.push_back(manage_mods);
        widgets.push_back(start_game);
        widgets.push_back(back);
        default_focus = seed.id;

        seed.nav_down = randomize.id;
        randomize.nav_up = seed.id;
        randomize.nav_down = session_btn.id;
        session_btn.nav_up = randomize.id;
        session_btn.nav_down = manage_mods.id;
        manage_mods.nav_up = session_btn.id;
        manage_mods.nav_down = start_game.id;
        start_game.nav_up = manage_mods.id;
        back.nav_up = start_game.id;
        start_game.nav_left = back.id;
        back.nav_right = start_game.id;
        browse.nav_down = manage_mods.id;
        browse.nav_left = players_panel.id;
        players_panel.nav_down = manage_mods.id;
    } else {
        std::vector<WidgetId> player_ids;
        player_ids.reserve(lobby.local_players.size());
        for (std::size_t i = 0; i < lobby.local_players.size(); ++i) {
            WidgetId widget_id = static_cast<WidgetId>(520 + static_cast<WidgetId>(i));
            UILayoutObjectId slot =
                static_cast<UILayoutObjectId>(LobbyObjectID::PLAYER0 + static_cast<int>(i));
            MenuWidget player_btn;
            player_btn.id = widget_id;
            player_btn.slot = slot;
            player_btn.type = WidgetType::Button;
            std::string label = "Player " + std::to_string(i + 1);
            std::string status = lobby.local_players[i] ? "Enabled" : "Disabled";
            text_cache.emplace_back(label);
            player_btn.label = text_cache.back().c_str();
            text_cache.emplace_back(status);
            player_btn.badge = text_cache.back().c_str();
            if (i == 0) {
                player_btn.badge = "Active";
            } else {
                player_btn.on_select = MenuAction::run_command(g_cmd_toggle_local_player,
                                                               static_cast<std::int32_t>(i));
            }
            widgets.push_back(player_btn);
            player_ids.push_back(widget_id);
        }

        MenuWidget session_btn = make_button(503,
                                             LobbyObjectID::GAME_SETTINGS,
                                             "Back to Lobby",
                                             MenuAction::run_command(g_cmd_open_session));
        widgets.push_back(session_btn);

        widgets.push_back(manage_mods);
        widgets.push_back(start_game);
        widgets.push_back(back);
        if (!player_ids.empty())
            default_focus = player_ids.front();

        for (std::size_t i = 0; i < player_ids.size(); ++i) {
            MenuWidget& player_btn = widgets[widgets.size() - 4 - player_ids.size() + i];
            player_btn.nav_up = (i == 0) ? player_btn.id : player_ids[i - 1];
            player_btn.nav_down = (i + 1 < player_ids.size()) ? player_ids[i + 1] : session_btn.id;
            player_btn.nav_right = manage_mods.id;
        }
        session_btn.nav_up = player_ids.empty() ? session_btn.id : player_ids.back();
        session_btn.nav_down = manage_mods.id;
        manage_mods.nav_up = session_btn.id;
        manage_mods.nav_down = start_game.id;
        start_game.nav_up = manage_mods.id;
        back.nav_up = start_game.id;
        start_game.nav_left = back.id;
        back.nav_right = start_game.id;
        browse.nav_down = manage_mods.id;
        browse.nav_left = players_panel.id;
        players_panel.nav_down = manage_mods.id;
    }

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
    if (g_cmd_randomize_seed == kMenuIdInvalid)
        g_cmd_randomize_seed = es->menu_commands.register_command(command_randomize_seed);
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
    if (g_cmd_open_session == kMenuIdInvalid)
        g_cmd_open_session = es->menu_commands.register_command(command_open_session);
    if (g_cmd_toggle_local_player == kMenuIdInvalid)
        g_cmd_toggle_local_player = es->menu_commands.register_command(command_toggle_local_player);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY;
    def.layout = UILayoutID::LOBBY_SCREEN;
    def.state_ops = screen_state_ops<LobbyScreenState>();
    def.build = build_lobby;
    es->menu_manager.register_screen(def);
}
