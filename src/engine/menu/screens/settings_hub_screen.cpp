#include "engine/menu/screens/settings_hub_screen.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/menu/settings_category_registry.hpp"
#include "engine/menu/menu_ids.hpp"
#include "engine/settings_catalog.hpp"
#include "game/ui_layout_ids.hpp"
namespace {

constexpr int kCategoriesPerPage = 4;
constexpr WidgetId kTitleWidgetId = 400;
constexpr WidgetId kStatusWidgetId = 401;
constexpr WidgetId kSearchWidgetId = 402;
constexpr WidgetId kPageLabelWidgetId = 403;
constexpr WidgetId kPrevButtonId = 404;
constexpr WidgetId kNextButtonId = 405;
constexpr WidgetId kBackButtonId = 430;
constexpr WidgetId kFirstCardWidgetId = 500;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;

struct CategoryCard {
    std::string tag;
    MenuScreenId screen_id{kMenuIdInvalid};
    int item_count{0};
    std::size_t order_hint{std::numeric_limits<std::size_t>::max()};
};

constexpr std::array<const char*, 12> kPreferredCategoryOrder = {
    "Display",
    "Video",
    "Graphics",
    "Audio",
    "Controls",
    "Gameplay",
    "HUD",
    "Accessibility",
    "Profiles",
    "Players",
    "Saves",
    "Cheats"
};

struct CategoryOverride {
    const char* label{nullptr};
    const char* description{nullptr};
};

const std::unordered_map<std::string_view, CategoryOverride> kCategoryOverrides = {
    {"Display", {"Display", "Resolution / fullscreen / scaling"}},
    {"Video", {"Video", "Graphics quality & effects"}},
    {"Graphics", {"Graphics", "Rendering presets"}},
    {"Audio", {"Audio", "Volume / devices"}},
    {"Controls", {"Controls", "Sensitivity / bindings"}},
    {"Gameplay", {"Gameplay", "Difficulty / assists"}},
    {"HUD", {"HUD", "HUD layout & minimap"}},
    {"Accessibility", {"Accessibility", "Accessibility options"}},
    {"Profiles", {"Profiles", "Profile management"}},
    {"Players", {"Players", "Split-screen players & devices"}},
    {"Saves", {"Saves", "Save slot management"}},
    {"Cheats", {"Cheats", "Cheats & sandbox"}},
    {"Debug", {"Debug", "Debug & dev tools"}},
};

std::size_t category_priority(std::string_view tag) {
    for (std::size_t i = 0; i < kPreferredCategoryOrder.size(); ++i) {
        if (tag == kPreferredCategoryOrder[i])
            return i;
    }
    return kPreferredCategoryOrder.size();
}

std::string dynamic_subtitle(const SettingsCatalog& catalog, const std::string& tag) {
    auto it = catalog.categories.find(tag);
    if (it == catalog.categories.end() || it->second.empty())
        return {};
    std::string out;
    int added = 0;
    for (const auto& entry : it->second) {
        if (!entry.metadata)
            continue;
        if (!out.empty())
            out += ", ";
        out += entry.metadata->label;
        ++added;
        if (added >= 3)
            break;
    }
    if (!out.empty())
        out += " …";
    return out;
}

struct SettingsHubState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
    std::vector<CategoryCard> cards;
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
    auto& st = ctx.state<SettingsHubState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
}

BuiltScreen build_settings_hub(MenuContext& ctx) {
    SettingsCatalog catalog = build_settings_catalog(ctx.player_index);
    auto& st = ctx.state<SettingsHubState>();
    st.cards.clear();
    st.cards.reserve(catalog.categories.size());
    for (const auto& [tag, entries] : catalog.categories) {
        CategoryCard card;
        card.tag = tag;
        card.item_count = static_cast<int>(entries.size());
        card.screen_id = ensure_settings_category_screen(tag);
        card.order_hint = category_priority(card.tag);
        st.cards.push_back(std::move(card));
    }

    std::sort(st.cards.begin(), st.cards.end(), [](const CategoryCard& a, const CategoryCard& b) {
        if (a.order_hint != b.order_hint)
            return a.order_hint < b.order_hint;
        return a.tag < b.tag;
    });

    st.total_pages = std::max(1, (static_cast<int>(st.cards.size()) + kCategoriesPerPage - 1) / kCategoriesPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);

    st.status_text = std::to_string(st.cards.size()) + (st.cards.size() == 1 ? " category" : " categories");
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, SettingsObjectID::TITLE, "Settings"));

    MenuWidget status_label = make_label_widget(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str());
    status_label.label = st.status_text.c_str();
    widgets.push_back(status_label);

    widgets.push_back(make_label_widget(kSearchWidgetId, SettingsObjectID::SEARCH, "Search coming soon"));

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, SettingsObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, SettingsObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, SettingsObjectID::NEXT, ">", next_action);
    prev_btn.on_left = MenuAction::none();
    prev_btn.on_right = MenuAction::request_focus(next_btn.id);
    next_btn.on_left = MenuAction::request_focus(prev_btn.id);
    next_btn.on_right = MenuAction::none();
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kCategoriesPerPage);

    int start_index = st.page * kCategoriesPerPage;
    std::size_t cards_offset = widgets.size();
    for (int i = 0; i < kCategoriesPerPage; ++i) {
        int card_index = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (card_index < static_cast<int>(st.cards.size())) {
            const CategoryCard& card = st.cards[static_cast<std::size_t>(card_index)];
            MenuWidget card_widget;
            card_widget.id = widget_id;
            card_widget.slot = slot;
            card_widget.type = WidgetType::Card;
            auto override_it = kCategoryOverrides.find(card.tag);
            if (override_it != kCategoryOverrides.end() && override_it->second.label)
                card_widget.label = override_it->second.label;
            else
                card_widget.label = card.tag.c_str();

            std::string count_text = (card.item_count == 1) ? "1 setting"
                                                            : std::to_string(card.item_count) + " settings";
            std::string subtitle;
            if (override_it != kCategoryOverrides.end() && override_it->second.description) {
                subtitle = std::string(override_it->second.description) + " · " + count_text;
            } else {
                subtitle = dynamic_subtitle(catalog, card.tag);
                if (!subtitle.empty())
                    subtitle += " · " + count_text;
                else
                    subtitle = count_text;
            }
            text_cache.emplace_back(std::move(subtitle));
            card_widget.secondary = text_cache.back().c_str();
            card_widget.on_select = (card.screen_id != kMenuIdInvalid) ? MenuAction::push(card.screen_id)
                                                                       : MenuAction::none();
            card_widget.on_left = MenuAction::none();
            card_widget.on_right = MenuAction::none();
            widgets.push_back(card_widget);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, SettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.front();
    WidgetId last_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.back();

    prev_ref.nav_left = prev_ref.id;
    prev_ref.nav_right = next_ref.id;
    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;

    if (first_card_id != kMenuIdInvalid) {
        prev_ref.nav_down = first_card_id;
        next_ref.nav_down = first_card_id;
    } else {
        prev_ref.nav_down = back_ref.id;
        next_ref.nav_down = back_ref.id;
    }
    prev_ref.nav_up = prev_ref.id;
    next_ref.nav_up = next_ref.id;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card_widget = widgets[cards_offset + i];
        card_widget.nav_left = card_widget.id;
        card_widget.nav_right = card_widget.id;
        card_widget.nav_up = (i == 0) ? prev_ref.id : card_ids[i - 1];
        card_widget.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_left = back_ref.id;
    back_ref.nav_right = back_ref.id;
    back_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : prev_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = !card_ids.empty()
                              ? card_ids.front()
                              : (prev_action.type != MenuActionType::None
                                         ? prev_btn.id
                                         : (next_action.type != MenuActionType::None ? next_btn.id : back_btn.id));
    return built;
}

} // namespace

void register_settings_hub_screen() {
    if (!es)
        return;

    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);

    MenuScreenDef def;
    def.id = MenuScreenID::SETTINGS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<SettingsHubState>();
    def.build = build_settings_hub;
    es->menu_manager.register_screen(def);
}
