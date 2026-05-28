#include "gubsy/runtime.hpp"
#include "gubsy/menu/ids.hpp"
#include "src/gubsy_runtime_internal.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_system_state.hpp"
#include "src/menu_layout_ids.hpp"

#include <glayout/layout.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct SmokeState {
    bool host_called{false};
    bool join_called{false};
    bool leave_called{false};
    bool direct_kick_called{false};
    bool validate_remote_called{false};
    bool apply_remote_called{false};
    std::string direct_kicked_member_id;
    std::string expected_host{"127.0.0.1"};
    std::uint16_t expected_port{45454};
};

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool has_alert_containing(const EngineState& engine, const std::string& needle) {
    return std::any_of(engine.alerts.begin(), engine.alerts.end(), [&](const Alert& alert) {
        return alert.text.find(needle) != std::string::npos;
    });
}

const MenuWidget* widget_by_slot(const EngineState& engine, UILayoutObjectId slot) {
    const auto& menu = menu_system_internal::runtime_state(engine);
    auto it = std::find_if(menu.cache.widgets.begin(), menu.cache.widgets.end(),
                           [&](const MenuWidget& widget) { return widget.slot == slot; });
    return it == menu.cache.widgets.end() ? nullptr : &*it;
}

SDL_FRect widget_rect_by_slot(const EngineState& engine, UILayoutObjectId slot) {
    const auto& menu = menu_system_internal::runtime_state(engine);
    for (std::size_t i = 0; i < menu.cache.widgets.size() && i < menu.cache.rects.size(); ++i) {
        if (menu.cache.widgets[i].slot == slot)
            return menu.cache.rects[i];
    }
    return SDL_FRect{};
}

bool rect_has_area(const SDL_FRect& rect) {
    return rect.w > 1.0f && rect.h > 1.0f;
}

bool rects_overlap(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

bool rendered_lobby_smoke_enabled() {
    const char* value = std::getenv("GUBSY_RENDER_SMOKE");
    return value != nullptr && std::string(value) == "1";
}

void ensure_renderer(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    if (engine.ui_layouts.layouts.empty()) {
        glayout::ParseResult layouts =
            glayout::load_layout_file(std::filesystem::current_path() / "data" / "ui_layouts" /
                                      "layouts.lisp");
        require(layouts.ok && !layouts.layouts.empty(),
                "render smoke failed to load project UI layouts");
        engine.ui_layouts.layouts = std::move(layouts.layouts);
    }

    GubsyFrame frame = gubsy_get_frame(runtime);
    if (frame.renderer != nullptr)
        return;
    require(gubsy_init_sdl_renderer(runtime), "failed to initialize renderer for lobby render smoke");
}

SDL_Surface* render_menu_to_surface(GubsyRuntime& runtime) {
    ensure_renderer(runtime);
    GubsyFrame frame = gubsy_get_frame(runtime);
    require(frame.renderer != nullptr, "render smoke missing SDL renderer");
    require(frame.window_width > 0 && frame.window_height > 0, "render smoke missing window size");

    SDL_SetRenderTarget(frame.renderer, nullptr);
    SDL_SetRenderDrawColor(frame.renderer, 5, 5, 10, 255);
    SDL_RenderClear(frame.renderer);
    gubsy_update_menu(runtime, 0.016f, frame.window_width, frame.window_height);
    gubsy_render_menu(runtime, frame.renderer, frame.window_width, frame.window_height);
    SDL_RenderPresent(frame.renderer);

    SDL_Surface* surface = SDL_RenderReadPixels(frame.renderer, nullptr);
    require(surface != nullptr, "render smoke failed to read rendered pixels");
    require(surface->w == frame.window_width && surface->h == frame.window_height,
            "render smoke readback size mismatch");
    return surface;
}

SDL_Color sample_surface(SDL_Surface* surface, int x, int y) {
    x = std::clamp(x, 0, surface->w - 1);
    y = std::clamp(y, 0, surface->h - 1);
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;
    require(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a),
            "render smoke failed to sample pixel");
    return SDL_Color{r, g, b, a};
}

int count_non_background_pixels(SDL_Surface* surface) {
    int count = 0;
    for (int y = 0; y < surface->h; y += 8) {
        for (int x = 0; x < surface->w; x += 8) {
            SDL_Color px = sample_surface(surface, x, y);
            if (px.r != 5 || px.g != 5 || px.b != 10)
                ++count;
        }
    }
    return count;
}

SDL_Color sample_rect_center(SDL_Surface* surface, const SDL_FRect& rect) {
    return sample_surface(surface,
                          static_cast<int>(rect.x + rect.w * 0.5f),
                          static_cast<int>(rect.y + rect.h * 0.5f));
}

SDL_Color average_rect_color(SDL_Surface* surface, const SDL_FRect& rect) {
    int samples = 0;
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
    const int x0 = std::clamp(static_cast<int>(rect.x), 0, surface->w - 1);
    const int y0 = std::clamp(static_cast<int>(rect.y), 0, surface->h - 1);
    const int x1 = std::clamp(static_cast<int>(rect.x + rect.w), x0 + 1, surface->w);
    const int y1 = std::clamp(static_cast<int>(rect.y + rect.h), y0 + 1, surface->h);
    for (int y = y0; y < y1; y += 6) {
        for (int x = x0; x < x1; x += 6) {
            SDL_Color px = sample_surface(surface, x, y);
            r += px.r;
            g += px.g;
            b += px.b;
            a += px.a;
            ++samples;
        }
    }
    if (samples == 0)
        return SDL_Color{};
    return SDL_Color{
        static_cast<Uint8>(r / samples),
        static_cast<Uint8>(g / samples),
        static_cast<Uint8>(b / samples),
        static_cast<Uint8>(a / samples),
    };
}

void verify_shell_lobby_render(GubsyRuntime& runtime) {
    if (!rendered_lobby_smoke_enabled())
        return;

    SDL_Surface* surface = render_menu_to_surface(runtime);
    require(count_non_background_pixels(surface) > 200,
            "rendered shell lobby should draw substantial non-background UI");

    const EngineState& engine = gubsy_runtime_engine(runtime);
    SDL_FRect players_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD0);
    SDL_FRect stop_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD4);
    require(rect_has_area(players_rect), "rendered shell lobby missing players rect");
    require(rect_has_area(stop_rect), "rendered shell lobby missing stop-hosting rect");
    require(!rects_overlap(players_rect, stop_rect),
            "rendered shell lobby players row and stop-hosting action should be separate");

    SDL_Color players_px = sample_rect_center(surface, players_rect);
    SDL_Color stop_px = sample_rect_center(surface, stop_rect);
    require(players_px.r != 5 || players_px.g != 5 || players_px.b != 10,
            "rendered players row center should not be background");
    require(stop_px.r != 5 || stop_px.g != 5 || stop_px.b != 10,
            "rendered stop-hosting action center should not be background");
    SDL_DestroySurface(surface);
}

void verify_browser_render(GubsyRuntime& runtime) {
    if (!rendered_lobby_smoke_enabled())
        return;

    SDL_Surface* surface = render_menu_to_surface(runtime);
    require(count_non_background_pixels(surface) > 200,
            "rendered browser should draw substantial non-background UI");

    const EngineState& engine = gubsy_runtime_engine(runtime);
    const auto& menu = menu_system_internal::runtime_state(engine);
    const MenuWidget* search = widget_by_slot(engine, SettingsObjectID::SEARCH);
    const MenuWidget* refresh = widget_by_slot(engine, SettingsObjectID::CARD4);
    SDL_FRect search_rect = widget_rect_by_slot(engine, SettingsObjectID::SEARCH);
    SDL_FRect refresh_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD4);
    require(search != nullptr && search->label != nullptr &&
                std::string(search->label) == "Search Servers",
            "rendered browser should put Search Servers in the top search slot");
    require(refresh != nullptr && refresh->label != nullptr &&
                std::string(refresh->label) == "Refresh",
            "rendered browser should put Refresh in the bottom action slot");
    require(rect_has_area(search_rect), "rendered browser missing search rect");
    require(rect_has_area(refresh_rect), "rendered browser missing refresh rect");
    require(search_rect.y < refresh_rect.y,
            "rendered browser search should be above bottom Refresh action");

    for (std::size_t i = 0; i < menu.cache.widgets.size() && i < menu.cache.rects.size(); ++i) {
        const MenuWidget& widget = menu.cache.widgets[i];
        if (widget.badge == nullptr || std::string(widget.badge) != "YOUR ROOM")
            continue;
        const SDL_FRect own_rect = menu.cache.rects[i];
        require(rect_has_area(own_rect), "rendered own-room card missing rect");
        require(search_rect.y + search_rect.h <= own_rect.y,
                "rendered browser own-room card should sit below search");
        require(own_rect.y + own_rect.h <= refresh_rect.y,
                "rendered browser own-room card should sit above Refresh");
        SDL_Color avg = average_rect_color(surface, own_rect);
        if (!(avg.r > avg.g && avg.r > avg.b && avg.g < 120 && avg.b < 130)) {
            std::fprintf(stderr,
                         "[lobby_online_smoke] own-room rendered avg rgb=(%u,%u,%u) style=(%u,%u,%u) rect=(%.1f,%.1f %.1fx%.1f)\n",
                         static_cast<unsigned>(avg.r),
                         static_cast<unsigned>(avg.g),
                         static_cast<unsigned>(avg.b),
                         static_cast<unsigned>(widget.style.bg_r),
                         static_cast<unsigned>(widget.style.bg_g),
                         static_cast<unsigned>(widget.style.bg_b),
                         static_cast<double>(own_rect.x),
                         static_cast<double>(own_rect.y),
                         static_cast<double>(own_rect.w),
                         static_cast<double>(own_rect.h));
        }
        require(avg.r > avg.g && avg.r > avg.b && avg.g < 120 && avg.b < 130,
                "rendered own-room card should read as red/error unavailable");
        SDL_DestroySurface(surface);
        return;
    }
    SDL_DestroySurface(surface);
    require(false, "rendered browser missing own-room card");
}

void verify_host_setup_render(GubsyRuntime& runtime) {
    if (!rendered_lobby_smoke_enabled())
        return;

    SDL_Surface* surface = render_menu_to_surface(runtime);
    require(count_non_background_pixels(surface) > 200,
            "rendered host setup should draw substantial non-background UI");

    const EngineState& engine = gubsy_runtime_engine(runtime);
    SDL_FRect room_name_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD0);
    SDL_FRect port_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD1);
    SDL_FRect max_players_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD2);
    SDL_FRect host_public_rect = widget_rect_by_slot(engine, SettingsObjectID::CARD4);
    SDL_FRect host_direct_rect = widget_rect_by_slot(engine, SettingsObjectID::ACTION);

    require(rect_has_area(room_name_rect), "rendered host setup missing room-name rect");
    require(rect_has_area(port_rect), "rendered host setup missing port rect");
    require(rect_has_area(max_players_rect), "rendered host setup missing max-players rect");
    require(rect_has_area(host_public_rect), "rendered host setup missing Host Public rect");
    require(rect_has_area(host_direct_rect), "rendered host setup missing Host Direct rect");
    require(!rects_overlap(host_public_rect, room_name_rect) &&
                !rects_overlap(host_public_rect, port_rect) &&
                !rects_overlap(host_public_rect, max_players_rect),
            "rendered Host Public should not overlap host setup form rows");
    require(host_public_rect.y > max_players_rect.y,
            "rendered Host Public should be below host setup form rows");
    require(host_direct_rect.y > max_players_rect.y,
            "rendered Host Direct should be below host setup form rows");

    SDL_Color host_public_px = sample_rect_center(surface, host_public_rect);
    require(host_public_px.r != 5 || host_public_px.g != 5 || host_public_px.b != 10,
            "rendered Host Public center should not be background");
    SDL_DestroySurface(surface);
}

void verify_players_row_hierarchy(const EngineState& engine, const char* context) {
    const MenuWidget* status = widget_by_slot(engine, SettingsObjectID::STATUS_RIGHT);
    require(status != nullptr && status->label != nullptr && status->secondary != nullptr,
            "missing shell lobby status hierarchy");
    const MenuWidget* players = widget_by_slot(engine, SettingsObjectID::CARD0);
    require(players != nullptr && players->label != nullptr && players->secondary != nullptr,
            "missing shell lobby players row hierarchy");
    require(std::string(players->label) == "Players",
            (std::string(context) + " players row title should be Players").c_str());
    const std::string status_heading = status->label;
    const std::string players_summary = players->secondary;
    require(players_summary.find("Currently ") == std::string::npos,
            (std::string(context) + " players summary should not contain hosting status").c_str());
    require(players_summary.find("Joined ") == std::string::npos,
            (std::string(context) + " players summary should not contain joined status").c_str());
    require(players_summary.find("gubsy-roomd") == std::string::npos,
            (std::string(context) + " players summary should not contain backend status").c_str());
    if (status_heading.find("Currently ") != std::string::npos ||
        status_heading.find("Joined ") != std::string::npos) {
        require(std::string(players->label) != status_heading,
                (std::string(context) + " players row should not mirror session status").c_str());
    }
}

void verify_shell_lobby_copy(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    require(gubsy_push_menu_screen(runtime, MenuScreenID::SHELL_LOBBY),
            "failed to push shell lobby");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* status = widget_by_slot(engine, SettingsObjectID::STATUS_RIGHT);
    require(status != nullptr, "missing shell lobby status widget");
    require(status->label != nullptr, "missing shell lobby status label");
    require(std::string(status->label) == "Currently Public Hosting via gubsy-roomd",
            "shell lobby status heading should own hosting text");
    require(status->secondary != nullptr, "missing shell lobby status detail");
    require(std::string(status->secondary).find("Players ") != std::string::npos,
            "shell lobby status detail should include room player count");

    const MenuWidget* players = widget_by_slot(engine, SettingsObjectID::CARD0);
    require(players != nullptr, "missing shell lobby players card");
    require(players->label != nullptr && std::string(players->label) == "Players",
            "players card title should be Players");
    require(players->secondary != nullptr, "missing players card summary");
    require(std::string(players->secondary).find("Currently Public Hosting") == std::string::npos,
            "players card summary should not contain hosting status text");
    verify_players_row_hierarchy(engine, "public host");

    const MenuWidget* stop = widget_by_slot(engine, SettingsObjectID::CARD4);
    require(stop != nullptr, "missing bottom stop-hosting command");
    require(stop->label != nullptr && std::string(stop->label) == "Stop Hosting",
            "hosted shell lobby should expose Stop Hosting in the bottom command slot");
    require(stop->secondary != nullptr &&
                std::string(stop->secondary).find("before joining elsewhere") != std::string::npos,
            "stop hosting copy should explain join-before-leave behavior");
    verify_shell_lobby_render(runtime);
}

void verify_joined_shell_lobby_context(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    require(gubsy_push_menu_screen(runtime, MenuScreenID::SHELL_LOBBY),
            "failed to push joined shell lobby");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* status = widget_by_slot(engine, SettingsObjectID::STATUS_RIGHT);
    require(status != nullptr, "missing joined shell lobby status widget");
    require(status->label != nullptr && std::string(status->label) == "Joined Public Game",
            "joined shell lobby status should identify public session");
    require(status->secondary != nullptr, "missing joined shell lobby status detail");
    const std::string detail = status->secondary;
    require(detail.find("Host ") != std::string::npos,
            "joined shell lobby detail missing host context");
    require(detail.find("Players 2/") != std::string::npos,
            "joined shell lobby detail missing player count");
    require(detail.find("1 remote client") != std::string::npos,
            "joined shell lobby detail missing remote client count");
    verify_players_row_hierarchy(engine, "joined public");

    const MenuWidget* host = widget_by_slot(engine, SettingsObjectID::CARD2);
    const MenuWidget* join = widget_by_slot(engine, SettingsObjectID::CARD3);
    require(host == nullptr, "joined shell lobby should hide Host Game while in a session");
    require(join == nullptr, "joined shell lobby should hide Join Game while in a session");
    const MenuWidget* leave = widget_by_slot(engine, SettingsObjectID::CARD4);
    require(leave != nullptr, "missing joined shell lobby leave-session command");
    require(leave->label != nullptr && std::string(leave->label) == "Leave Session",
            "joined shell lobby should expose Leave Session in the bottom command slot");

    const MenuWidget* start = widget_by_slot(engine, SettingsObjectID::ACTION);
    require(start != nullptr, "missing joined shell lobby start action");
    require(start->label != nullptr && std::string(start->label) == "Waiting For Host",
            "joined shell lobby should replace Start Game with waiting state");
    require(start->secondary != nullptr &&
                std::string(start->secondary).find("Only the host") != std::string::npos,
            "joined waiting action should explain host-only start");
    require(start->on_select.type == MenuActionType::None,
            "joined waiting action should not start the game");
    require(start->style.fg_r < 180 && start->style.fg_g < 180 && start->style.fg_b < 180,
            "joined waiting action should be visually muted");
}

void verify_direct_member_shell_context(GubsyRuntime& runtime,
                                        const std::string& remote_member_id) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    engine.menu_manager.clear();
    require(gubsy_push_menu_screen(runtime, MenuScreenID::SHELL_LOBBY),
            "failed to push direct shell lobby");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* status = widget_by_slot(engine, SettingsObjectID::STATUS_RIGHT);
    require(status != nullptr, "missing direct shell lobby status widget");
    require(status->label != nullptr && std::string(status->label) == "Currently Direct Hosting",
            "direct shell lobby status should identify direct hosting");
    require(status->secondary != nullptr, "missing direct shell lobby status detail");
    const std::string detail = status->secondary;
    require(detail.find("Players ") != std::string::npos,
            "direct shell lobby detail missing player count");
    require(detail.find("1 remote client") != std::string::npos,
            "direct shell lobby detail missing remote client count");

    const MenuWidget* players = widget_by_slot(engine, SettingsObjectID::CARD0);
    require(players != nullptr, "missing direct shell lobby players card");
    require(players->label != nullptr && std::string(players->label) == "Players",
            "direct players card title should be Players");
    require(players->secondary != nullptr &&
                std::string(players->secondary).find("1 remote client") != std::string::npos,
            "direct players card summary missing remote client count");
    verify_players_row_hierarchy(engine, "direct host");

    engine.menu_manager.clear();
    require(gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_LOCAL_PLAYERS),
            "failed to push direct players screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const auto& menu = menu_system_internal::runtime_state(engine);
    auto remote_card = std::find_if(menu.cache.widgets.begin(), menu.cache.widgets.end(),
                                    [&](const MenuWidget& widget) {
                                        return widget.secondary != nullptr &&
                                               std::string(widget.secondary).find(remote_member_id) !=
                                                   std::string::npos;
                                    });
    require(remote_card != menu.cache.widgets.end(), "missing direct remote player card");
    const std::string remote_detail = remote_card->secondary;
    require(remote_detail.find("Direct") != std::string::npos,
            "direct remote player detail missing direct backend context");
    require(remote_detail.find("Client 127.0.0.1:45454") != std::string::npos,
            "direct remote player detail missing client label");
    require(remote_detail.find("State Lobby") != std::string::npos,
            "direct remote player detail missing lobby state");
    require(remote_detail.find("Last seen 0s ago") != std::string::npos,
            "direct remote player detail missing last-seen freshness");
    require(remote_detail.find("gubsy-roomd") == std::string::npos,
            "direct remote player detail should not mention room service");
    require(remote_detail.find("Select for actions") != std::string::npos,
            "direct remote player row should open management actions");
    require(remote_card->on_select.type == MenuActionType::RunCommand,
            "direct remote player row should run the open-actions command");

    require(engine.menu_manager.push_screen(MenuScreenID::LOBBY_REMOTE_PLAYER, 0),
            "failed to push direct remote player screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);
    const MenuWidget* kick = widget_by_slot(engine, SettingsObjectID::CARD1);
    require(kick != nullptr, "missing direct remote player action");
    require(kick->label != nullptr && std::string(kick->label) == "Kick Player",
            "direct remote player action screen should expose kick action");
    require(kick->secondary != nullptr &&
                std::string(kick->secondary).find("direct session") != std::string::npos,
            "direct remote player kick action should explain direct disconnection");
    require(kick->on_select.type == MenuActionType::RunCommand,
            "direct remote player kick action should run an explicit command");
    engine.menu_manager.clear();
}

void verify_direct_member_sorting(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    engine.menu_manager.clear();
    require(gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_LOCAL_PLAYERS),
            "failed to push direct players screen for sorting");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const auto& menu = menu_system_internal::runtime_state(engine);
    std::vector<std::string> remote_labels;
    for (const MenuWidget& widget : menu.cache.widgets) {
        if (widget.secondary == nullptr)
            continue;
        const std::string detail = widget.secondary;
        if (detail.find("Direct") == std::string::npos)
            continue;
        remote_labels.push_back(widget.label ? widget.label : "");
    }
    require(remote_labels.size() >= 2, "direct sorting smoke missing remote rows");
    require(remote_labels[0] == "Client A Guest" && remote_labels[1] == "Client B Guest",
            "direct remote rows should sort by client label");
    engine.menu_manager.clear();
}

void verify_own_room_browser_card(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    require(gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_SERVER_BROWSER),
            "failed to push browser screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const auto& menu = menu_system_internal::runtime_state(engine);
    auto own_card =
        std::find_if(menu.cache.widgets.begin(), menu.cache.widgets.end(), [](const MenuWidget& widget) {
            return widget.badge != nullptr && std::string(widget.badge) == "YOUR ROOM";
        });
    require(own_card != menu.cache.widgets.end(), "missing own-room browser card");
    require(own_card->on_select.type == MenuActionType::None,
            "own-room browser card should not have a join action");
    require(own_card->secondary != nullptr &&
                std::string(own_card->secondary).find("Hosting Here | Unavailable") !=
                    std::string::npos,
            "own-room browser card should explain that it is unavailable");
    require(own_card->style.bg_r > own_card->style.bg_g &&
                own_card->style.bg_r > own_card->style.bg_b,
            "own-room browser card should use a red unavailable background");
    require(own_card->style.fg_r < 180 && own_card->style.fg_g < 180 &&
                own_card->style.fg_b < 180,
            "own-room browser card should be greyed out");
    require(own_card->badge_color.r > 220 && own_card->badge_color.g < 120,
            "own-room browser badge should be red");
    verify_browser_render(runtime);
}

void verify_host_screen_layout_and_validation(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    require(gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_HOST_SETUP),
            "failed to push host screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* room_name = widget_by_slot(engine, SettingsObjectID::CARD0);
    const MenuWidget* port = widget_by_slot(engine, SettingsObjectID::CARD1);
    const MenuWidget* max_players = widget_by_slot(engine, SettingsObjectID::CARD2);
    const MenuWidget* host_public = widget_by_slot(engine, SettingsObjectID::CARD4);
    const MenuWidget* host_direct = widget_by_slot(engine, SettingsObjectID::ACTION);
    require(room_name != nullptr && room_name->label != nullptr &&
                std::string(room_name->label) == "Room Name",
            "host screen should label the room name field");
    require(port != nullptr && port->label != nullptr && std::string(port->label) == "Host Port",
            "host screen should label the port field");
    require(max_players != nullptr && max_players->label != nullptr &&
                std::string(max_players->label) == "Max Players",
            "host screen should expose max players");
    require(host_public != nullptr && host_public->label != nullptr &&
                std::string(host_public->label) == "Host Public",
            "host public action should be in the bottom-middle slot");
    require(host_direct != nullptr && host_direct->label != nullptr &&
                std::string(host_direct->label) == "Host Direct",
            "host direct action should be in the bottom-right slot");
    require(host_public->on_select.type == MenuActionType::RunCommand,
            "valid host public action should be actionable");
    require(host_direct->on_select.type == MenuActionType::RunCommand,
            "valid host direct action should be actionable");
    verify_host_setup_render(runtime);

    require(port->text_buffer != nullptr, "host screen port missing text buffer");
    *port->text_buffer = "0";
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* status = widget_by_slot(engine, SettingsObjectID::STATUS);
    host_public = widget_by_slot(engine, SettingsObjectID::CARD4);
    host_direct = widget_by_slot(engine, SettingsObjectID::ACTION);
    require(status != nullptr && status->label != nullptr &&
                std::string(status->label).find("port from 1 to 65535") != std::string::npos,
            "invalid host port should show a status error");
    require(host_public != nullptr && host_public->on_select.type == MenuActionType::None,
            "invalid host port should disable Host Public");
    require(host_direct != nullptr && host_direct->on_select.type == MenuActionType::None,
            "invalid host port should disable Host Direct");
    require(host_public->style.bg_r > host_public->style.bg_g &&
                host_public->style.bg_r > host_public->style.bg_b,
            "invalid Host Public should be red/error styled");
    require(host_direct->style.bg_r > host_direct->style.bg_g &&
                host_direct->style.bg_r > host_direct->style.bg_b,
            "invalid Host Direct should be red/error styled");

    engine.menu_manager.clear();
}

void verify_join_by_ip_validation(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    require(gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_JOIN_BY_IP),
            "failed to push join-by-ip screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* host = widget_by_slot(engine, SettingsObjectID::CARD0);
    const MenuWidget* port = widget_by_slot(engine, SettingsObjectID::CARD1);
    const MenuWidget* action = widget_by_slot(engine, SettingsObjectID::ACTION);
    require(host != nullptr && host->label != nullptr && std::string(host->label) == "IP / Host",
            "join-by-ip host field should be labeled");
    require(port != nullptr && port->label != nullptr && std::string(port->label) == "Port",
            "join-by-ip port field should be labeled");
    require(action != nullptr && action->label != nullptr && std::string(action->label) == "Join",
            "join-by-ip action should be Join");
    require(action->on_select.type == MenuActionType::RunCommand,
            "valid join-by-ip input should be actionable");

    require(port->text_buffer != nullptr, "join-by-ip port missing text buffer");
    *port->text_buffer = "bad";
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* status = widget_by_slot(engine, SettingsObjectID::STATUS);
    action = widget_by_slot(engine, SettingsObjectID::ACTION);
    require(status != nullptr && status->label != nullptr &&
                std::string(status->label).find("port from 1 to 65535") != std::string::npos,
            "invalid join-by-ip port should show a status error");
    require(action != nullptr && action->on_select.type == MenuActionType::None,
            "invalid join-by-ip input should disable the Join action");
    require(action->secondary != nullptr &&
                std::string(action->secondary).find("port from 1 to 65535") != std::string::npos,
            "invalid join-by-ip action should explain the error");
    require(action->style.bg_r > action->style.bg_g && action->style.bg_r > action->style.bg_b,
            "invalid join-by-ip action should be red/error styled");

    *port->text_buffer = "35355";
    engine.lobby.last_error = "No server found at 127.0.0.1:35355";
    gubsy_update_menu(runtime, 0.016f, 1280, 720);
    action = widget_by_slot(engine, SettingsObjectID::ACTION);
    require(action != nullptr && action->on_select.type == MenuActionType::None,
            "join-by-ip failed endpoint should disable the Join action");
    require(action->label != nullptr && std::string(action->label) == "No Server Found",
            "join-by-ip failed endpoint should label the unavailable action");
    require(action->secondary != nullptr &&
                std::string(action->secondary).find("No server found") != std::string::npos,
            "join-by-ip failed attempt should explain unreachable state");
    require(action->style.bg_r > action->style.bg_g && action->style.bg_r > action->style.bg_b,
            "join-by-ip failed attempt should be red/error styled");

    engine.lobby.last_error.clear();
    engine.lobby.status_message.clear();
    engine.menu_manager.clear();
}

void verify_players_remote_detail(GubsyRuntime& runtime, const std::string& remote_member_id) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    require(gubsy_push_menu_screen(runtime, MenuScreenID::LOBBY_LOCAL_PLAYERS),
            "failed to push players screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const auto& menu = menu_system_internal::runtime_state(engine);
    auto remote_card =
        std::find_if(menu.cache.widgets.begin(), menu.cache.widgets.end(), [&](const MenuWidget& widget) {
            if (!widget.secondary)
                return false;
            const std::string detail = widget.secondary;
            return detail.find(remote_member_id) != std::string::npos;
        });
    require(remote_card != menu.cache.widgets.end(), "missing remote player card");
    const std::string detail = remote_card->secondary;
    require(detail.find("gubsy-roomd") != std::string::npos,
            "remote player detail missing backend");
    require(detail.find("Room ") != std::string::npos,
            "remote player detail missing room code");
    require(detail.find("Endpoint ") != std::string::npos,
            "remote player detail missing endpoint");
    require(detail.find("State Lobby") != std::string::npos,
            "remote player detail missing lobby state");
    require(detail.find("Last seen ") != std::string::npos,
            "remote player detail missing last-seen freshness");
    require(detail.find("Select for actions") != std::string::npos,
            "remote player row should open management actions");
    require(remote_card->on_select.type == MenuActionType::RunCommand,
            "remote player row should run the open-actions command");

    require(engine.menu_manager.push_screen(MenuScreenID::LOBBY_REMOTE_PLAYER, 0),
            "failed to push remote player screen");
    gubsy_update_menu(runtime, 0.016f, 1280, 720);

    const MenuWidget* title = widget_by_slot(engine, SettingsObjectID::TITLE);
    require(title != nullptr && title->label != nullptr, "missing remote player title");
    const MenuWidget* info = widget_by_slot(engine, SettingsObjectID::CARD0);
    require(info != nullptr && info->secondary != nullptr, "missing remote player info");
    require(std::string(info->secondary).find(remote_member_id) != std::string::npos,
            "remote player action screen missing selected member context");
    const MenuWidget* kick = widget_by_slot(engine, SettingsObjectID::CARD1);
    require(kick != nullptr, "missing remote player action");
    require(kick->label != nullptr && std::string(kick->label) == "Kick Player",
            "remote player action screen should expose explicit kick action");
    require(kick->secondary != nullptr &&
                std::string(kick->secondary).find("Remove this client") != std::string::npos,
            "remote player kick action should explain its effect");
    require(kick->on_select.type == MenuActionType::RunCommand,
            "remote player kick action should run an explicit command");
}

nlohmann::json serialize_config(void*, const GubsyLobbyState& lobby) {
    nlohmann::json players = nlohmann::json::array();
    for (int i = 0; i < static_cast<int>(lobby.local_players.size()); ++i)
        players.push_back({{"local_index", i}, {"character", "player"}});

    return nlohmann::json{
        {"mode", "campaign"},
        {"respawn_policy", "next_level"},
        {"players", players},
    };
}

bool validate_config(void*, const GubsyLobbyState&, std::string&) {
    return true;
}

bool validate_remote_config(void* user_data, const GubsyLobbyState&, const SessionContract& remote,
                            std::string& message) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->validate_remote_called = true;
    if (!remote.game_config.is_object()) {
        message = "remote config is not an object";
        return false;
    }
    if (remote.game_config.value("mode", "") != "campaign") {
        message = "remote mode is not campaign";
        return false;
    }
    if (remote.game_config.value("respawn_policy", "") != "next_level") {
        message = "remote respawn policy mismatch";
        return false;
    }
    return true;
}

bool apply_remote_config(void* user_data, GubsyLobbyState&, const SessionContract& remote,
                         std::string& message) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->apply_remote_called = true;
    return validate_remote_config(user_data, {}, remote, message);
}

GubsyLobbyHostResult host_transport(void* user_data, const GubsyLobbyState& lobby, std::uint16_t) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->host_called = true;
    require(lobby.contract.game_config.value("mode", "") == "campaign",
            "host saw missing game config");

    GubsyLobbyHostResult result;
    result.ok = true;
    result.status = "host transport started";
    result.advertised_endpoint = state->expected_host + ":" + std::to_string(state->expected_port);
    return result;
}

GubsyLobbyJoinResult join_transport(void* user_data, const GubsyLobbyState&, const char* host,
                                    std::uint16_t port) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->join_called = true;
    require(host != nullptr, "join host is null");
    require(state->expected_host == host, "join host mismatch");
    require(state->expected_port == port, "join port mismatch");

    GubsyLobbyJoinResult result;
    result.ok = true;
    result.status = "join transport connected";
    return result;
}

GubsyLobbyLeaveResult leave_transport(void* user_data, const GubsyLobbyState&) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->leave_called = true;

    GubsyLobbyLeaveResult result;
    result.ok = true;
    result.status = "left transport";
    return result;
}

GubsyLobbyKickResult kick_direct_member(void* user_data, const GubsyLobbyState&,
                                        const MatchmakingMember& member) {
    auto* state = static_cast<SmokeState*>(user_data);
    state->direct_kick_called = true;
    state->direct_kicked_member_id = member.member_id;

    GubsyLobbyKickResult result;
    result.ok = true;
    result.status = "direct kick transport disconnected";
    return result;
}

void install_smoke_hooks(GubsyRuntime& runtime, SmokeState& state) {
    GubsyLobbyConfigProvider provider;
    provider.user_data = &state;
    provider.serialize = serialize_config;
    provider.validate = validate_config;
    provider.validate_remote = validate_remote_config;
    provider.apply_remote = apply_remote_config;
    gubsy_set_lobby_config_provider(runtime, provider);

    GubsyLobbyCommands commands;
    commands.host = host_transport;
    commands.host_user_data = &state;
    commands.join = join_transport;
    commands.join_user_data = &state;
    commands.leave = leave_transport;
    commands.leave_user_data = &state;
    commands.kick_direct_member = kick_direct_member;
    commands.kick_direct_member_user_data = &state;
    gubsy_set_lobby_commands(runtime, commands);
}

void init_runtime(GubsyRuntime& runtime, const char* data_dir) {
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = std::filesystem::current_path().string();
    config.data_root = (std::filesystem::current_path() / "build" / data_dir).string();
    require(init_gubsy_runtime(runtime, config), "failed to init runtime");
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string server_url = "http://127.0.0.1:8788";
        if (argc > 1)
            server_url = argv[1];

        GubsyRuntime host_runtime;
        GubsyRuntime guest_runtime;
        GubsyRuntime other_host_runtime;
        init_runtime(host_runtime, "lobby_online_smoke_host_data");
        init_runtime(guest_runtime, "lobby_online_smoke_guest_data");
        init_runtime(other_host_runtime, "lobby_online_smoke_other_host_data");

        EngineState& host_engine = gubsy_runtime_engine(host_runtime);
        EngineState& guest_engine = gubsy_runtime_engine(guest_runtime);
        EngineState& other_host_engine = gubsy_runtime_engine(other_host_runtime);
        host_engine.lobby.room_server_url = server_url;
        guest_engine.lobby.room_server_url = server_url;
        other_host_engine.lobby.room_server_url = server_url;

        SmokeState host_state;
        SmokeState guest_state;
        SmokeState other_host_state;
        other_host_state.expected_port = 46464;
        install_smoke_hooks(host_runtime, host_state);
        install_smoke_hooks(guest_runtime, guest_state);
        install_smoke_hooks(other_host_runtime, other_host_state);

        gubsy_lobby_ensure_ready(host_engine);
        gubsy_lobby_ensure_ready(other_host_engine);
        require(!host_engine.lobby.lobby_name.empty(), "default lobby name was not generated");
        require(host_engine.lobby.lobby_name != "Local Game",
                "default lobby name should not be Local Game");
        (void)gubsy_lobby_add_local_player(host_engine);
        (void)gubsy_lobby_add_local_player(other_host_engine);
        verify_host_screen_layout_and_validation(host_runtime);
        verify_join_by_ip_validation(guest_runtime);
        std::string message;

        require(gubsy_lobby_host_direct(host_engine, host_state.expected_port, message),
                "host direct failed");
        require(host_state.host_called, "direct host transport was not called");
        require(host_engine.lobby.online, "direct host lobby is not online");
        require(host_engine.lobby.is_host, "direct host lobby is not marked as host");
        require(host_engine.lobby.room_code.empty(), "direct host should not have room code");
        require(host_engine.lobby.advertised_endpoint == "127.0.0.1:45454",
                "direct host advertised endpoint mismatch");
        require(gubsy_lobby_refresh_rooms(guest_engine, true, message),
                "guest direct/private room refresh failed");
        require(guest_engine.lobby.discovered_rooms.empty(),
                "direct/private host should not create a public room listing");

        MatchmakingMember direct_guest;
        direct_guest.member_id = "direct:127.0.0.1:45454";
        direct_guest.display_name = "Direct Guest";
        direct_guest.client_label = "127.0.0.1:45454";
        direct_guest.is_host = false;
        gubsy_lobby_set_direct_members(host_engine, std::vector<MatchmakingMember>{direct_guest},
                                       true);
        require(host_engine.lobby.members.size() == 1,
                "direct host did not cache direct remote member");
        require(host_engine.lobby.room_current_players ==
                    static_cast<int>(host_engine.lobby.local_players.size() + 1),
                "direct host player count did not include direct remote member");
        require(has_alert_containing(host_engine, "Direct Guest joined from client 127.0.0.1:45454"),
                "direct host did not alert direct member join with client label");
        verify_direct_member_shell_context(host_runtime, direct_guest.member_id);
        require(gubsy_lobby_kick_direct_member(host_engine, direct_guest, message),
                "direct host kick failed");
        require(host_state.direct_kick_called, "direct host kick transport was not called");
        require(host_state.direct_kicked_member_id == direct_guest.member_id,
                "direct host kicked wrong member");
        require(message == "direct kick transport disconnected",
                "direct host kick did not surface transport status");
        require(has_alert_containing(host_engine, "direct kick transport disconnected"),
                "direct host did not alert direct kick");

        gubsy_lobby_set_direct_members(host_engine, {}, true);
        require(host_engine.lobby.members.empty(), "direct host did not clear direct members");
        require(has_alert_containing(host_engine, "Direct Guest left from client 127.0.0.1:45454"),
                "direct host did not alert direct member leave with client label");

        MatchmakingMember client_b;
        client_b.member_id = "direct:10.0.0.2:5000";
        client_b.display_name = "Client B Guest";
        client_b.client_label = "client-b";
        MatchmakingMember client_a;
        client_a.member_id = "direct:10.0.0.1:5000";
        client_a.display_name = "Client A Guest";
        client_a.client_label = "client-a";
        gubsy_lobby_set_direct_members(host_engine,
                                       std::vector<MatchmakingMember>{client_b, client_a},
                                       false);
        verify_direct_member_sorting(host_runtime);
        gubsy_lobby_set_direct_members(host_engine, {}, false);

        host_state.expected_port = other_host_state.expected_port;
        host_state.join_called = false;
        host_state.leave_called = false;
        require(gubsy_lobby_join_direct(host_engine, host_state.expected_host,
                                        host_state.expected_port, message),
                "direct host should leave hosting before direct-IP join");
        require(host_state.leave_called,
                "direct host-then-direct-join did not leave hosted session");
        require(host_state.join_called,
                "direct host-then-direct-join did not call join transport");
        require(host_engine.lobby.online,
                "direct host-then-direct-join did not leave runtime online");
        require(!host_engine.lobby.is_host, "direct host-then-direct-join should become a client");
        require(host_engine.lobby.room_code.empty(),
                "direct host-then-direct-join should not keep a public room code");
        require(gubsy_lobby_leave_room(host_engine, message),
                "direct host-then-direct-join cleanup leave failed");
        host_state.expected_port = 45454;
        host_state.host_called = false;
        host_state.join_called = false;
        host_state.leave_called = false;
        require(gubsy_lobby_host_direct(host_engine, host_state.expected_port, message),
                "direct host restore after direct-join smoke failed");
        require(host_state.host_called, "direct host restore did not call host transport");

        require(gubsy_lobby_join_direct(guest_engine, guest_state.expected_host,
                                        guest_state.expected_port, message),
                "guest direct join failed");
        require(guest_state.join_called, "direct guest transport was not called");
        require(guest_engine.lobby.online, "direct guest lobby is not online");
        require(!guest_engine.lobby.is_host, "direct guest lobby is marked as host");
        require(guest_engine.lobby.room_code.empty(), "direct guest should not have room code");

        require(gubsy_lobby_leave_room(guest_engine, message), "direct guest leave failed");
        require(guest_state.leave_called, "direct guest leave transport was not called");
        require(gubsy_lobby_leave_room(host_engine, message), "direct host leave failed");
        require(host_state.leave_called, "direct host leave transport was not called");

        host_state.host_called = false;
        host_state.leave_called = false;
        guest_state.join_called = false;
        guest_state.leave_called = false;

        std::string active_room_name = host_engine.lobby.lobby_name;
        host_engine.lobby.visibility = GubsyLobbyVisibility::Public;
        require(gubsy_lobby_host_room(host_engine, host_state.expected_port, message),
                "host room failed");
        require(host_state.host_called, "host transport was not called");
        require(host_engine.lobby.online, "host lobby is not online");
        require(host_engine.lobby.is_host, "host lobby is not marked as host");
        require(!host_engine.lobby.room_code.empty(), "host room code missing");
        require(host_engine.lobby.contract.game_config.value("mode", "") == "campaign",
                "host contract missing game config");
        require(host_engine.lobby.members.size() == 1,
                "host did not fetch initial room membership");
        require(host_engine.lobby.room_current_players == 1,
                "host did not cache initial room player count");
        verify_shell_lobby_copy(host_runtime);

        const std::string old_room_code = host_engine.lobby.room_code;
        host_engine.lobby.lobby_name = "Bright Rehosted Tunnel";
        active_room_name = host_engine.lobby.lobby_name;
        host_state.host_called = false;
        host_state.leave_called = false;
        require(gubsy_lobby_host_room(host_engine, host_state.expected_port, message),
                "rehost room failed");
        require(host_state.leave_called, "rehost did not leave previous room");
        require(host_state.host_called, "rehost did not restart host transport");
        require(host_engine.lobby.online, "rehost lobby is not online");
        require(host_engine.lobby.is_host, "rehost lobby is not marked as host");
        require(!host_engine.lobby.room_code.empty(), "rehost room code missing");
        require(host_engine.lobby.members.size() == 1,
                "rehost did not fetch initial room membership");
        require(host_engine.lobby.room_current_players == 1,
                "rehost did not cache initial room player count");

        require(gubsy_lobby_refresh_rooms(guest_engine, true, message),
                "guest public room refresh failed");
        if (old_room_code != host_engine.lobby.room_code) {
            auto old_listed =
                std::find_if(guest_engine.lobby.discovered_rooms.begin(),
                             guest_engine.lobby.discovered_rooms.end(),
                             [&](const MatchmakingRoom& room) {
                                 return room.room_code == old_room_code;
                             });
            require(old_listed == guest_engine.lobby.discovered_rooms.end(),
                    "rehost left stale old room listed");
        }
        auto listed_room =
            std::find_if(guest_engine.lobby.discovered_rooms.begin(),
                         guest_engine.lobby.discovered_rooms.end(),
                         [&](const MatchmakingRoom& room) {
                             return room.room_code == host_engine.lobby.room_code;
                         });
        require(listed_room != guest_engine.lobby.discovered_rooms.end(),
                "public hosted room was not listed");
        require(listed_room->session_name == active_room_name,
                "listed room did not keep generated room name");
        require(listed_room->privacy > 0, "listed room was not public");

        host_state.join_called = false;
        host_state.leave_called = false;
        require(!gubsy_lobby_join_room(host_engine, *listed_room, message),
                "host should not join its own public room");
        require(message == "Cannot join room: already hosting this room",
                "unexpected own-room join error");
        require(!host_state.join_called, "own-room join should not call join transport");
        require(!host_state.leave_called, "own-room join should not leave hosting");
        require(host_engine.lobby.online && host_engine.lobby.is_host,
                "own-room join rejection should keep host online");
        verify_own_room_browser_card(host_runtime);

        require(gubsy_lobby_join_room_code(guest_engine, host_engine.lobby.room_code, message),
                "guest join by room code failed");
        require(guest_state.validate_remote_called, "guest did not validate remote config");
        require(guest_state.apply_remote_called, "guest did not apply remote config");
        require(guest_state.join_called, "guest transport was not called");
        require(guest_engine.lobby.online, "guest lobby is not online");
        require(!guest_engine.lobby.is_host, "guest lobby is marked as host");
        require(guest_engine.lobby.room_code == host_engine.lobby.room_code,
                "guest joined wrong room");
        require(guest_engine.lobby.members.size() == 2,
                "guest did not fetch joined room membership");
        require(guest_engine.lobby.room_current_players == 2,
                "guest did not cache joined room player count");
        verify_joined_shell_lobby_context(guest_runtime);

        host_engine.now = host_engine.lobby.next_heartbeat_at + 0.1;
        gubsy_lobby_tick_online(host_engine);
        require(host_engine.lobby.members.size() == 2,
                "host did not refresh joined room membership");
        require(host_engine.lobby.room_current_players == 2,
                "host did not refresh joined room player count");
        require(has_alert_containing(host_engine, "joined"), "host did not alert member join");

        require(gubsy_lobby_leave_room(guest_engine, message), "guest leave failed");
        require(guest_state.leave_called, "guest leave transport was not called");
        host_engine.now = host_engine.lobby.next_heartbeat_at + 0.1;
        gubsy_lobby_tick_online(host_engine);
        require(host_engine.lobby.members.size() == 1,
                "host did not refresh left room membership");
        require(host_engine.lobby.room_current_players == 1,
                "host did not refresh left room player count");
        require(has_alert_containing(host_engine, "left"), "host did not alert member leave");

        guest_state.join_called = false;
        guest_state.leave_called = false;
        require(gubsy_lobby_join_room_code(guest_engine, host_engine.lobby.room_code, message),
                "guest rejoin by room code failed");
        require(guest_state.join_called, "guest rejoin transport was not called");
        host_engine.now = host_engine.lobby.next_heartbeat_at + 0.1;
        gubsy_lobby_tick_online(host_engine);
        require(host_engine.lobby.members.size() == 2,
                "host did not refresh rejoined room membership");
        require(host_engine.lobby.room_current_players == 2,
                "host did not refresh rejoined room player count");
        verify_players_remote_detail(host_runtime, guest_engine.lobby.member_id);

        require(gubsy_lobby_remove_room_member(host_engine, guest_engine.lobby.member_id, message),
                "host kick failed");
        require(host_engine.lobby.members.size() == 1,
                "host did not refresh kicked room membership");
        require(host_engine.lobby.room_current_players == 1,
                "host did not refresh kicked room player count");
        require(has_alert_containing(host_engine, "Kicked"), "host did not alert member kick");
        guest_state.leave_called = false;
        guest_engine.now = guest_engine.lobby.next_heartbeat_at + 0.1;
        gubsy_lobby_tick_online(guest_engine);
        require(guest_state.leave_called, "kicked guest did not disconnect transport");
        require(!guest_engine.lobby.online, "kicked guest stayed online");
        require(guest_engine.lobby.room_code.empty(), "kicked guest kept room code");
        require(has_alert_containing(guest_engine, "Removed from online room"),
                "kicked guest did not receive removal alert");

        other_host_engine.lobby.visibility = GubsyLobbyVisibility::Public;
        other_host_engine.lobby.lobby_name = "Second Public Tunnel";
        require(gubsy_lobby_host_room(other_host_engine, other_host_state.expected_port, message),
                "other host room failed");
        require(!other_host_engine.lobby.room_code.empty(), "other host room code missing");

        require(gubsy_lobby_refresh_rooms(guest_engine, true, message),
                "multi-host room refresh failed");
        auto room_listed = [&](const std::string& room_code) {
            return std::any_of(guest_engine.lobby.discovered_rooms.begin(),
                               guest_engine.lobby.discovered_rooms.end(),
                               [&](const MatchmakingRoom& room) {
                                   return room.room_code == room_code;
                               });
        };
        require(room_listed(host_engine.lobby.room_code),
                "multi-host list omitted first public room");
        require(room_listed(other_host_engine.lobby.room_code),
                "multi-host list omitted second public room");
        auto other_listed_room =
            std::find_if(guest_engine.lobby.discovered_rooms.begin(),
                         guest_engine.lobby.discovered_rooms.end(),
                         [&](const MatchmakingRoom& room) {
                             return room.room_code == other_host_engine.lobby.room_code;
                         });
        require(other_listed_room != guest_engine.lobby.discovered_rooms.end(),
                "multi-host list did not expose second room for browser join");

        host_state.expected_port = other_host_state.expected_port;
        host_state.join_called = false;
        host_state.leave_called = false;
        require(gubsy_lobby_join_room(host_engine, *other_listed_room, message),
                "host should leave old room and join another browser-listed room");
        require(host_state.leave_called,
                "host-then-browser-join did not leave old hosted session");
        require(host_state.join_called, "host-then-browser-join did not call join transport");
        require(host_engine.lobby.online, "host-then-browser-join did not leave runtime online");
        require(!host_engine.lobby.is_host, "host-then-browser-join should become a client");
        require(host_engine.lobby.room_code == other_host_engine.lobby.room_code,
                "host-then-browser-join joined wrong room");

        require(gubsy_lobby_leave_room(host_engine, message), "host leave failed");
        require(host_state.leave_called, "host leave transport was not called");
        require(gubsy_lobby_leave_room(other_host_engine, message), "other host leave failed");

        cleanup_gubsy_runtime(other_host_runtime);
        cleanup_gubsy_runtime(guest_runtime);
        cleanup_gubsy_runtime(host_runtime);
        std::puts("[lobby_online_smoke] ok");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[lobby_online_smoke] %s\n", e.what());
        return 1;
    }
}
