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
    widgets.clear();

    int index = lobby_state().selected_player_index;
    std::string title = "Player Settings (Player " + std::to_string(index + 1) + ")";

    MenuWidget title_widget = make_label_widget(1000, PlayerSettingsObjectID::TITLE, title.c_str());
    widgets.push_back(title_widget);
    widgets.push_back(make_label_widget(1001, PlayerSettingsObjectID::STATUS,
                                        "Player settings coming next."));
    MenuWidget back = make_button_widget(1030, PlayerSettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::PLAYER_SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = back.id;
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
