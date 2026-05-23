#include "src/menu/screens/shell_lobby_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"
#include "src/player.hpp"
#include "src/user_profiles.hpp"

#include <string>
#include <vector>

namespace {

MenuCommandId g_cmd_cycle_player_profile = kMenuIdInvalid;

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

int find_profile_index(const std::vector<UserProfile>& profiles, int profile_id) {
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (profiles[i].id == profile_id)
            return static_cast<int>(i);
    }
    return -1;
}

void ensure_profile_pool(EngineState& engine) {
    if (!engine.user_profiles_pool.empty())
        return;
    engine.user_profiles_pool.push_back(create_default_user_profile());
}

void command_cycle_player_profile(MenuContext& ctx, std::int32_t) {
    ensure_profile_pool(ctx.engine);
    if (ctx.engine.players.empty()) {
        add_player(ctx.engine, 0);
        return;
    }

    UserProfile* current = get_player_user_profile(ctx.engine, 0);
    int current_index = current ? find_profile_index(ctx.engine.user_profiles_pool, current->id) : -1;
    int next_index = current_index + 1;
    if (next_index >= static_cast<int>(ctx.engine.user_profiles_pool.size()))
        next_index = 0;
    set_user_profile_for_player(ctx.engine, 0, ctx.engine.user_profiles_pool[static_cast<std::size_t>(next_index)].id);
}

BuiltScreen build_shell_lobby(MenuContext& ctx) {
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    static std::vector<std::string> text_cache;
    widgets.clear();
    frame_actions.clear();
    text_cache.clear();

    MenuWidget title;
    title.id = 200;
    title.slot = SettingsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Lobby";
    title.secondary = "Local session shell";
    widgets.push_back(title);

    ensure_profile_pool(ctx.engine);

    MenuWidget player;
    player.id = 201;
    player.slot = SettingsObjectID::CARD0;
    player.type = WidgetType::Card;
    player.on_select = MenuAction::run_command(g_cmd_cycle_player_profile);
    UserProfile* active_profile = get_player_user_profile(ctx.engine, 0);
    if (active_profile) {
        text_cache.push_back("Player 1: " + active_profile->name);
        text_cache.push_back("Select to cycle profile.");
    } else {
        text_cache.push_back("Add Local Player");
        text_cache.push_back("Select to join with " + ctx.engine.user_profiles_pool.front().name + ".");
    }
    player.label = text_cache[0].c_str();
    player.secondary = text_cache[1].c_str();
    widgets.push_back(player);

    MenuWidget profiles = make_button(202,
                                      SettingsObjectID::CARD1,
                                      "Profiles",
                                      MenuAction::push(MenuScreenID::PROFILES));
    MenuWidget settings = make_button(203,
                                      SettingsObjectID::CARD2,
                                      "Settings",
                                      MenuAction::push(MenuScreenID::SETTINGS));
    MenuWidget start = make_button(204,
                                   SettingsObjectID::CARD3,
                                   "Start Game",
                                   MenuAction::run_command(ctx.engine.main_menu_commands.start_game));
    MenuWidget back = make_button(205, SettingsObjectID::BACK, "Back", MenuAction::pop());

    player.nav_down = profiles.id;
    profiles.nav_up = player.id;
    profiles.nav_down = settings.id;
    settings.nav_up = profiles.id;
    settings.nav_down = start.id;
    start.nav_up = settings.id;
    start.nav_down = back.id;
    back.nav_up = start.id;

    widgets.push_back(profiles);
    widgets.push_back(settings);
    widgets.push_back(start);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = player.id;
    return built;
}

} // namespace

void register_shell_lobby_screen(EngineState& engine) {
    if (g_cmd_cycle_player_profile == kMenuIdInvalid)
        g_cmd_cycle_player_profile = engine.menu_commands.register_command(command_cycle_player_profile);

    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_LOBBY;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_shell_lobby;
    engine.menu_manager.register_screen(def);
}
