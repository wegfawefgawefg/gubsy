#include "src/menu/screens/lobby_player_settings_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
#include "src/input_settings_profiles.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kRowsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 2500;
constexpr WidgetId kStatusWidgetId = 2501;
constexpr WidgetId kPageLabelWidgetId = 2502;
constexpr WidgetId kPrevButtonId = 2503;
constexpr WidgetId kNextButtonId = 2504;
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
MenuCommandId g_cmd_page_delta = kMenuIdInvalid;

struct PlayerSettingsState {
    int page{0};
    int total_pages{1};
    std::string page_text;
};

void update_page(PlayerSettingsState& st, int row_count) {
    st.total_pages = std::max(1, (row_count + kRowsPerPage - 1) / kRowsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

MenuWidget make_label(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Label;
    widget.label = label;
    return widget;
}

MenuWidget make_card(WidgetId id, UILayoutObjectId slot, const char* label, const char* secondary,
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

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<PlayerSettingsState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, 4);
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
    auto& st = ctx.state<PlayerSettingsState>();
    bool can_remove_player = ctx.engine.lobby.local_players.size() > 1;
    update_page(st, 4);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    text_cache.push_back(gubsy_lobby_player_label(ctx.engine, ctx.player_index));
    widgets.push_back(
        make_label(kTitleWidgetId, SettingsObjectID::TITLE, text_cache.back().c_str()));
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, "Player setup"));
    widgets.push_back(make_label(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuAction prev_action =
        st.page > 0 ? MenuAction::run_command(g_cmd_page_delta, -1) : MenuAction::none();
    MenuAction next_action = st.page + 1 < st.total_pages
                                 ? MenuAction::run_command(g_cmd_page_delta, 1)
                                 : MenuAction::none();
    MenuWidget prev = st.page > 0
                          ? make_button(kPrevButtonId, SettingsObjectID::PREV, "<", prev_action)
                          : make_label(kPrevButtonId, SettingsObjectID::PREV, "");
    prev.role = MenuWidgetRole::PagePrev;
    MenuWidget next = st.page + 1 < st.total_pages
                          ? make_button(kNextButtonId, SettingsObjectID::NEXT, ">", next_action)
                          : make_label(kNextButtonId, SettingsObjectID::NEXT, "");
    next.role = MenuWidgetRole::PageNext;
    widgets.push_back(prev);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next);
    std::size_t next_idx = widgets.size() - 1;

    text_cache.push_back(
        profile_name_or_missing(gubsy_lobby_user_profile(ctx.engine, ctx.player_index)));
    MenuWidget profile =
        make_card(kProfileWidgetId, SettingsObjectID::CARD0, "User Profile",
                  text_cache.back().c_str(), MenuAction::run_command(g_cmd_open_profile));

    text_cache.push_back(
        binds_name_or_missing(gubsy_lobby_binds_profile(ctx.engine, ctx.player_index)));
    MenuWidget binds =
        make_card(kBindsWidgetId, SettingsObjectID::CARD1, "Binds Profile",
                  text_cache.back().c_str(), MenuAction::run_command(g_cmd_open_binds));

    text_cache.push_back(
        input_name_or_missing(gubsy_lobby_input_settings_profile(ctx.engine, ctx.player_index)));
    MenuWidget input =
        make_card(kInputWidgetId, SettingsObjectID::CARD2, "Input Settings",
                  text_cache.back().c_str(), MenuAction::run_command(g_cmd_open_input));

    const GubsyLobbyPlayer* player = gubsy_lobby_player(ctx.engine, ctx.player_index);
    text_cache.push_back("Assigned devices: " +
                         std::to_string(player ? player->devices.size() : 0));
    MenuWidget devices =
        make_card(kDeviceWidgetId, SettingsObjectID::CARD3, "Input Devices",
                  text_cache.back().c_str(), MenuAction::run_command(g_cmd_open_devices));
    MenuWidget remove = make_button(kRemoveWidgetId, SettingsObjectID::SEARCH, "Remove Player",
                                    MenuAction::run_command(g_cmd_remove_player));
    remove.style.bg_r = 74;
    remove.style.bg_g = 26;
    remove.style.bg_b = 30;
    remove.style.focus_r = 255;
    remove.style.focus_g = 120;
    remove.style.focus_b = 120;
    if (can_remove_player) {
        widgets.push_back(remove);
    }
    std::size_t remove_idx = can_remove_player ? widgets.size() - 1 : 0;
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    std::vector<MenuWidget> rows;
    rows.push_back(profile);
    rows.push_back(binds);
    rows.push_back(input);
    rows.push_back(devices);

    std::vector<WidgetId> row_ids;
    int start = st.page * kRowsPerPage;
    for (int i = 0; i < kRowsPerPage; ++i) {
        int row_index = start + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (row_index < static_cast<int>(rows.size())) {
            MenuWidget row = rows[static_cast<std::size_t>(row_index)];
            row.slot = slot;
            row.on_left = prev_action;
            row.on_right = next_action;
            widgets.push_back(row);
            row_ids.push_back(row.id);
        } else {
            widgets.push_back(
                make_label(kProfileWidgetId + static_cast<WidgetId>(100 + i), slot, ""));
        }
    }
    widgets.push_back(back);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];
    WidgetId first_row = row_ids.empty() ? back_ref.id : row_ids.front();
    WidgetId last_row = row_ids.empty() ? back_ref.id : row_ids.back();
    prev_ref.nav_right = next_ref.type == WidgetType::Button ? next_ref.id : kMenuIdInvalid;
    prev_ref.nav_down = first_row;
    next_ref.nav_left = prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid;
    next_ref.nav_down = first_row;
    if (can_remove_player) {
        MenuWidget& remove_ref = widgets[remove_idx];
        remove_ref.nav_right =
            next_ref.type == WidgetType::Button
                ? next_ref.id
                : (prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid);
        remove_ref.nav_down = first_row;
    }
    for (std::size_t i = 0; i < row_ids.size(); ++i) {
        for (MenuWidget& widget : widgets) {
            if (widget.id != row_ids[i])
                continue;
            widget.nav_up =
                (i == 0)
                    ? (can_remove_player
                           ? kRemoveWidgetId
                           : (prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid))
                    : row_ids[i - 1];
            widget.nav_down = (i + 1 < row_ids.size()) ? row_ids[i + 1] : back_ref.id;
            break;
        }
    }
    back_ref.nav_up = last_row;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = can_remove_player ? kRemoveWidgetId : first_row;
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
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_PLAYER_SETTINGS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<PlayerSettingsState>();
    def.build = build_player_settings;
    engine.menu_manager.register_screen(def);
}
