#include "src/menu/screens/lobby_picker_screens.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/binds_profiles.hpp"
#include "src/engine_state.hpp"
#include "src/input_settings_profiles.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_manager.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"
#include "src/user_profiles.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kItemsPerPage = 3;
constexpr WidgetId kTitleWidgetId = 2600;
constexpr WidgetId kStatusWidgetId = 2601;
constexpr WidgetId kPageLabelWidgetId = 2603;
constexpr WidgetId kPrevButtonId = 2604;
constexpr WidgetId kNextButtonId = 2605;
constexpr WidgetId kBackButtonId = 2630;
constexpr WidgetId kFirstCardWidgetId = 2620;

MenuCommandId g_cmd_profile_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_binds_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_input_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_device_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_pick_profile = kMenuIdInvalid;
MenuCommandId g_cmd_pick_binds = kMenuIdInvalid;
MenuCommandId g_cmd_pick_input = kMenuIdInvalid;
MenuCommandId g_cmd_toggle_device = kMenuIdInvalid;

struct PickerState {
    int page{0};
    int total_pages{1};
    std::string page_text;
    std::string status_text;
};

enum class PickerKind {
    UserProfile,
    Binds,
    InputSettings,
    Device,
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

void update_page(PickerState& st, int count) {
    st.total_pages = std::max(1, (count + kItemsPerPage - 1) / kItemsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

void command_profile_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<PickerState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, static_cast<int>(ctx.engine.user_profiles_pool.size()));
}

void command_binds_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<PickerState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, static_cast<int>(ctx.engine.binds_profiles.size()));
}

void command_input_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<PickerState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, static_cast<int>(ctx.engine.input_settings_profiles.size()));
}

void command_device_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<PickerState>();
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, static_cast<int>(ctx.engine.input_sources.size()));
}

void command_pick_profile(MenuContext& ctx, std::int32_t index) {
    if (index < 0 || index >= static_cast<int>(ctx.engine.user_profiles_pool.size()))
        return;
    int id = ctx.engine.user_profiles_pool[static_cast<std::size_t>(index)].id;
    if (gubsy_lobby_set_user_profile(ctx.engine, ctx.player_index, id))
        ctx.manager.pop_screen();
}

void command_pick_binds(MenuContext& ctx, std::int32_t index) {
    if (index < 0 || index >= static_cast<int>(ctx.engine.binds_profiles.size()))
        return;
    int id = ctx.engine.binds_profiles[static_cast<std::size_t>(index)].id;
    if (gubsy_lobby_set_binds_profile(ctx.engine, ctx.player_index, id))
        ctx.manager.pop_screen();
}

void command_pick_input(MenuContext& ctx, std::int32_t index) {
    if (index < 0 || index >= static_cast<int>(ctx.engine.input_settings_profiles.size()))
        return;
    int id = ctx.engine.input_settings_profiles[static_cast<std::size_t>(index)].id;
    if (gubsy_lobby_set_input_settings_profile(ctx.engine, ctx.player_index, id))
        ctx.manager.pop_screen();
}

void command_toggle_device(MenuContext& ctx, std::int32_t index) {
    if (index < 0 || index >= static_cast<int>(ctx.engine.input_sources.size()))
        return;
    GubsyLobbyDeviceAssignment device = gubsy_lobby_device_from_input_source(
        ctx.engine.input_sources[static_cast<std::size_t>(index)]);
    gubsy_lobby_toggle_device(ctx.engine, ctx.player_index, device);
}

int item_count(const EngineState& engine, PickerKind kind) {
    if (kind == PickerKind::UserProfile)
        return static_cast<int>(engine.user_profiles_pool.size());
    if (kind == PickerKind::Binds)
        return static_cast<int>(engine.binds_profiles.size());
    if (kind == PickerKind::InputSettings)
        return static_cast<int>(engine.input_settings_profiles.size());
    return static_cast<int>(engine.input_sources.size());
}

const char* title_for_kind(PickerKind kind) {
    if (kind == PickerKind::UserProfile)
        return "Choose User Profile";
    if (kind == PickerKind::Binds)
        return "Choose Binds Profile";
    if (kind == PickerKind::InputSettings)
        return "Choose Input Settings";
    return "Assign Input Devices";
}

MenuCommandId page_command_for_kind(PickerKind kind) {
    if (kind == PickerKind::UserProfile)
        return g_cmd_profile_page_delta;
    if (kind == PickerKind::Binds)
        return g_cmd_binds_page_delta;
    if (kind == PickerKind::InputSettings)
        return g_cmd_input_page_delta;
    return g_cmd_device_page_delta;
}

MenuAction select_action_for_kind(PickerKind kind, int item_index) {
    if (kind == PickerKind::UserProfile)
        return MenuAction::run_command(g_cmd_pick_profile, item_index);
    if (kind == PickerKind::Binds)
        return MenuAction::run_command(g_cmd_pick_binds, item_index);
    if (kind == PickerKind::InputSettings)
        return MenuAction::run_command(g_cmd_pick_input, item_index);
    return MenuAction::run_command(g_cmd_toggle_device, item_index);
}

std::string item_label(const EngineState& engine, PickerKind kind, int item_index) {
    if (kind == PickerKind::UserProfile)
        return engine.user_profiles_pool[static_cast<std::size_t>(item_index)].name;
    if (kind == PickerKind::Binds)
        return engine.binds_profiles[static_cast<std::size_t>(item_index)].name;
    if (kind == PickerKind::InputSettings)
        return engine.input_settings_profiles[static_cast<std::size_t>(item_index)].name;
    return gubsy_lobby_device_label(gubsy_lobby_device_from_input_source(
        engine.input_sources[static_cast<std::size_t>(item_index)]));
}

std::string item_detail(const EngineState& engine, PickerKind kind, int player_index,
                        int item_index) {
    const GubsyLobbyPlayer* player = gubsy_lobby_player(engine, player_index);
    if (!player)
        return {};
    if (kind == PickerKind::UserProfile) {
        int id = engine.user_profiles_pool[static_cast<std::size_t>(item_index)].id;
        return id == player->user_profile_id ? "Current user profile." : "Select for this player.";
    }
    if (kind == PickerKind::Binds) {
        int id = engine.binds_profiles[static_cast<std::size_t>(item_index)].id;
        return id == player->binds_profile_id ? "Current binds profile."
                                              : "Select for this player.";
    }
    if (kind == PickerKind::InputSettings) {
        int id = engine.input_settings_profiles[static_cast<std::size_t>(item_index)].id;
        return id == player->input_settings_profile_id ? "Current input settings."
                                                       : "Select for this player.";
    }

    GubsyLobbyDeviceAssignment device = gubsy_lobby_device_from_input_source(
        engine.input_sources[static_cast<std::size_t>(item_index)]);
    return gubsy_lobby_player_has_device(engine, player_index, device)
               ? "Assigned. Select to remove."
               : "Not assigned. Select to add.";
}

BuiltScreen build_picker(MenuContext& ctx, PickerKind kind) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<PickerState>();
    int count = item_count(ctx.engine, kind);
    update_page(st, count);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, title_for_kind(kind)));
    st.status_text = gubsy_lobby_player_label(ctx.engine, ctx.player_index);
    widgets.push_back(
        make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
    widgets.push_back(make_label(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuCommandId page_cmd = page_command_for_kind(kind);
    MenuAction prev_action =
        st.page > 0 ? MenuAction::run_command(page_cmd, -1) : MenuAction::none();
    MenuAction next_action =
        st.page + 1 < st.total_pages ? MenuAction::run_command(page_cmd, 1) : MenuAction::none();

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

    std::vector<WidgetId> card_ids;
    int start = st.page * kItemsPerPage;
    for (int i = 0; i < kItemsPerPage; ++i) {
        int item_index = start + i;
        WidgetId widget_id = kFirstCardWidgetId + static_cast<WidgetId>(i);
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (item_index < count) {
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            text_cache.push_back(item_label(ctx.engine, kind, item_index));
            text_cache.push_back(item_detail(ctx.engine, kind, ctx.player_index, item_index));
            card.label = text_cache[text_cache.size() - 2].c_str();
            card.secondary = text_cache[text_cache.size() - 1].c_str();
            card.on_select = select_action_for_kind(kind, item_index);
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label(widget_id, slot, ""));
        }
    }

    MenuWidget action =
        make_button(kBackButtonId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(action);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];
    WidgetId first_card = card_ids.empty() ? back_ref.id : card_ids.front();
    WidgetId last_card = card_ids.empty() ? back_ref.id : card_ids.back();
    prev_ref.nav_right = next_ref.type == WidgetType::Button ? next_ref.id : kMenuIdInvalid;
    prev_ref.nav_down = first_card;
    next_ref.nav_left = prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid;
    next_ref.nav_down = first_card;
    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        for (MenuWidget& widget : widgets) {
            if (widget.id != card_ids[i])
                continue;
            widget.nav_up =
                (i == 0) ? (prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid)
                         : card_ids[i - 1];
            widget.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
            break;
        }
    }
    back_ref.nav_up = last_card;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = first_card;
    return built;
}

BuiltScreen build_profile_picker(MenuContext& ctx) {
    return build_picker(ctx, PickerKind::UserProfile);
}

BuiltScreen build_binds_picker(MenuContext& ctx) {
    return build_picker(ctx, PickerKind::Binds);
}

BuiltScreen build_input_picker(MenuContext& ctx) {
    return build_picker(ctx, PickerKind::InputSettings);
}

BuiltScreen build_device_picker(MenuContext& ctx) {
    return build_picker(ctx, PickerKind::Device);
}

void register_picker(EngineState& engine, MenuScreenId id, MenuBuildFn build) {
    MenuScreenDef def;
    def.id = id;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<PickerState>();
    def.build = build;
    engine.menu_manager.register_screen(def);
}

} // namespace

void register_lobby_picker_screens(EngineState& engine) {
    g_cmd_profile_page_delta = engine.menu_commands.register_command(command_profile_page_delta);
    g_cmd_binds_page_delta = engine.menu_commands.register_command(command_binds_page_delta);
    g_cmd_input_page_delta = engine.menu_commands.register_command(command_input_page_delta);
    g_cmd_device_page_delta = engine.menu_commands.register_command(command_device_page_delta);
    g_cmd_pick_profile = engine.menu_commands.register_command(command_pick_profile);
    g_cmd_pick_binds = engine.menu_commands.register_command(command_pick_binds);
    g_cmd_pick_input = engine.menu_commands.register_command(command_pick_input);
    g_cmd_toggle_device = engine.menu_commands.register_command(command_toggle_device);

    register_picker(engine, MenuScreenID::LOBBY_PROFILE_PICKER, build_profile_picker);
    register_picker(engine, MenuScreenID::LOBBY_BINDS_PICKER, build_binds_picker);
    register_picker(engine, MenuScreenID::LOBBY_INPUT_SETTINGS_PICKER, build_input_picker);
    register_picker(engine, MenuScreenID::LOBBY_DEVICE_PICKER, build_device_picker);
}
