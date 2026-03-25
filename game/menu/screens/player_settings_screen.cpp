#include "game/menu/screens/player_settings_screen.hpp"

#include <string>
#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

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

BuiltScreen build_player_settings(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    int index = lobby_state().selected_player_index;
    text_cache.emplace_back("Player Settings (Player " + std::to_string(index + 1) + ")");

    MenuWidget title_widget =
        make_label_widget(1000, PlayerSettingsObjectID::TITLE, text_cache.back().c_str());
    widgets.push_back(title_widget);
    widgets.push_back(make_label_widget(1001, PlayerSettingsObjectID::STATUS, "Configure this player."));

    MenuWidget profile_btn =
        make_button_widget(1002, PlayerSettingsObjectID::PROFILE, "Profile", MenuAction::push(MenuScreenID::PROFILE_PICKER));
    MenuWidget inputs_btn =
        make_button_widget(1003, PlayerSettingsObjectID::INPUTS, "Inputs", MenuAction::push(MenuScreenID::INPUT_DEVICES));
    MenuWidget binds_btn = make_button_widget(1004, PlayerSettingsObjectID::BINDS, "Binds", MenuAction::none());
    MenuWidget back = make_button_widget(1030, PlayerSettingsObjectID::BACK, "Back", MenuAction::pop());

    widgets.push_back(profile_btn);
    std::size_t profile_idx = widgets.size() - 1;
    widgets.push_back(inputs_btn);
    std::size_t inputs_idx = widgets.size() - 1;
    widgets.push_back(binds_btn);
    std::size_t binds_idx = widgets.size() - 1;
    widgets.push_back(back);
    std::size_t back_idx = widgets.size() - 1;

    MenuWidget& profile_ref = widgets[profile_idx];
    MenuWidget& inputs_ref = widgets[inputs_idx];
    MenuWidget& binds_ref = widgets[binds_idx];
    MenuWidget& back_ref = widgets[back_idx];

    profile_ref.nav_down = inputs_ref.id;
    inputs_ref.nav_up = profile_ref.id;
    inputs_ref.nav_down = binds_ref.id;
    binds_ref.nav_up = inputs_ref.id;
    binds_ref.nav_down = back_ref.id;
    back_ref.nav_up = binds_ref.id;

    BuiltScreen built;
    built.layout = UILayoutID::PLAYER_SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = profile_ref.id;
    return built;
}

} // namespace

void register_player_settings_screen() {
    if (!es)
        return;

    MenuScreenDef def;
    def.id = MenuScreenID::PLAYER_SETTINGS;
    def.layout = UILayoutID::PLAYER_SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_player_settings;
    es->menu_manager.register_screen(def);
}
