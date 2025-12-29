#include "game/menu/screens/server_browser_screen.hpp"

#include <vector>

#include "engine/globals.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr WidgetId kTitleWidgetId = 700;
constexpr WidgetId kStatusWidgetId = 701;
constexpr WidgetId kFirstCardWidgetId = 720;
constexpr WidgetId kBackButtonId = 730;

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

BuiltScreen build_server_browser(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label_widget(kTitleWidgetId, ServerBrowserObjectID::TITLE, "Server Browser"));
    widgets.push_back(make_label_widget(kStatusWidgetId,
                                        ServerBrowserObjectID::STATUS,
                                        "No servers found yet."));

    for (int i = 0; i < 4; ++i) {
        WidgetId widget_id = static_cast<WidgetId>(kFirstCardWidgetId + static_cast<WidgetId>(i));
        UILayoutObjectId slot =
            static_cast<UILayoutObjectId>(ServerBrowserObjectID::CARD0 + static_cast<UILayoutObjectId>(i));
        MenuWidget card;
        card.id = widget_id;
        card.slot = slot;
        card.type = WidgetType::Card;
        card.label = (i == 0) ? "No servers available" : "";
        widgets.push_back(card);
    }

    MenuWidget back_btn = make_button_widget(kBackButtonId, ServerBrowserObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back_btn);

    BuiltScreen built;
    built.layout = UILayoutID::SERVER_BROWSER_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = back_btn.id;
    return built;
}

} // namespace

void register_server_browser_screen() {
    if (!es)
        return;

    MenuScreenDef def;
    def.id = MenuScreenID::SERVER_BROWSER;
    def.layout = UILayoutID::SERVER_BROWSER_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_server_browser;
    es->menu_manager.register_screen(def);
}
