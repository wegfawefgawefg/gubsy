#include "game/menu/screens/main_menu_screen.hpp"

#include "engine/globals.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_commands.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"
#include "game/actions.hpp"
#include "engine/mode_registry.hpp"

#include <vector>

namespace {

MenuCommandId g_cmd_play = kMenuIdInvalid;
MenuCommandId g_cmd_quit = kMenuIdInvalid;
MenuCommandId g_cmd_open_mods = kMenuIdInvalid;

void command_play(MenuContext& ctx, std::int32_t) {
    ctx.engine.mode = modes::SETUP;
}

void command_quit(MenuContext& ctx, std::int32_t) {
    ctx.engine.running = false;
}

void command_open_mods(MenuContext& ctx, std::int32_t) {
    ctx.manager.push_screen(MenuScreenID::MODS);
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

BuiltScreen build_main_menu(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    widgets.clear();
    frame_actions.clear();

    MenuWidget title;
    title.id = 100;
    title.slot = MenuObjectID::TITLE;
    title.type = WidgetType::Label;
    title.label = "Main Menu";
    widgets.push_back(title);

    MenuWidget play = make_button(101, MenuObjectID::PLAY, "Play", MenuAction::run_command(g_cmd_play));
    MenuWidget mods = make_button(102, MenuObjectID::MODS, "Mods", MenuAction::run_command(g_cmd_open_mods));
    MenuWidget settings = make_button(103, MenuObjectID::SETTINGS, "Settings (coming soon)", MenuAction::none());
    MenuWidget quit = make_button(104, MenuObjectID::QUIT, "Quit", MenuAction::run_command(g_cmd_quit));

    play.nav_down = mods.id;
    mods.nav_up = play.id;
    mods.nav_down = settings.id;
    settings.nav_up = mods.id;
    settings.nav_down = quit.id;
    quit.nav_up = settings.id;

    widgets.push_back(play);
    widgets.push_back(mods);
    widgets.push_back(settings);
    widgets.push_back(quit);

    BuiltScreen built;
    built.layout = UILayoutID::MENU_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = play.id;
    return built;
}

} // namespace

void register_main_menu_screen() {
    if (!es)
        return;

    g_cmd_play = es->menu_commands.register_command(command_play);
    g_cmd_quit = es->menu_commands.register_command(command_quit);
    g_cmd_open_mods = es->menu_commands.register_command(command_open_mods);

    MenuScreenDef def;
    def.id = MenuScreenID::MAIN;
    def.layout = UILayoutID::MENU_SCREEN;
    def.state_ops = screen_state_ops<int>(); // trivial placeholder
    def.build = build_main_menu;
    es->menu_manager.register_screen(def);
}
