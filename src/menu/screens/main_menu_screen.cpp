#include "src/menu/screens/main_menu_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <vector>

namespace {

MenuCommandId g_cmd_open_lobby = kMenuIdInvalid;
MenuCommandId g_cmd_open_settings = kMenuIdInvalid;

void command_open_lobby(MenuContext& ctx, std::int32_t) {
    ctx.engine.menu_context = GubsyMenuContext::Lobby;
    ctx.manager.push_screen(MenuScreenID::SHELL_LOBBY);
}

void command_open_settings(MenuContext& ctx, std::int32_t) {
    ctx.engine.menu_context = GubsyMenuContext::Title;
    ctx.manager.push_screen(MenuScreenID::SETTINGS);
}

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

BuiltScreen build_main_menu(MenuContext& ctx) {
    (void)ctx;
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    widgets.clear();
    frame_actions.clear();

    MenuWidget title;
    title.id = 100;
    title.slot = SettingsObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Main Menu";
    widgets.push_back(title);

    MenuWidget play = make_button(101, SettingsObjectID::CARD0, "Play",
                                  MenuAction::run_command(g_cmd_open_lobby));
    MenuWidget settings = make_button(102, SettingsObjectID::CARD1, "Settings",
                                      MenuAction::run_command(g_cmd_open_settings));
    MenuWidget quit = make_button(105, SettingsObjectID::CARD2, "Quit",
                                  MenuAction::run_command(ctx.engine.main_menu_commands.quit));

    play.nav_down = settings.id;
    settings.nav_up = play.id;
    settings.nav_down = quit.id;
    quit.nav_up = settings.id;

    widgets.push_back(play);
    widgets.push_back(settings);
    widgets.push_back(quit);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = play.id;
    return built;
}

} // namespace

void register_main_menu_screen(EngineState& engine) {
    if (g_cmd_open_lobby == kMenuIdInvalid)
        g_cmd_open_lobby = engine.menu_commands.register_command(command_open_lobby);
    if (g_cmd_open_settings == kMenuIdInvalid)
        g_cmd_open_settings = engine.menu_commands.register_command(command_open_settings);

    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_MAIN;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_main_menu;
    engine.menu_manager.register_screen(def);
}
