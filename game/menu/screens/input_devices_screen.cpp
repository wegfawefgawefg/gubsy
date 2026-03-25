#include "game/menu/screens/input_devices_screen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/globals.hpp"
#include "engine/input_sources.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kDevicesPerPage = 4;
constexpr WidgetId kTitleWidgetId = 1200;
constexpr WidgetId kStatusWidgetId = 1201;
constexpr WidgetId kPageLabelWidgetId = 1202;
constexpr WidgetId kPrevButtonId = 1203;
constexpr WidgetId kNextButtonId = 1204;
constexpr WidgetId kBackButtonId = 1230;
constexpr WidgetId kFirstCardWidgetId = 1220;

MenuCommandId g_cmd_page_delta = kMenuIdInvalid;
MenuCommandId g_cmd_toggle_device = kMenuIdInvalid;

struct DisplayDeviceEntry {
    std::string label;
    std::vector<PlayerDeviceKey> keys;
};

struct InputDevicesState {
    int page = 0;
    int total_pages = 1;
    std::string page_text;
    std::vector<DisplayDeviceEntry> devices;
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
    auto& st = ctx.state<InputDevicesState>();
    int max_page = std::max(0, st.total_pages - 1);
    st.page = std::clamp(st.page + delta, 0, max_page);
    if (st.total_pages <= 0) {
        st.page_text = "Page 0 / 0";
    } else {
        st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);
    }
}

void command_toggle_device(MenuContext& ctx, std::int32_t index) {
    auto& st = ctx.state<InputDevicesState>();
    if (index < 0 || index >= static_cast<int>(st.devices.size()))
        return;
    int player_index = lobby_state().selected_player_index;
    const auto& entry = st.devices[static_cast<std::size_t>(index)];
    bool all_enabled = true;
    for (const auto& key : entry.keys) {
        if (!lobby_device_enabled(player_index, key.type, key.id)) {
            all_enabled = false;
            break;
        }
    }
    if (all_enabled) {
        for (const auto& key : entry.keys)
            lobby_toggle_device(player_index, key.type, key.id);
        return;
    }
    for (const auto& key : entry.keys) {
        if (!lobby_device_enabled(player_index, key.type, key.id))
            lobby_toggle_device(player_index, key.type, key.id);
    }
}

BuiltScreen build_input_devices(MenuContext& ctx) {
    auto& st = ctx.state<InputDevicesState>();
    int player_index = lobby_state().selected_player_index;
    lobby_ensure_player_devices(player_index);

    st.devices.clear();
    if (es) {
        PlayerDeviceKey keyboard_key{};
        PlayerDeviceKey mouse_key{};
        bool has_keyboard = false;
        bool has_mouse = false;
        for (const auto& src : es->input_sources) {
            if (src.type == InputSourceType::Keyboard) {
                keyboard_key = PlayerDeviceKey{static_cast<int>(src.type), src.device_id.id};
                has_keyboard = true;
                continue;
            }
            if (src.type == InputSourceType::Mouse) {
                mouse_key = PlayerDeviceKey{static_cast<int>(src.type), src.device_id.id};
                has_mouse = true;
                continue;
            }
            if (src.type == InputSourceType::Gamepad) {
                DisplayDeviceEntry entry;
                entry.label = "Gamepad " + std::to_string(src.device_id.id + 1);
                entry.keys.push_back(PlayerDeviceKey{static_cast<int>(src.type), src.device_id.id});
                st.devices.push_back(std::move(entry));
            }
        }
        if (has_keyboard && has_mouse) {
            DisplayDeviceEntry entry;
            entry.label = "Keyboard + Mouse";
            entry.keys.push_back(keyboard_key);
            entry.keys.push_back(mouse_key);
            st.devices.insert(st.devices.begin(), std::move(entry));
        } else if (has_keyboard) {
            DisplayDeviceEntry entry;
            entry.label = "Keyboard";
            entry.keys.push_back(keyboard_key);
            st.devices.insert(st.devices.begin(), std::move(entry));
        } else if (has_mouse) {
            DisplayDeviceEntry entry;
            entry.label = "Mouse";
            entry.keys.push_back(mouse_key);
            st.devices.insert(st.devices.begin(), std::move(entry));
        }
    }

    int total_devices = static_cast<int>(st.devices.size());
    st.total_pages = std::max(1, (total_devices + kDevicesPerPage - 1) / kDevicesPerPage);
    st.page = std::clamp(st.page, 0, st.total_pages - 1);
    st.page_text = "Page " + std::to_string(st.page + 1) + " / " + std::to_string(st.total_pages);

    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();
    text_cache.reserve(8);

    widgets.push_back(make_label_widget(kTitleWidgetId, InputDevicesObjectID::TITLE, "Input Devices"));

    text_cache.emplace_back("Player " + std::to_string(player_index + 1));
    MenuWidget status_label =
        make_label_widget(kStatusWidgetId, InputDevicesObjectID::STATUS, text_cache.back().c_str());
    widgets.push_back(status_label);

    MenuWidget page_label = make_label_widget(kPageLabelWidgetId, InputDevicesObjectID::PAGE, st.page_text.c_str());
    page_label.label = st.page_text.c_str();
    widgets.push_back(page_label);

    MenuAction prev_action = MenuAction::none();
    MenuAction next_action = MenuAction::none();
    if (st.page > 0 && g_cmd_page_delta != kMenuIdInvalid)
        prev_action = MenuAction::run_command(g_cmd_page_delta, -1);
    if (st.page + 1 < st.total_pages && g_cmd_page_delta != kMenuIdInvalid)
        next_action = MenuAction::run_command(g_cmd_page_delta, +1);

    MenuWidget prev_btn = make_button_widget(kPrevButtonId, InputDevicesObjectID::PREV, "<", prev_action);
    MenuWidget next_btn = make_button_widget(kNextButtonId, InputDevicesObjectID::NEXT, ">", next_action);
    widgets.push_back(prev_btn);
    std::size_t prev_idx = widgets.size() - 1;
    widgets.push_back(next_btn);
    std::size_t next_idx = widgets.size() - 1;

    std::vector<WidgetId> card_ids;
    card_ids.reserve(kDevicesPerPage);
    std::size_t cards_offset = widgets.size();
    int start_index = st.page * kDevicesPerPage;
    for (int i = 0; i < kDevicesPerPage; ++i) {
        int device_index = start_index + i;
        UILayoutObjectId slot = static_cast<UILayoutObjectId>(InputDevicesObjectID::CARD0 + i);
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        if (device_index < static_cast<int>(st.devices.size())) {
            const DisplayDeviceEntry& entry = st.devices[static_cast<std::size_t>(device_index)];
            MenuWidget card;
            card.id = widget_id;
            card.slot = slot;
            card.type = WidgetType::Button;
            text_cache.emplace_back(entry.label);
            card.label = text_cache.back().c_str();
            bool enabled = true;
            for (const auto& key : entry.keys) {
                if (!lobby_device_enabled(player_index, key.type, key.id)) {
                    enabled = false;
                    break;
                }
            }
            card.badge = enabled ? "Enabled" : "Disabled";
            card.on_select = MenuAction::run_command(g_cmd_toggle_device, device_index);
            card.on_left = prev_action;
            card.on_right = next_action;
            widgets.push_back(card);
            card_ids.push_back(widget_id);
        } else {
            widgets.push_back(make_label_widget(widget_id, slot, ""));
        }
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, InputDevicesObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& prev_ref = widgets[prev_idx];
    MenuWidget& next_ref = widgets[next_idx];
    MenuWidget& back_ref = widgets[back_idx];

    WidgetId first_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.front();
    WidgetId last_card_id = card_ids.empty() ? kMenuIdInvalid : card_ids.back();
    WidgetId cards_start = first_card_id != kMenuIdInvalid ? first_card_id : back_ref.id;

    prev_ref.nav_left = prev_ref.id;
    prev_ref.nav_right = next_ref.id;
    prev_ref.nav_down = cards_start;
    prev_ref.nav_up = prev_ref.id;

    next_ref.nav_left = prev_ref.id;
    next_ref.nav_right = next_ref.id;
    next_ref.nav_down = cards_start;
    next_ref.nav_up = next_ref.id;

    for (std::size_t i = 0; i < card_ids.size(); ++i) {
        MenuWidget& card = widgets[cards_offset + i];
        card.nav_left = prev_ref.id;
        card.nav_right = next_ref.id;
        card.nav_up = (i == 0) ? prev_ref.id : card_ids[i - 1];
        card.nav_down = (i + 1 < card_ids.size()) ? card_ids[i + 1] : back_ref.id;
    }

    back_ref.nav_up = (last_card_id != kMenuIdInvalid) ? last_card_id : prev_ref.id;
    back_ref.nav_left = prev_ref.id;
    back_ref.nav_right = next_ref.id;
    back_ref.nav_down = back_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::INPUT_DEVICES_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = cards_start != kMenuIdInvalid ? cards_start : back_ref.id;
    return built;
}

} // namespace

void register_input_devices_screen() {
    if (!es)
        return;

    if (g_cmd_page_delta == kMenuIdInvalid)
        g_cmd_page_delta = es->menu_commands.register_command(command_page_delta);
    if (g_cmd_toggle_device == kMenuIdInvalid)
        g_cmd_toggle_device = es->menu_commands.register_command(command_toggle_device);

    MenuScreenDef def;
    def.id = MenuScreenID::INPUT_DEVICES;
    def.layout = UILayoutID::INPUT_DEVICES_SCREEN;
    def.state_ops = screen_state_ops<InputDevicesState>();
    def.build = build_input_devices;
    es->menu_manager.register_screen(def);
}
