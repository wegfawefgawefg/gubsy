#include "game/menu/screens/game_settings_screen.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "engine/alerts.hpp"
#include "engine/engine_state.hpp"
#include "engine/menu/menu_commands.hpp"
#include "engine/menu/menu_manager.hpp"
#include "engine/menu/menu_screen.hpp"
#include "game/lobby_config.hpp"
#include "game/menu/lobby_state.hpp"
#include "game/menu/menu_ids.hpp"
#include "game/ui_layout_ids.hpp"

namespace {

constexpr int kScenarioCount = 4;

MenuCommandId g_cmd_scenario_delta = kMenuIdInvalid;
MenuCommandId g_cmd_seed_mode_toggle = kMenuIdInvalid;

bool lobby_settings_editable(const LobbySession& lobby) {
    return !lobby.online.in_room || lobby.online.is_host;
}

void command_scenario_delta(MenuContext& ctx, std::int32_t delta) {
    LobbySession& lobby = lobby_state();
    if (!lobby_settings_editable(lobby)) {
        add_alert(ctx.engine, "Only the host can change online game settings.");
        return;
    }
    lobby.scenario_index = (lobby.scenario_index + delta + kScenarioCount) % kScenarioCount;
}

void command_seed_mode_toggle(MenuContext& ctx, std::int32_t) {
    LobbySession& lobby = lobby_state();
    if (!lobby_settings_editable(lobby)) {
        add_alert(ctx.engine, "Only the host can change online game settings.");
        return;
    }
    lobby.seed_randomized = !lobby.seed_randomized;
    if (lobby.seed_randomized)
        lobby.seed.clear();
}

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

MenuWidget make_option_widget(WidgetId id,
                              UILayoutObjectId slot,
                              const char* label,
                              const char* badge,
                              MenuAction on_left,
                              MenuAction on_right) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::OptionCycle;
    widget.label = label;
    widget.badge = badge;
    widget.on_left = on_left;
    widget.on_right = on_right;
    return widget;
}

BuiltScreen build_game_settings(MenuContext&) {
    static std::vector<MenuWidget> widgets;
    static std::vector<std::string> text_cache;
    widgets.clear();
    text_cache.clear();

    LobbySession& lobby = lobby_state();
    const bool editable = lobby_settings_editable(lobby);

    widgets.push_back(make_label_widget(800, GameSettingsObjectID::TITLE, "Game Settings"));

    text_cache.emplace_back(editable ? "Host-controlled game setup." : "Viewing host-controlled game setup.");
    widgets.push_back(make_label_widget(801, GameSettingsObjectID::STATUS, text_cache.back().c_str()));

    if (editable) {
        text_cache.emplace_back(lobby_scenario_label(lobby.scenario_index));
        widgets.push_back(make_option_widget(820,
                                             GameSettingsObjectID::CARD0,
                                             "Scenario",
                                             text_cache.back().c_str(),
                                             MenuAction::run_command(g_cmd_scenario_delta, -1),
                                             MenuAction::run_command(g_cmd_scenario_delta, +1)));

        widgets.push_back(make_option_widget(821,
                                             GameSettingsObjectID::CARD1,
                                             "Seed Mode",
                                             lobby.seed_randomized ? "Auto" : "Fixed",
                                             MenuAction::run_command(g_cmd_seed_mode_toggle),
                                             MenuAction::run_command(g_cmd_seed_mode_toggle)));

        MenuWidget seed_text;
        seed_text.id = 822;
        seed_text.slot = GameSettingsObjectID::CARD2;
        seed_text.type = WidgetType::TextInput;
        seed_text.label = "Seed";
        seed_text.text_buffer = &lobby.seed;
        seed_text.text_max_len = 32;
        seed_text.placeholder = lobby.seed_randomized ? "Auto generated on start" : "Enter fixed seed";
        seed_text.secondary = lobby.seed_randomized ? "Only used when Seed Mode is Fixed." : "Joined clients will see this update.";
        widgets.push_back(seed_text);
    } else {
        text_cache.emplace_back(std::string("Scenario: ") + lobby_scenario_label(lobby.scenario_index));
        widgets.push_back(make_label_widget(820, GameSettingsObjectID::CARD0, text_cache.back().c_str()));

        text_cache.emplace_back(std::string("Seed Mode: ") + (lobby.seed_randomized ? "Auto" : "Fixed"));
        widgets.push_back(make_label_widget(821, GameSettingsObjectID::CARD1, text_cache.back().c_str()));

        text_cache.emplace_back(lobby.seed_randomized
                                    ? std::string("Seed: generated on start")
                                    : std::string("Seed: ") + (lobby.seed.empty() ? "<empty>" : lobby.seed));
        widgets.push_back(make_label_widget(822, GameSettingsObjectID::CARD2, text_cache.back().c_str()));
    }

    text_cache.emplace_back(std::string("Session Phase: ") + lobby_session_phase(lobby));
    widgets.push_back(make_label_widget(823, GameSettingsObjectID::CARD3, text_cache.back().c_str()));

    MenuWidget back = make_button_widget(830, GameSettingsObjectID::BACK, "Back", MenuAction::pop());
    widgets.push_back(back);

    if (editable) {
        widgets[2].nav_down = widgets[3].id;
        widgets[3].nav_up = widgets[2].id;
        widgets[3].nav_down = widgets[4].id;
        widgets[4].nav_up = widgets[3].id;
        widgets[4].nav_down = back.id;
        back.nav_up = widgets[4].id;
        back.nav_down = back.id;
        back.nav_left = back.id;
        back.nav_right = back.id;
    } else {
        back.nav_up = back.id;
        back.nav_down = back.id;
        back.nav_left = back.id;
        back.nav_right = back.id;
    }

    BuiltScreen built;
    built.layout = UILayoutID::GAME_SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = editable ? widgets[2].id : back.id;
    return built;
}

} // namespace

void register_game_settings_screen(EngineState& engine) {
    if (g_cmd_scenario_delta == kMenuIdInvalid)
        g_cmd_scenario_delta = engine.menu_commands.register_command(command_scenario_delta);
    if (g_cmd_seed_mode_toggle == kMenuIdInvalid)
        g_cmd_seed_mode_toggle = engine.menu_commands.register_command(command_seed_mode_toggle);

    MenuScreenDef def;
    def.id = MenuScreenID::GAME_SETTINGS;
    def.layout = UILayoutID::GAME_SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<int>();
    def.build = build_game_settings;
    engine.menu_manager.register_screen(def);
}
