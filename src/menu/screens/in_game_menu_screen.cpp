#include "src/menu/screens/in_game_menu_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <vector>

namespace {

constexpr WidgetId kTitleWidgetId = 2900;
constexpr WidgetId kStatusWidgetId = 2901;
constexpr WidgetId kResumeWidgetId = 2920;
constexpr WidgetId kSettingsWidgetId = 2921;
constexpr WidgetId kRestartWidgetId = 2922;
constexpr WidgetId kQuitWidgetId = 2923;
constexpr WidgetId kBackWidgetId = 2930;

MenuCommandId g_cmd_resume = kMenuIdInvalid;
MenuCommandId g_cmd_open_settings = kMenuIdInvalid;
MenuCommandId g_cmd_restart_run = kMenuIdInvalid;
MenuCommandId g_cmd_quit_to_main_menu = kMenuIdInvalid;

MenuWidget make_label(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Label;
    widget.label = label;
    return widget;
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

void command_resume(MenuContext& ctx, std::int32_t) {
    if (ctx.engine.in_game_menu_commands.resume != kMenuIdInvalid) {
        ctx.engine.menu_commands.invoke(ctx, ctx.engine.in_game_menu_commands.resume, 0);
        return;
    }
    ctx.engine.menu_manager.clear();
    ctx.engine.menu_context = GubsyMenuContext::None;
}

void command_open_settings(MenuContext& ctx, std::int32_t) {
    ctx.engine.menu_context = GubsyMenuContext::InGame;
    ctx.manager.push_screen(MenuScreenID::SETTINGS);
}

void command_restart_run(MenuContext& ctx, std::int32_t) {
    if (ctx.engine.in_game_menu_commands.restart_run != kMenuIdInvalid)
        ctx.engine.menu_commands.invoke(ctx, ctx.engine.in_game_menu_commands.restart_run, 0);
}

void command_quit_to_main_menu(MenuContext& ctx, std::int32_t) {
    if (ctx.engine.in_game_menu_commands.quit_to_main_menu != kMenuIdInvalid)
        ctx.engine.menu_commands.invoke(ctx, ctx.engine.in_game_menu_commands.quit_to_main_menu, 0);
}

BuiltScreen build_in_game_menu(MenuContext& ctx) {
    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Paused"));
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, "In-game menu"));

    MenuAction resume_action = MenuAction::run_command(g_cmd_resume);
    MenuWidget resume =
        make_button(kResumeWidgetId, SettingsObjectID::CARD0, "Resume", resume_action);
    resume.secondary = "Return to the game.";
    MenuWidget settings = make_button(kSettingsWidgetId, SettingsObjectID::CARD1, "Settings",
                                      MenuAction::run_command(g_cmd_open_settings));
    settings.secondary = "Audio, video, controls, binds, and profiles.";
    MenuWidget quit = make_button(kQuitWidgetId, SettingsObjectID::CARD3, "Quit to Main Menu",
                                  MenuAction::run_command(g_cmd_quit_to_main_menu));
    quit.secondary = "Leave the current run.";
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", resume_action);

    resume.nav_down = settings.id;
    settings.nav_up = resume.id;
    const bool restart_available = ctx.engine.in_game_menu_commands.restart_run != kMenuIdInvalid;
    if (restart_available) {
        settings.nav_down = kRestartWidgetId;
        quit.nav_up = kRestartWidgetId;
    } else {
        settings.nav_down = quit.id;
        quit.nav_up = settings.id;
    }
    quit.nav_down = back.id;
    back.nav_up = quit.id;

    resume.on_back = resume_action;
    settings.on_back = resume_action;
    quit.on_back = resume_action;
    back.on_back = resume_action;

    widgets.push_back(resume);
    widgets.push_back(settings);
    if (restart_available) {
        MenuWidget restart = make_button(kRestartWidgetId, SettingsObjectID::CARD2, "Restart Run",
                                         MenuAction::run_command(g_cmd_restart_run));
        restart.secondary = "Restart from the first level.";
        restart.nav_up = settings.id;
        restart.nav_down = quit.id;
        restart.on_back = resume_action;
        widgets.push_back(restart);
    }
    widgets.push_back(quit);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = resume.id;
    return built;
}

} // namespace

void register_in_game_menu_screen(EngineState& engine) {
    g_cmd_resume = engine.menu_commands.register_command(command_resume);
    g_cmd_open_settings = engine.menu_commands.register_command(command_open_settings);
    g_cmd_restart_run = engine.menu_commands.register_command(command_restart_run);
    g_cmd_quit_to_main_menu = engine.menu_commands.register_command(command_quit_to_main_menu);

    MenuScreenDef def;
    def.id = MenuScreenID::IN_GAME_MENU;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_in_game_menu;
    engine.menu_manager.register_screen(def);
}
