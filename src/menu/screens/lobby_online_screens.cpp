#include "src/menu/screens/lobby_online_screens.hpp"

#include "gubsy/menu/ids.hpp"
#include "src/alerts.hpp"
#include "src/engine_state.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_commands.hpp"
#include "src/menu/menu_screen.hpp"
#include "src/menu_layout_ids.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr WidgetId kTitleWidgetId = 2700;
constexpr WidgetId kStatusWidgetId = 2701;
constexpr WidgetId kHostInputWidgetId = 2720;
constexpr WidgetId kPortInputWidgetId = 2721;
constexpr WidgetId kActionWidgetId = 2722;
constexpr WidgetId kBackWidgetId = 2730;
constexpr int kDefaultPort = 35355;

MenuCommandId g_cmd_host = kMenuIdInvalid;
MenuCommandId g_cmd_join = kMenuIdInvalid;

struct OnlineState {
    std::string host_text{"127.0.0.1"};
    std::string port_text{std::to_string(kDefaultPort)};
    std::string status_text;
    bool initialized{false};
};

MenuWidget make_label(WidgetId id, UILayoutObjectId slot, const char* label) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Label;
    widget.label = label;
    return widget;
}

MenuWidget make_text(WidgetId id,
                     UILayoutObjectId slot,
                     const char* label,
                     std::string* buffer,
                     int max_len) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::TextInput;
    widget.label = label;
    widget.text_buffer = buffer;
    widget.text_max_len = max_len;
    return widget;
}

MenuWidget make_button(WidgetId id, UILayoutObjectId slot, const char* label, MenuAction action) {
    MenuWidget widget;
    widget.id = id;
    widget.slot = slot;
    widget.type = WidgetType::Button;
    widget.label = label;
    widget.on_select = action;
    return widget;
}

std::uint16_t parse_port(const std::string& text) {
    try {
        int value = std::stoi(text);
        if (value <= 0 || value > 65535)
            return static_cast<std::uint16_t>(kDefaultPort);
        return static_cast<std::uint16_t>(value);
    } catch (...) {
        return static_cast<std::uint16_t>(kDefaultPort);
    }
}

void sync_state_from_lobby(OnlineState& st, const GubsyLobbyState& lobby) {
    if (st.initialized)
        return;
    st.host_text = lobby.join_host.empty() ? "127.0.0.1" : lobby.join_host;
    int port = lobby.network_port > 0 ? lobby.network_port : kDefaultPort;
    st.port_text = std::to_string(port);
    st.initialized = true;
}

bool validate_common(MenuContext& ctx) {
    std::string message;
    if (gubsy_lobby_validate_start(ctx.engine, message))
        return true;
    ctx.engine.lobby.status_message = message;
    add_alert(ctx.engine, message);
    return false;
}

void command_host(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    if (!validate_common(ctx))
        return;

    std::uint16_t port = parse_port(st.port_text);
    ctx.engine.lobby.network_port = static_cast<int>(port);
    if (!ctx.engine.lobby_commands.host) {
        ctx.engine.lobby.status_message = "No host callback registered";
        add_alert(ctx.engine, ctx.engine.lobby.status_message);
        return;
    }
    bool ok = ctx.engine.lobby_commands.host(ctx.engine.lobby_commands.host_user_data,
                                            ctx.engine.lobby,
                                            port);
    ctx.engine.lobby.online = ok;
    ctx.engine.lobby.is_host = ok;
    ctx.engine.lobby.status_message = ok ? "Hosting session" : "Failed to host session";
    add_alert(ctx.engine, ctx.engine.lobby.status_message);
    if (ok)
        ctx.manager.pop_screen();
}

void command_join(MenuContext& ctx, std::int32_t) {
    auto& st = ctx.state<OnlineState>();
    if (!validate_common(ctx))
        return;
    if (st.host_text.empty()) {
        add_alert(ctx.engine, "Host address is required");
        return;
    }

    std::uint16_t port = parse_port(st.port_text);
    ctx.engine.lobby.join_host = st.host_text;
    ctx.engine.lobby.network_port = static_cast<int>(port);
    if (!ctx.engine.lobby_commands.join) {
        ctx.engine.lobby.status_message = "No join callback registered";
        add_alert(ctx.engine, ctx.engine.lobby.status_message);
        return;
    }
    bool ok = ctx.engine.lobby_commands.join(ctx.engine.lobby_commands.join_user_data,
                                            ctx.engine.lobby,
                                            st.host_text.c_str(),
                                            port);
    ctx.engine.lobby.online = ok;
    ctx.engine.lobby.is_host = false;
    ctx.engine.lobby.status_message = ok ? "Joined session" : "Failed to join session";
    add_alert(ctx.engine, ctx.engine.lobby.status_message);
    if (ok)
        ctx.manager.pop_screen();
}

BuiltScreen build_online_screen(MenuContext& ctx, bool host_screen) {
    gubsy_lobby_ensure_ready(ctx.engine);
    auto& st = ctx.state<OnlineState>();
    sync_state_from_lobby(st, ctx.engine.lobby);

    static std::vector<MenuWidget> widgets;
    widgets.clear();

    widgets.push_back(make_label(kTitleWidgetId,
                                 SettingsObjectID::TITLE,
                                 host_screen ? "Host Session" : "Join Session"));
    st.status_text = ctx.engine.lobby.status_message.empty() ? "Network session setup"
                                                             : ctx.engine.lobby.status_message;
    widgets.push_back(make_label(kStatusWidgetId, SettingsObjectID::STATUS, st.status_text.c_str()));

    MenuWidget host_input = make_text(kHostInputWidgetId,
                                      SettingsObjectID::CARD0,
                                      "Host Address",
                                      &st.host_text,
                                      64);
    host_input.placeholder = "127.0.0.1";
    MenuWidget port_input = make_text(kPortInputWidgetId,
                                      host_screen ? SettingsObjectID::CARD0 : SettingsObjectID::CARD1,
                                      "Port",
                                      &st.port_text,
                                      6);
    port_input.placeholder = "35355";

    MenuWidget action = make_button(kActionWidgetId,
                                    host_screen ? SettingsObjectID::CARD1 : SettingsObjectID::CARD2,
                                    host_screen ? "Start Host" : "Join Host",
                                    MenuAction::run_command(host_screen ? g_cmd_host : g_cmd_join));
    MenuWidget back = make_button(kBackWidgetId, SettingsObjectID::BACK, "Back", MenuAction::pop());

    if (host_screen) {
        port_input.nav_down = action.id;
        action.nav_up = port_input.id;
        widgets.push_back(port_input);
    } else {
        host_input.nav_down = port_input.id;
        port_input.nav_up = host_input.id;
        port_input.nav_down = action.id;
        action.nav_up = port_input.id;
        widgets.push_back(host_input);
        widgets.push_back(port_input);
    }
    action.nav_down = back.id;
    back.nav_up = action.id;
    widgets.push_back(action);
    widgets.push_back(back);

    BuiltScreen built;
    built.layout = UILayoutID::SETTINGS_SCREEN;
    built.widgets = MenuWidgetList{widgets};
    built.default_focus = host_screen ? port_input.id : host_input.id;
    return built;
}

BuiltScreen build_host_screen(MenuContext& ctx) {
    return build_online_screen(ctx, true);
}

BuiltScreen build_join_screen(MenuContext& ctx) {
    return build_online_screen(ctx, false);
}

void register_screen(EngineState& engine, MenuScreenId id, MenuBuildFn build) {
    MenuScreenDef def;
    def.id = id;
    def.layout = UILayoutID::SETTINGS_SCREEN;
    def.state_ops = screen_state_ops<OnlineState>();
    def.build = build;
    engine.menu_manager.register_screen(def);
}

} // namespace

void register_lobby_online_screens(EngineState& engine) {
    if (g_cmd_host == kMenuIdInvalid)
        g_cmd_host = engine.menu_commands.register_command(command_host);
    if (g_cmd_join == kMenuIdInvalid)
        g_cmd_join = engine.menu_commands.register_command(command_join);

    register_screen(engine, MenuScreenID::LOBBY_HOST_SETUP, build_host_screen);
    register_screen(engine, MenuScreenID::LOBBY_SERVER_BROWSER, build_join_screen);
}
