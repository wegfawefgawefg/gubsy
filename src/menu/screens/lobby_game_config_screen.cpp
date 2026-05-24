#include "src/menu/screens/lobby_game_config_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kRowsPerPage = 3;
constexpr WidgetId kTitleWidgetId = 2800;
constexpr WidgetId kStatusWidgetId = 2801;
constexpr WidgetId kPageWidgetId = 2802;
constexpr WidgetId kPrevWidgetId = 2803;
constexpr WidgetId kNextWidgetId = 2804;
constexpr WidgetId kFirstRowWidgetId = 2820;
constexpr WidgetId kBackWidgetId = 2830;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_option_delta = kMenuIdInvalid;

struct GameConfigState {
    int page{0};
    int total_pages{1};
    std::string page_text;
    std::string status_text;
    std::vector<GubsyLobbyConfigRow> rows;
};

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

void update_page(GameConfigState& st) {
    int count = static_cast<int>(st.rows.size());
    st.total_pages = std::max(1, (count + kRowsPerPage - 1) / kRowsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

void rebuild_rows(MenuContext& ctx, GameConfigState& st) {
    st.rows.clear();
    GubsyLobbyConfigProvider& provider = ctx.engine.lobby_config_provider;
    if (provider.build_rows)
        provider.build_rows(provider.user_data, ctx.engine.lobby, st.rows);
    update_page(st);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<GameConfigState>();
    rebuild_rows(ctx, st);
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st);
}

void command_option_delta(MenuContext& ctx, std::int32_t payload) {
    auto& st = ctx.state<GameConfigState>();
    int row_index = payload / 10;
    int delta = payload % 10;
    if (delta == 9)
        delta = -1;
    rebuild_rows(ctx, st);
    if (row_index < 0 || row_index >= static_cast<int>(st.rows.size()))
        return;
    GubsyLobbyConfigRow& row = st.rows[static_cast<std::size_t>(row_index)];
    if (row.options.empty() || !ctx.engine.lobby_config_provider.set_option)
        return;
    if (row.host_only && ctx.engine.lobby.online && !ctx.engine.lobby.is_host) {
        add_alert(ctx.engine, "Only the host can change that setting");
        return;
    }
    int next = row.selected_option + delta;
    int count = static_cast<int>(row.options.size());
    next = (next + count) % count;
    if (!ctx.engine.lobby_config_provider.set_option(ctx.engine.lobby_config_provider.user_data,
                                                     ctx.engine.lobby, row.key.c_str(),
                                                     row.player_index, next)) {
        add_alert(ctx.engine, "Could not change lobby setting");
    }
}

BuiltScreen build_game_config(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<GameConfigState>();
    rebuild_rows(ctx, st);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Game Settings"));
    st.status_text = st.rows.empty() ? "No game lobby config provider registered"
                                     : "Game settings are provided by the game";
    if (!st.rows.empty() && ctx.engine.lobby.online && !ctx.engine.lobby.is_host)
        st.status_text = "Joined room. Host-owned settings are read-only.";
    widgets.push_back(
        make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
    widgets.push_back(make_label(kPageWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuAction prev_action =
        st.page > 0 ? MenuAction::run_command(g_cmd_page_delta, -1) : MenuAction::none();
    MenuAction next_action = st.page + 1 < st.total_pages
                                 ? MenuAction::run_command(g_cmd_page_delta, 1)
                                 : MenuAction::none();

    MenuWidget prev = st.page > 0 ? make_button(kPrevWidgetId, SettingsObjectID::PREV, "<", prev_action)
                                  : make_label(kPrevWidgetId, SettingsObjectID::PREV, "");
    prev.role = MenuWidgetRole::PagePrev;
    MenuWidget next = st.page + 1 < st.total_pages
                          ? make_button(kNextWidgetId, SettingsObjectID::NEXT, ">", next_action)
                          : make_label(kNextWidgetId, SettingsObjectID::NEXT, "");
    next.role = MenuWidgetRole::PageNext;
    widgets.push_back(prev);
    widgets.push_back(next);

    std::vector<WidgetId> row_ids;
    int start = st.page * kRowsPerPage;
    for (int i = 0; i < kRowsPerPage; ++i) {
        int row_index = start + i;
        WidgetId widget_id = kFirstRowWidgetId + static_cast<WidgetId>(i);
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (row_index < static_cast<int>(st.rows.size())) {
            const GubsyLobbyConfigRow& row = st.rows[static_cast<std::size_t>(row_index)];
            MenuWidget widget;
            widget.id = widget_id;
            widget.slot = slot;
            widget.type = WidgetType::OptionCycle;
            widget.label = row.label.c_str();
            widget.secondary = row.description.c_str();
            if (!row.options.empty()) {
                int selected =
                    std::clamp(row.selected_option, 0, static_cast<int>(row.options.size()) - 1);
                const GubsyLobbyOption& option = row.options[static_cast<std::size_t>(selected)];
                widget.badge = option.label.c_str();
            }
            const bool read_only =
                row.host_only && ctx.engine.lobby.online && !ctx.engine.lobby.is_host;
            if (!read_only) {
                widget.on_left = MenuAction::run_command(g_cmd_option_delta, row_index * 10 + 9);
                widget.on_right = MenuAction::run_command(g_cmd_option_delta, row_index * 10 + 1);
            }
            widgets.push_back(widget);
            row_ids.push_back(widget.id);
        } else {
            widgets.push_back(make_label(widget_id, slot, ""));
        }
    }

    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);

    for (std::size_t i = 0; i < row_ids.size(); ++i) {
        for (MenuWidget& widget : widgets) {
            if (widget.id != row_ids[i])
                continue;
            widget.nav_up = (i == 0)
                                ? (prev.type == WidgetType::Button ? prev.id : kMenuIdInvalid)
                                : row_ids[i - 1];
            widget.nav_down = (i + 1 < row_ids.size()) ? row_ids[i + 1] : back.id;
        }
    }
    back.nav_up = row_ids.empty()
                      ? (prev.type == WidgetType::Button ? prev.id : kMenuIdInvalid)
                      : row_ids.back();

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = row_ids.empty() ? back.id : row_ids.front();
    return built;
}

} // namespace

void register_lobby_game_config_screen(EngineState& engine) {
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    if (g_cmd_option_delta == kMenuIdInvalid)
        g_cmd_option_delta = engine.menu_commands.register_command(command_option_delta);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_GAME_CONFIG;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<GameConfigState>();
    def.build = build_game_config;
    engine.menu_manager.register_screen(def);
}
