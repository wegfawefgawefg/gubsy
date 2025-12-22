#include "game/menu/screens/settings_hub_screen.hpp"

#include "engine/globals.hpp"
#include "engine/menu/menu_screen.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_commands.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

struct SettingsHubState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::string status_text;
};

struct CategoryDesc {
    const char* label;
    const char* subtitle;
    MenuAction action;
};

constexpr int kCardsPerPage = 4;

const std::array<CategoryDesc, 8> kCategories{ {
    {"Video", "Display / resolution", MenuAction::push(MenuScreenID::SETTINGS_CATEGORY_BASE + 0)},
    {"Audio", "Volume / devices", MenuAction::none()},
    {"Controls", "Sensitivity / invert / vibration", MenuAction::none()},
    {"Gameplay", "HUD / subtitles", MenuAction::none()},
    {"Profiles", "Manage users", MenuAction::none()},
    {"Players & Devices", "Assign controllers", MenuAction::none()},
    {"Binds", "Edit bindings", MenuAction::none()},
    {"Saves", "Manage save files", MenuAction::none()},
} };

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;

void command_page_delta(MenuContext& ctx, std::int32_t delta) {
    auto& st = ctx.state<SettingsHubState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + static_cast<int>(delta), 0, max_page);
}

MenuWidget make_card_widget(WidgetId id, UILayoutObjectId slot, const CategoryDesc& desc) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Card;
    w.label = desc.label;
    w.secondary = desc.subtitle;
    w.on_select = desc.action;
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

MenuWidget make_label_widget(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget w;
    w.id = id;
    w.slot = slot;
    w.type = WidgetType::Label;
    w.label = label;
    return w;
}

BuiltScreen build_settings_hub(MenuContext& ctx) {
    auto& st = ctx.state<SettingsHubState>();
    static std::vector<MenuWidget> widgets;
    static std::vector<MenuAction> frame_actions;
    widgets.clear();
    frame_actions.clear();
    widgets.reserve(kCardsPerPage + 10);

    const int total_categories = static_cast<int>(kCategories.size());
    st.total_pages = std::max(1, (total_categories + kCardsPerPage - 1) / kCardsPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);

    widgets.push_back(make_label_widget(400, SettingsObjectID::TITLE, "Settings"));
    widgets.push_back(make_label_widget(401, SettingsObjectID::STATUS, ""));
    widgets.push_back(make_label_widget(402, SettingsObjectID::SEARCH, "Search coming soon"));

    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    MenuWidget page_label = make_label_widget(403, SettingsObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages)
        next_action = MenuAction::run_command(g_cmd_page_delta, 1);

    MenuWidget prev_btn = make_button_widget(404, SettingsObjectID::PREV, "<", prev_action);
    prev_btn.on_left = prev_action;
    prev_btn.on_right = next_action;

    MenuWidget next_btn = make_button_widget(405, SettingsObjectID::NEXT, ">", next_action);
    next_btn.on_left = prev_action;
    next_btn.on_right = next_action;
    widgets.push_back(prev_btn);
    widgets.push_back(next_btn);

    std::vector<WidgetId> card_ids;
    for (int i = 0; i < kCardsPerPage; ++i) {
        int cat_index = st.page * kCardsPerPage + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(SettingsObjectID::CARD0 + i);
        WidgetId card_id = static_cast<WidgetId>(420 + i);
        if (cat_index < total_categories) {
            MenuWidget card = make_card_widget(card_id, slot, kCategories[static_cast<std::size_t>(cat_index)]);
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(card_id);
        } else {
            widgets.push_back(make_label_widget(card_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(430, SettingsObjectID::BACK, "Back", MenuAction::pop());
    back_btn.on_left = prev_action;
    back_btn.on_right = next_action;
    widgets.push_back(back_btn);

    auto find_widget = [&](WidgetId id) -> MenuWidget* {
        for (auto& w : widgets) {
            if (w.id == id)
                return &w;
        }
        return nullptr;
    };

    MenuWidget* prev_ptr = find_widget(404);
    MenuWidget* next_ptr = find_widget(405);
    MenuWidget* back_ptr = find_widget(430);

    MenuWidget* first_card = nullptr;
    MenuWidget* last_card = nullptr;
    for (WidgetId id : card_ids) {
        if (MenuWidget* card = find_widget(id)) {
            if (!first_card)
                first_card = card;
            last_card = card;
        }
    }

    if (prev_ptr) {
        prev_ptr->nav_right = next_ptr ? next_ptr->id : prev_ptr->id;
        prev_ptr->nav_down = first_card ? first_card->id : back_ptr ? back_ptr->id : prev_ptr->id;
        prev_ptr->nav_up = prev_ptr->id;
    }
    if (next_ptr) {
        next_ptr->nav_left = prev_ptr ? prev_ptr->id : next_ptr->id;
        next_ptr->nav_down = first_card ? first_card->id : back_ptr ? back_ptr->id : next_ptr->id;
        next_ptr->nav_up = next_ptr->id;
    }

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget* card = find_widget(card_ids[i]);
        if (!card)
            continue;
        if (i == 0) {
            card->nav_up = prev_ptr ? prev_ptr->id : card->id;
        } else {
            card->nav_up = card_ids[i - 1];
        }
        if (i + 1 < card_ids.size()) {
            card->nav_down = card_ids[i + 1];
        } else if (back_ptr) {
            card->nav_down = back_ptr->id;
        } else {
            card->nav_down = card->id;
        }
    }

    if (back_ptr) {
        back_ptr->nav_up = last_card ? last_card->id : (prev_ptr ? prev_ptr->id : back_ptr->id);
        back_ptr->nav_down = back_ptr->id;
    }

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.frame_actions = MenuActionList{frame_actions};
    built.default_focus = first_card ? first_card->id : (back_ptr ? back_ptr->id : prev_ptr ? prev_ptr->id : 400);
    return built;
}

} // namespace

void register_settings_hub_screen() {
    if (!es)
        return;

    g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);

    MenuScreenDef def;
    def.id = MenuScreenID::SETTINGS;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<SettingsHubState>();
    def.build = build_settings_hub;
    es->menu_manager.register_screen(def);
}
