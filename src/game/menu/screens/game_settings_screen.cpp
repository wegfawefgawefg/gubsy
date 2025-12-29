#include "game/menu/screens/game_settings_screen.hpp"

#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
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

BuiltScreen build_game_settings(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label_widget(800, GameSettingsObjectID::TITLE, "Game Settings"));
    widgets.push_back(make_label_widget(801, GameSettingsObjectID::STATUS,
                                        "Game-specific settings go here."));
    MenuWidget back = make_button_widget(830, GameSettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::GAME_SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = back.id;
    return built;
}

} // namespace

void register_game_settings_screen() {
    if (!es)
        return;

    MenuScreenDef def;
    def.id = MenuScreenID::GAME_SETTINGS;
    def.layout = UILayoutID::GAME_SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_game_settings;
    es->menu_manager.register_screen(def);
}
