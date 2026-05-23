#include "src/menu/screens/shell_lobby_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <vector>

namespace {

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

BuiltScreen build_shell_lobby(MenuContext& ctx) {
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    widgets.clear();
    frame_actions.clear();

    MenuWidget title;
    title.id = 200;
    title.slot = SettingsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Lobby";
    title.secondary = "Local session shell";
    widgets.push_back(title);

    MenuWidget players;
    players.id = 201;
    players.slot = SettingsObjectID::CARD0;
    players.type = WidgetType::Card;
    players.label = "Players";
    players.secondary = "Local player selection will live here.";
    widgets.push_back(players);

    MenuWidget settings = make_button(202,
                                      SettingsObjectID::CARD1,
                                      "Settings",
                                      MenuAction::push(MenuScreenID::SETTINGS));
    MenuWidget profiles = make_button(203,
                                      SettingsObjectID::CARD2,
                                      "Profiles",
                                      MenuAction::push(MenuScreenID::PROFILES));
    MenuWidget start = make_button(204,
                                   SettingsObjectID::CARD3,
                                   "Start Game",
                                   MenuAction::run_command(ctx.engine.main_menu_commands.start_game));
    MenuWidget back = make_button(205, SettingsObjectID::BACK, "Back", MenuAction::pop());

    players.nav_down = settings.id;
    settings.nav_up = players.id;
    settings.nav_down = profiles.id;
    profiles.nav_up = settings.id;
    profiles.nav_down = start.id;
    start.nav_up = profiles.id;
    start.nav_down = back.id;
    back.nav_up = start.id;

    widgets.push_back(settings);
    widgets.push_back(profiles);
    widgets.push_back(start);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = start.id;
    return built;
}

} // namespace

void register_shell_lobby_screen(EngineState& engine) {
    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_LOBBY;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_shell_lobby;
    engine.menu_manager.register_screen(def);
}
