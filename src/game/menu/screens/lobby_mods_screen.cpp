#include "game/menu/screens/lobby_mods_screen.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kModsPerPage = 4;
constexpr WidgetId kTitleWidgetId = 600;
constexpr WidgetId kStatusWidgetId = 601;
constexpr WidgetId kSearchWidgetId = 602;
constexpr WidgetId kPageLabelWidgetId = 603;
constexpr WidgetId kPrevButtonId = 604;
constexpr WidgetId kNextButtonId = 605;
constexpr WidgetId kBackButtonId = 630;
constexpr WidgetId kFirstCardWidgetId = 620;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_toggle_mod = kMenuIdInvalid;

struct LobbyModsState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::vector<int> filtered_indices;
    std::string search_query;
    std::string prev_search;
};

MenuWidget make_label_widget(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Label;
    w.label = label;
    return w;
}

MenuWidget make_button_widget(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Button;
    w.label = label;
    w.on_select = action;
    return w;
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<LobbyModsState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_toggle_mod(MenuContext& ctx, std::int32_t index) {
    (void)ctx;
    LobbySession& lobby = lobby_state();
    if (index < 0 || index >= static_cast<int>(lobby.mods.size()))
        return;
    auto& entry = lobby.mods[static_cast<std::size_t>(index)];
    if (entry.required)
        return;
    entry.enabled = !entry.enabled;
}

void rebuild_filter(LobbyModsState& st, const std::vector<LobbyModEntry>& mods) {
    st.filtered_indices.clear();
    std::string needle = st.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (std::size_t i = 0; i < mods.size(); ++i) {
        if (!needle.empty()) {
            std::string hay = mods[i].title + " " + mods[i].author + " " + mods[i].description + " " + mods[i].id;
            std::transform(hay.begin(), hay.end(), hay.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (hay.find(needle) == std::string::npos)
                continue;
        }
        st.filtered_indices.push_back(static_cast<int>(i));
    }

    int filtered = static_cast<int>(st.filtered_indices.size());
    if (filtered == 0) {
        st.page = 0;
        st.total_pages = 1;
        st.page_text = "Page 0 / 0";
        return;
    }
    st.total_pages = std::max(1, (filtered + kModsPerPage - 1) / kModsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

BuiltScreen build_lobby_mods(MenuContext& ctx) {
    LobbySession& lobby = lobby_state();
    lobby_refresh_mods();

    auto& st = ctx.state<LobbyModsState>();
    if (st.search_query != st.prev_search) {
        st.prev_search = st.search_query;
        rebuild_filter(st, lobby.mods);
    } else if (st.filtered_indices.empty() || st.filtered_indices.size() != lobby.mods.size()) {
        rebuild_filter(st, lobby.mods);
    }
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(8);

    widgets.push_back(make_label_widget(kTitleWidgetId, LobbyModsObjectID::TITLE, "Session Mods"));

    st.status_text = std::to_string(st.filtered_indices.size()) +
                     (st.filtered_indices.size() == 1 ? " mod" : " mods");
    MenuWidget status_label = make_label_widget(kStatusWidgetId, LobbyModsObjectID::STATUS, st.status_text.c_str());
    status_label.label = st.status_text.c_str();
    widgets.push_back(status_label);

    MenuWidget search;
    search.id = kSearchWidgetId;
    search.slot = LobbyModsObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &st.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search mods...";
    widgets.push_back(search);
    std::size_t search_idx = widgets.size() - 1;

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, LobbyModsObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, LobbyModsObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, LobbyModsObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kModsPerPage);
    std::size_t cards_offset = widgets.size();

    int start_index = st.page * kModsPerPage;
    for (int i = 0; i < kModsPerPage; ++i) {
        int filtered_idx = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(LobbyModsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (filtered_idx < static_cast<int>(st.filtered_indices.size())) {
            int mod_index = st.filtered_indices[static_cast<std::size_t>(filtered_idx)];
            const LobbyModEntry& entry = lobby.mods[static_cast<std::size_t>(mod_index)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            card.label = entry.title.c_str();
            std::string subtitle;
            if (!entry.author.empty() && !entry.description.empty())
                subtitle = entry.author + " - " + entry.description;
            else if (!entry.author.empty())
                subtitle = entry.author;
            else
                subtitle = entry.description;
            text_cache.emplace_back(std::move(subtitle));
            card.secondary = text_cache.back().c_str();
            if (entry.required) {
                card.badge = "Core";
                card.badge_color = SDL_Color{220, 200, 150, 255};
                card.on_select = MenuAction::none();
            } else {
                card.badge = entry.enabled ? "Enabled" : "Disabled";
                card.badge_color = entry.enabled ? SDL_Color{120, 200, 140, 255}
                                                : SDL_Color{200, 160, 120, 255};
                card.on_select = MenuAction::run_command(g_cmd_toggle_mod, mod_index);
            }
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, LobbyModsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& search_ref = widgets[search_idx];
    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.front();
    WidgetId last_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.back();
    WidgetId cards_start = first_card_id != kMenuIdInvalid ? first_card_id : back_ref.id;

    search_ref.nav_down = cards_start;
    search_ref.nav_left = search_ref.id;
    search_ref.nav_right = prev_ref.id;
    search_ref.nav_up = search_ref.id;

    prev_ref.nav_left = search_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_up = search_ref.id;
    prev_ref.nav_down = cards_start;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_up = search_ref.id;
    next_ref.nav_down = cards_start;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card = widgets[cards_offset + i];
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
        card.nav_up = (i == 0) ? search_ref.id : card_ids[i - 1];
        card.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : search_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::LOBBY_MODS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = !card_ids.empty() ? card_ids.front() : back_ref.id;
    return built;
}

} // namespace

void register_lobby_mods_screen() {
    if (!es)
        return;
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_toggle_mod == kMenuIdInvalid)
        g_cmd_toggle_mod = es->menu_commands.register_command(command_toggle_mod);

    MenuScreenDef def;
    def.id = MenuScreenID::LOBBY_MODS;
    def.layout = UILayoutID::LOBBY_MODS_SCREEN;
    def.state_ops = screen_state_ops<LobbyModsState>();
    def.build = build_lobby_mods;
    es->menu_manager.register_screen(def);
}
