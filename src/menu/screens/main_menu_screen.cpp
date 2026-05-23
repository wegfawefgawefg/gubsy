#include "src/menu/screens/main_menu_screen.hpp"

#include "src/engine_state.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"
#include "gubsy/menu/ids.hpp"

#include <vector>

namespace {

MenuCommandId g_cmd_open_settings = kMenuIdInvalid;
MenuCommandId g_cmd_open_profiles = kMenuIdInvalid;
MenuCommandId g_cmd_open_binds = kMenuIdInvalid;

void command_open_settings(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::SETTINGS);
}

void command_open_profiles(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::PROFILES);
}

void command_open_binds(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::BINDS_PROFILES);
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

    MenuWidget play = make_button(101,
                                  SettingsObjectID::CARD0,
                                  "Start Game",
                                  MenuAction::run_command(ctx.engine.main_menu_commands.start_game));
    MenuWidget settings = make_button(102,
                                      SettingsObjectID::CARD1,
                                      "Settings",
                                      MenuAction::run_command(g_cmd_open_settings));
    MenuWidget profiles = make_button(103,
                                      SettingsObjectID::CARD2,
                                      "Profiles",
                                      MenuAction::run_command(g_cmd_open_profiles));
    MenuWidget binds = make_button(104,
                                   SettingsObjectID::CARD3,
                                   "Input Profiles",
                                   MenuAction::run_command(g_cmd_open_binds));
    MenuWidget quit = make_button(105,
                                  SettingsObjectID::BACK,
                                  "Quit",
                                  MenuAction::run_command(ctx.engine.main_menu_commands.quit));

    play.nav_down = settings.id;
    settings.nav_up = play.id;
    settings.nav_down = profiles.id;
    profiles.nav_up = settings.id;
    profiles.nav_down = binds.id;
    binds.nav_up = profiles.id;
    binds.nav_down = quit.id;
    quit.nav_up = binds.id;

    widgets.push_back(play);
    widgets.push_back(settings);
    widgets.push_back(profiles);
    widgets.push_back(binds);
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
    if (g_cmd_open_settings == kMenuIdInvalid)
        g_cmd_open_settings = engine.menu_commands.register_command(command_open_settings);
    if (g_cmd_open_profiles == kMenuIdInvalid)
        g_cmd_open_profiles = engine.menu_commands.register_command(command_open_profiles);
    if (g_cmd_open_binds == kMenuIdInvalid)
        g_cmd_open_binds = engine.menu_commands.register_command(command_open_binds);

    MenuScreenDef def;
    def.id = MenuScreenID::SHELL_MAIN;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_main_menu;
    engine.menu_manager.register_screen(def);
}
