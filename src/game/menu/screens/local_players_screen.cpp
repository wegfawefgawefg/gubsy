#include "game/menu/screens/local_players_screen.hpp"

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

BuiltScreen build_local_players(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label_widget(900, LocalPlayersObjectID::TITLE, "Local Players"));
    widgets.push_back(make_label_widget(901, LocalPlayersObjectID::STATUS,
                                        "Player list setup goes here."));
    MenuWidget back = make_button_widget(930, LocalPlayersObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::LOCAL_PLAYERS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = back.id;
    return built;
}

} // namespace

void register_local_players_screen() {
    if (!es)
        return;

    MenuScreenDef def;
    def.id = MenuScreenID::LOCAL_PLAYERS;
    def.layout = UILayoutID::LOCAL_PLAYERS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_local_players;
    es->menu_manager.register_screen(def);
}
