#include "src/menu/screens/lobby_local_players_screen.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/engine_state.hpp"
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
constexpr WidgetId kTitleWidgetId = 2400;
constexpr WidgetId kStatusWidgetId = 2401;
constexpr WidgetId kPageLabelWidgetId = 2403;
constexpr WidgetId kPrevButtonId = 2404;
constexpr WidgetId kNextButtonId = 2405;
constexpr WidgetId kAddButtonId = 2410;
constexpr WidgetId kBackButtonId = 2430;
constexpr WidgetId kFirstCardWidgetId = 2420;
constexpr WidgetId kRemoteTitleWidgetId = 2500;
constexpr WidgetId kRemoteStatusWidgetId = 2501;
constexpr WidgetId kRemoteInfoWidgetId = 2510;
constexpr WidgetId kRemoteKickWidgetId = 2511;
constexpr WidgetId kRemoteBackWidgetId = 2530;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_add_player = kMenuIdInvalid;
MenuCommandId g_cmd_open_player = kMenuIdInvalid;
MenuCommandId g_cmd_open_remote_member = kMenuIdInvalid;
MenuCommandId g_cmd_kick_member = kMenuIdInvalid;

struct LocalPlayersState {
    int page{0};
    int total_pages{1};
    std::string page_text;
    std::string status_text;
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

void update_page(LocalPlayersState& st, int count) {
    st.total_pages = std::max(1, (count + kRowsPerPage - 1) / kRowsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

std::vector<const MatchmakingMember*> remote_members(const GubsyLobbyState& lobby) {
    std::vector<const MatchmakingMember*> members;
    members.reserve(lobby.members.size());
    for (const MatchmakingMember& member : lobby.members) {
        if (!lobby.member_id.empty() && member.member_id == lobby.member_id)
            continue;
        members.push_back(&member);
    }
    return members;
}

std::string remote_member_label(const MatchmakingMember& member) {
    if (!member.display_name.empty())
        return member.display_name;
    if (!member.member_id.empty())
        return member.member_id;
    return "Remote Player";
}

bool can_manage_remote_member(const EngineState& engine, const MatchmakingMember& member) {
    if (!engine.lobby.online || !engine.lobby.is_host || member.is_host)
        return false;
    if (!engine.lobby.room_code.empty())
        return !engine.lobby.host_secret.empty();
    return engine.lobby_commands.kick_direct_member != nullptr;
}

std::string remote_member_detail(const EngineState& engine, const MatchmakingMember& member) {
    const GubsyLobbyState& lobby = engine.lobby;
    std::string detail = member.is_host ? "Host client" : "Remote client";
    if (!lobby.room_code.empty()) {
        detail += " | gubsy-roomd";
        detail += " | Room ";
        detail += lobby.room_code;
    } else if (lobby.online) {
        detail += " | Direct";
    }
    if (!member.client_label.empty()) {
        detail += " | Client ";
        detail += member.client_label;
    }
    if (!lobby.advertised_endpoint.empty()) {
        detail += " | Endpoint ";
        detail += lobby.advertised_endpoint;
    }
    if (!member.member_id.empty()) {
        detail += " | ";
        detail += member.member_id;
    }
    if (can_manage_remote_member(engine, member))
        detail += " | Select for actions.";
    else
        detail += " | Host-managed.";
    return detail;
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<LocalPlayersState>();
    const std::vector<const MatchmakingMember*> remotes = remote_members(ctx.engine.lobby);
    const int count = static_cast<int>(ctx.engine.lobby.local_players.size() + remotes.size());
    st.page = std::clamp(st.page + delta, 0, std::max(0, st.total_pages - 1));
    update_page(st, count);
}

void command_add_player(MenuContext& ctx, std::int32_t) {
    int index = gubsy_lobby_add_local_player(ctx.engine);
    ctx.manager.push_screen(MenuScreenID::LOBBY_PLAYER_SETTINGS, index);
}

void command_open_player(MenuContext& ctx, std::int32_t index) {
    gubsy_lobby_select_player(ctx.engine, index);
    ctx.manager.push_screen(MenuScreenID::LOBBY_PLAYER_SETTINGS, index);
}

void command_open_remote_member(MenuContext& ctx, std::int32_t index) {
    ctx.manager.push_screen(MenuScreenID::LOBBY_REMOTE_PLAYER, index);
}

void command_kick_member(MenuContext& ctx, std::int32_t index) {
    const std::vector<const MatchmakingMember*> remotes = remote_members(ctx.engine.lobby);
    if (index < 0 || index >= static_cast<int>(remotes.size()))
        return;
    std::string message;
    const MatchmakingMember* member = remotes[static_cast<std::size_t>(index)];
    const bool kicked = ctx.engine.lobby.room_code.empty()
        ? gubsy_lobby_kick_direct_member(ctx.engine, *member, message)
        : gubsy_lobby_remove_room_member(ctx.engine, member->member_id, message);
    if (kicked)
        ctx.manager.pop_screen();
}

BuiltScreen build_local_players(MenuContext& ctx) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<LocalPlayersState>();
    const int local_count = static_cast<int>(ctx.engine.lobby.local_players.size());
    const std::vector<const MatchmakingMember*> remotes = remote_members(ctx.engine.lobby);
    const int remote_count = static_cast<int>(remotes.size());
    int count = local_count + remote_count;
    update_page(st, count);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(static_cast<std::size_t>(kRowsPerPage) * 2 + 4);

    widgets.push_back(make_label(kTitleWidgetId, SettingsObjectID::TITLE, "Players"));
    st.status_text = std::to_string(local_count) +
                     (local_count == 1 ? " local player" : " local players");
    if (remote_count > 0) {
        st.status_text += ", ";
        st.status_text += std::to_string(remote_count);
        st.status_text += remote_count == 1 ? " remote client" : " remote clients";
    }
    widgets.push_back(
        make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));
    widgets.push_back(make_label(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str()));

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages)
        next_action = MenuAction::run_command(g_cmd_page_delta, 1);

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
    int start = st.page * kRowsPerPage;
    MenuWidget add = make_button(kAddButtonId, SettingsObjectID::SEARCH, "Add Local Player",
                                 MenuAction::run_command(g_cmd_add_player));
    add.style.bg_r = 22;
    add.style.bg_g = 58;
    add.style.bg_b = 34;
    add.style.focus_r = 110;
    add.style.focus_g = 230;
    add.style.focus_b = 140;

    widgets.push_back(add);
    std::size_t add_idx = widgets.size() - 1;

    for (int i = 0; i < kRowsPerPage; ++i) {
        int row_index = start + i;
        WidgetId widget_id = kFirstCardWidgetId + static_cast<WidgetId>(i);
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        if (row_index < local_count) {
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            int player_index = row_index;
            text_cache.push_back(gubsy_lobby_player_label(ctx.engine, player_index));
            const GubsyLobbyPlayer* player = gubsy_lobby_player(ctx.engine, player_index);
            std::string detail = "Devices: ";
            detail += player ? std::to_string(player->devices.size()) : "0";
            detail += "  Select to edit.";
            text_cache.push_back(std::move(detail));
            card.label = text_cache[text_cache.size() - 2].c_str();
            card.secondary = text_cache[text_cache.size() - 1].c_str();
            card.on_select = MenuAction::run_command(g_cmd_open_player, player_index);
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else if (row_index < count) {
            const int remote_index = row_index - local_count;
            const MatchmakingMember* member = remotes[static_cast<std::size_t>(remote_index)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            text_cache.push_back(remote_member_label(*member));
            text_cache.push_back(remote_member_detail(ctx.engine, *member));
            card.label = text_cache[text_cache.size() - 2].c_str();
            card.secondary = text_cache[text_cache.size() - 1].c_str();
            if (can_manage_remote_member(ctx.engine, *member)) {
                card.on_select = MenuAction::run_command(g_cmd_open_remote_member, remote_index);
            }
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label(widget_id, slot, ""));
        }
    }

    MenuWidget back = make_button(kBackButtonId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& add_ref = widgets[add_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card = card_ids.empty() ? back_ref.id : card_ids.front();
    WidgetId last_card = card_ids.empty() ? back_ref.id : card_ids.back();
    prev_ref.nav_right = next_ref.type == WidgetType::Button ? next_ref.id : kMenuIdInvalid;
    prev_ref.nav_down = first_card;
    next_ref.nav_left = prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid;
    next_ref.nav_down = first_card;
    add_ref.nav_right = next_ref.type == WidgetType::Button
                            ? next_ref.id
                            : (prev_ref.type == WidgetType::Button ? prev_ref.id : kMenuIdInvalid);
    add_ref.nav_down = first_card;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget* card = nullptr;
        for (auto& widget : widgets) {
            if (widget.id == card_ids[i]) {
                card = &widget;
                break;
            }
        }
        if (!card)
            continue;
        card->nav_up = (i == 0) ? add_ref.id : card_ids[i - 1];
        card->nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }
    back_ref.nav_up = last_card;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = add_ref.id;
    return built;
}

BuiltScreen build_remote_player(MenuContext& ctx) {
    const std::vector<const MatchmakingMember*> remotes = remote_members(ctx.engine.lobby);
    const int remote_index = ctx.player_index;
    const MatchmakingMember* member =
        (remote_index >= 0 && remote_index < static_cast<int>(remotes.size()))
            ? remotes[static_cast<std::size_t>(remote_index)]
            : nullptr;
    const bool can_kick = member && can_manage_remote_member(ctx.engine, *member);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(6);

    text_cache.push_back(member ? remote_member_label(*member) : std::string("Remote Player"));
    widgets.push_back(make_label(kRemoteTitleWidgetId, SettingsObjectID::TITLE,
                                 text_cache.back().c_str()));

    text_cache.push_back(member ? remote_member_detail(ctx.engine, *member)
                                : std::string("This remote player is no longer connected."));
    widgets.push_back(make_label(kRemoteStatusWidgetId, SettingsObjectID::STATUS,
                                 text_cache.back().c_str()));

    MenuWidget info;
    info.id = kRemoteInfoWidgetId;
    info.slot = SettingsObjectID::CARD0;
    info.type = WidgetType::Card;
    info.label = "Remote Client";
    info.secondary = text_cache[1].c_str();
    widgets.push_back(info);

    MenuWidget action;
    if (can_kick) {
        action = make_button(kRemoteKickWidgetId, SettingsObjectID::CARD1, "Kick Player",
                             MenuAction::run_command(g_cmd_kick_member, remote_index));
        action.secondary = ctx.engine.lobby.room_code.empty()
            ? "Disconnect this client from the direct session."
            : "Remove this client from the public room.";
        action.style.bg_r = 82;
        action.style.bg_g = 26;
        action.style.bg_b = 28;
        action.style.focus_r = 235;
        action.style.focus_g = 92;
        action.style.focus_b = 92;
    } else {
        action = make_label(kRemoteKickWidgetId, SettingsObjectID::CARD1, "Host Managed");
        action.secondary =
            member ? "Only the host can manage this client."
                   : "This remote player is no longer available.";
        action.style.fg_r = 150;
        action.style.fg_g = 150;
        action.style.fg_b = 165;
    }
    widgets.push_back(action);

    MenuWidget back =
        make_button(kRemoteBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);

    widgets[2].nav_down = can_kick ? kRemoteKickWidgetId : kRemoteBackWidgetId;
    widgets[3].nav_up = kRemoteInfoWidgetId;
    widgets[3].nav_down = kRemoteBackWidgetId;
    widgets[4].nav_up = can_kick ? kRemoteKickWidgetId : kRemoteInfoWidgetId;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = can_kick ? kRemoteKickWidgetId : kRemoteBackWidgetId;
    return built;
}

} // namespace

void register_lobby_local_players_screen(EngineState& engine) {
    g_cmd_page_delta = engine.menu_commands.register_command(command_page_delta);
    g_cmd_add_player = engine.menu_commands.register_command(command_add_player);
    g_cmd_open_player = engine.menu_commands.register_command(command_open_player);
    g_cmd_open_remote_member = engine.menu_commands.register_command(command_open_remote_member);
    g_cmd_kick_member = engine.menu_commands.register_command(command_kick_member);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_LOCAL_PLAYERS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<LocalPlayersState>();
    def.build = build_local_players;
    engine.menu_manager.register_screen(def);

    MenuScreenDef remote_def;
    remote_def.id = MenuScreenID::LOBBY_REMOTE_PLAYER;
    remote_def.layout = UILayoutID::SETTINGS_SCREEN;
    remote_def.build = build_remote_player;
    engine.menu_manager.register_screen(remote_def);
}
