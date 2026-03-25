#include "engine/menu/screens/profiles_screen.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/menu/menu_ids.hpp"
#include "engine/menu/settings_category_registry.hpp"
#include "engine/player.hpp"
#include "engine/user_profiles.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kProfilesPerPage = 4;
constexpr int kNewProfileMarker = -1;
constexpr WidgetId kTitleWidgetId = 1400;
constexpr WidgetId kStatusWidgetId = 1401;
constexpr WidgetId kSearchWidgetId = 1402;
constexpr WidgetId kPageLabelWidgetId = 1403;
constexpr WidgetId kPrevButtonId = 1404;
constexpr WidgetId kNextButtonId = 1405;
constexpr WidgetId kBackButtonId = 1430;
constexpr WidgetId kFirstCardWidgetId = 1420;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_select_profile = kMenuIdInvalid;
MenuCommandId g_cmd_create_profile = kMenuIdInvalid;

struct ProfilesState {
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

bool search_matches_new(const std::string& query) {
    std::string lower = query;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("new") != std::string::npos || lower.find("create") != std::string::npos;
}

void rebuild_filter(ProfilesState& st, const std::vector<UserProfile>& profiles) {
    st.filtered_indices.clear();
    std::string needle = st.search_query;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    bool include_new = st.search_query.empty() || search_matches_new(st.search_query);
    if (include_new)
        st.filtered_indices.push_back(kNewProfileMarker);

    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (!needle.empty()) {
            std::string hay = profiles[i].name;
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
    st.total_pages = std::max(1, (filtered + kProfilesPerPage - 1) / kProfilesPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
}

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<ProfilesState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.filtered_indices.empty()) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void open_profile_editor(MenuContext& ctx, int profile_id) {
    set_user_profile_for_player(ctx.player_index, profile_id);
    MenuScreenId screen_id = ensure_settings_category_screen("Profiles");
    ctx.manager.push_screen(screen_id, ctx.player_index);
}

void command_select_profile(MenuContext& ctx, std::int32_t index) {
    if (!es)
        return;
    auto& pool = es->user_profiles_pool;
    if (index < 0 || index >= static_cast<int>(pool.size()))
        return;
    open_profile_editor(ctx, pool[static_cast<std::size_t>(index)].id);
}

void command_create_profile(MenuContext& ctx, std::int32_t) {
    if (!es)
        return;
    auto& pool = es->user_profiles_pool;
    auto make_name = [&pool]() {
        int counter = 1;
        while (counter < 1000) {
            std::string candidate = "Profile " + std::to_string(counter);
            bool exists = false;
            for (const auto& profile : pool) {
                if (profile.name == candidate) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                return candidate;
            ++counter;
        }
        return std::string("Profile");
    };

    UserProfile profile;
    profile.id = generate_user_profile_id();
    profile.name = make_name();
    profile.last_binds_profile_id = -1;
    profile.last_input_settings_profile_id = -1;
    profile.last_game_settings_profile_id = -1;
    save_user_profile(profile);
    pool.push_back(profile);
    open_profile_editor(ctx, profile.id);
}

BuiltScreen build_profiles(MenuContext& ctx) {
    auto& st = ctx.state<ProfilesState>();
    if (!es) {
        BuiltScreen built;
        built.layout = UILayoutID::SETTINGS_SCREEN;
        return built;
    }

    auto& profiles = es->user_profiles_pool;
    if (st.search_query != st.prev_search) {
        st.prev_search = st.search_query;
        rebuild_filter(st, profiles);
    } else if (st.filtered_indices.empty() || st.filtered_indices.size() != profiles.size() + 1) {
        rebuild_filter(st, profiles);
    }

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, "Profiles"));

    st.status_text = std::to_string(profiles.size()) + (profiles.size() == 1 ? " profile" : " profiles");
    MenuWidget status_label = make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str());
    widgets.push_back(status_label);

    MenuWidget search;
    search.id = kSearchWidgetId;
    search.slot = SettingsObjectID::SEARCH;
    search.type = WidgetType::TextInput;
    search.text_buffer = &st.search_query;
    search.text_max_len = 48;
    search.placeholder = "Search profiles...";
    widgets.push_back(search);
    std::size_t search_idx = widgets.size() - 1;

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str());
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, SettingsObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, SettingsObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kProfilesPerPage);
    std::size_t cards_offset = widgets.size();
    int start_index = st.page * kProfilesPerPage;
    for (int i = 0; i < kProfilesPerPage; ++i) {
        int filtered_idx = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (filtered_idx < static_cast<int>(st.filtered_indices.size())) {
            int profile_index = st.filtered_indices[static_cast<std::size_t>(filtered_idx)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Card;
            if (profile_index == kNewProfileMarker) {
                card.label = "New Profile";
                card.secondary = "Create a new profile.";
                card.on_select = MenuAction::run_command(g_cmd_create_profile);
            } else if (profile_index >= 0 && profile_index < static_cast<int>(profiles.size())) {
                const UserProfile& profile = profiles[static_cast<std::size_t>(profile_index)];
                card.label = profile.name.c_str();
                card.on_select = MenuAction::run_command(g_cmd_select_profile, profile_index);
            } else {
                card.label = "";
            }
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, SettingsObjectID::BACK, "Back", MenuAction::pop());
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
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = !card_ids.empty() ? card_ids.front() : back_ref.id;
    return built;
}

} // namespace

void register_profiles_screen() {
    if (!es)
        return;
    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_select_profile == kMenuIdInvalid)
        g_cmd_select_profile = es->menu_commands.register_command(command_select_profile);
    if (g_cmd_create_profile == kMenuIdInvalid)
        g_cmd_create_profile = es->menu_commands.register_command(command_create_profile);

    MenuScreenDef def;
    def.id = MenuScreenID::PROFILES;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<ProfilesState>();
    def.build = build_profiles;
    es->menu_manager.register_screen(def);
}
