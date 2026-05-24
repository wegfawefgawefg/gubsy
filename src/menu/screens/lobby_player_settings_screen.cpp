#include "src/menu/screens/lobby_player_settings_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/input_settings_profiles.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <string>
#include <vector>

namespace {

constexpr WidgetId kTitleWidgetId = 2500;
constexpr WidgetId kStatusWidgetId = 2501;
constexpr WidgetId kProfileWidgetId = 2520;
constexpr WidgetId kBindsWidgetId = 2521;
constexpr WidgetId kInputWidgetId = 2522;
constexpr WidgetId kDeviceWidgetId = 2523;
constexpr WidgetId kRemoveWidgetId = 2524;
constexpr WidgetId kBackWidgetId = 2530;

MenuCommandId g_cmd_open_profile = kMenuIdInvalid;
MenuCommandId g_cmd_open_binds = kMenuIdInvalid;
MenuCommandId g_cmd_open_input = kMenuIdInvalid;
MenuCommandId g_cmd_open_devices = kMenuIdInvalid;
MenuCommandId g_cmd_remove_player = kMenuIdInvalid;

MenuWidget make_label(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Label;
    widget.label = label;
    return widget;
}

MenuWidget make_card(WidgetId id,
                     UILayoutObjectId slot,
                     const char* label,
                     const char* secondary,
                     MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Card;
    widget.label = label;
    widget.secondary = secondary;
    widget.on_select = action;
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

void push_for_player(MenuContext& ctx, MenuScreenId screen_id) {
    ctx.manager.push_screen(screen_id, ctx.player_index);
}

void command_open_profile(MenuContext& ctx, std::int32_t) {
    push_for_player(ctx, MenuScreenID::LOBBY_PROFILE_PICKER);
}

void command_open_binds(MenuContext& ctx, std::int32_t) {
    push_for_player(ctx, MenuScreenID::LOBBY_BINDS_PICKER);
}

void command_open_input(MenuContext& ctx, std::int32_t) {
    push_for_player(ctx, MenuScreenID::LOBBY_INPUT_SETTINGS_PICKER);
}

void command_open_devices(MenuContext& ctx, std::int32_t) {
    push_for_player(ctx, MenuScreenID::LOBBY_DEVICE_PICKER);
}

void command_remove_player(MenuContext& ctx, std::int32_t) {
    gubsy_lobby_remove_local_player(ctx.engine, ctx.player_index);
    ctx.manager.pop_screen();
}

std::string profile_name_or_missing(UserProfile* profile) {
    return profile ? profile->name : "Missing";
}

std::string binds_name_or_missing(BindsProfile* profile) {
    return profile ? profile->name : "Missing";
}

std::string input_name_or_missing(InputSettingsProfile* profile) {
    return profile ? profile->name : "Missing";
}

BuiltScreen build_player_settings(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    gubsy_lobby_select_player(ctx.engine, ctx.player_index);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    text_cache.push_back(gubsy_lobby_player_label(ctx.engine, ctx.player_index));
    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, text_cache.back().c_str()));
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, "Player setup"));

    text_cache.push_back(profile_name_or_missing(gubsy_lobby_user_profile(ctx.engine,
                                                                          ctx.player_index)));
    MenuWidget profile = make_card(kProfileWidgetId,
                                   SettingsObjectID::CARD0,
                                   "User Profile",
                                   text_cache.back().c_str(),
                                   MenuAction::run_command(g_cmd_open_profile));

    text_cache.push_back(binds_name_or_missing(gubsy_lobby_binds_profile(ctx.engine,
                                                                         ctx.player_index)));
    MenuWidget binds = make_card(kBindsWidgetId,
                                 SettingsObjectID::CARD1,
                                 "Binds Profile",
                                 text_cache.back().c_str(),
                                 MenuAction::run_command(g_cmd_open_binds));

    text_cache.push_back(input_name_or_missing(gubsy_lobby_input_settings_profile(
        ctx.engine,
        ctx.player_index)));
    MenuWidget input = make_card(kInputWidgetId,
                                 SettingsObjectID::CARD2,
                                 "Input Settings",
                                 text_cache.back().c_str(),
                                 MenuAction::run_command(g_cmd_open_input));

    const GubsyLobbyPlayer* player = gubsy_lobby_player(ctx.engine, ctx.player_index);
    text_cache.push_back("Assigned devices: " +
                         std::to_string(player ? player->devices.size() : 0));
    MenuWidget devices = make_card(kDeviceWidgetId,
                                   SettingsObjectID::CARD3,
                                   "Input Devices",
                                   text_cache.back().c_str(),
                                   MenuAction::run_command(g_cmd_open_devices));
    MenuWidget remove = make_button(kRemoveWidgetId,
                                    SettingsObjectID::PREV,
                                    "Remove Player",
                                    MenuAction::run_command(g_cmd_remove_player));
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    profile.nav_down = binds.id;
    binds.nav_up = profile.id;
    binds.nav_down = input.id;
    input.nav_up = binds.id;
    input.nav_down = devices.id;
    devices.nav_up = input.id;
    devices.nav_down = remove.id;
    remove.nav_up = devices.id;
    remove.nav_down = back.id;
    back.nav_up = remove.id;

    widgets.push_back(profile);
    widgets.push_back(binds);
    widgets.push_back(input);
    widgets.push_back(devices);
    widgets.push_back(remove);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = profile.id;
    return built;
}

} // namespace

void register_lobby_player_settings_screen(EngineState& engine) {
    if (g_cmd_open_profile == kMenuIdInvalid)
        g_cmd_open_profile = engine.menu_commands.register_command(command_open_profile);
    if (g_cmd_open_binds == kMenuIdInvalid)
        g_cmd_open_binds = engine.menu_commands.register_command(command_open_binds);
    if (g_cmd_open_input == kMenuIdInvalid)
        g_cmd_open_input = engine.menu_commands.register_command(command_open_input);
    if (g_cmd_open_devices == kMenuIdInvalid)
        g_cmd_open_devices = engine.menu_commands.register_command(command_open_devices);
    if (g_cmd_remove_player == kMenuIdInvalid)
        g_cmd_remove_player = engine.menu_commands.register_command(command_remove_player);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_PLAYER_SETTINGS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_player_settings;
    engine.menu_manager.register_screen(def);
}
