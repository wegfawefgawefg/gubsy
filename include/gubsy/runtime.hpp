#pragma once

#include "gubsy/app.hpp"
#include "gubsy/input/binds_profile.hpp"
#include "gubsy/lobby/commands.hpp"
#include "gubsy/lobby/config.hpp"
#include "gubsy/menu/commands.hpp"
#include "gubsy/menu/ids.hpp"
#include "gubsy/menu/system.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Renderer;
union SDL_Event;
struct SDL_Texture;
struct SDL_Window;

struct EngineState;

class GubsyRuntime {
  public:
    GubsyRuntime();
    ~GubsyRuntime();

    GubsyRuntime(const GubsyRuntime&) = delete;
    GubsyRuntime& operator=(const GubsyRuntime&) = delete;

    GubsyRuntime(GubsyRuntime&& other) noexcept;
    GubsyRuntime& operator=(GubsyRuntime&& other) noexcept;

  private:
    EngineState* engine_{nullptr};

    friend EngineState& gubsy_runtime_engine(GubsyRuntime& runtime);
    friend const EngineState& gubsy_runtime_engine(const GubsyRuntime& runtime);
};

enum class GubsyRenderBackend {
    None = 0,
    SDLRenderer = 1,
};

enum class GubsyAlertSeverity {
    Info = 0,
    Success = 1,
    Warning = 2,
    Error = 3,
    Debug = 4,
};

struct GubsyFrame {
    GubsyRenderBackend backend{GubsyRenderBackend::None};
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    SDL_Texture* render_target{nullptr};
    int window_width{0};
    int window_height{0};
    int render_width{0};
    int render_height{0};
};

bool init_gubsy_runtime(GubsyRuntime& runtime, const GubsyAppConfig& config = {});
void cleanup_gubsy_runtime(GubsyRuntime& runtime);

const GubsyAppConfig& gubsy_runtime_config(const GubsyRuntime& runtime);
bool gubsy_runtime_has_menu_screen(const GubsyRuntime& runtime, MenuScreenId screen_id);

bool gubsy_attach_sdl_renderer(GubsyRuntime& runtime, SDL_Window* window, SDL_Renderer* renderer,
                               int render_width, int render_height);
bool gubsy_init_sdl_renderer(GubsyRuntime& runtime);
GubsyFrame gubsy_get_frame(GubsyRuntime& runtime);
bool gubsy_draw_frame_to_window(GubsyRuntime& runtime);
void gubsy_present_frame(GubsyRuntime& runtime);
int gubsy_configured_frame_cap_fps(GubsyRuntime& runtime);
MenuCommandId gubsy_register_menu_command(GubsyRuntime& runtime, GubsyHostMenuCommandFn fn,
                                          void* user_data);
void gubsy_register_binds_schema(GubsyRuntime& runtime, const BindsSchema& schema);
const std::vector<BindsProfile>& gubsy_get_binds_profiles(GubsyRuntime& runtime);
const BindsProfile* gubsy_find_binds_profile(GubsyRuntime& runtime, int profile_id);
bool gubsy_replace_binds_profile(GubsyRuntime& runtime, const BindsProfile& profile);
bool gubsy_lobby_player_action_down(GubsyRuntime& runtime, int player_index, int action_id);
bool gubsy_lobby_player_axis_1d_down(GubsyRuntime& runtime, int player_index, int axis_1d_id,
                                     float threshold);
int gubsy_add_lobby_local_player(GubsyRuntime& runtime);
bool gubsy_remove_lobby_local_player(GubsyRuntime& runtime, int player_index);
bool gubsy_select_lobby_local_player(GubsyRuntime& runtime, int player_index);
bool gubsy_set_lobby_player_user_profile(GubsyRuntime& runtime, int player_index, int profile_id);
bool gubsy_set_lobby_player_binds_profile(GubsyRuntime& runtime, int player_index, int profile_id);
bool gubsy_set_lobby_player_input_settings_profile(GubsyRuntime& runtime, int player_index,
                                                   int profile_id);
void gubsy_toggle_lobby_player_device(GubsyRuntime& runtime, int player_index,
                                      GubsyLobbyDeviceAssignment device);
bool gubsy_start_lobby_game(GubsyRuntime& runtime, std::string& message);
bool gubsy_host_lobby_direct(GubsyRuntime& runtime, std::uint16_t port, std::string& message);
bool gubsy_join_lobby_direct(GubsyRuntime& runtime, const std::string& host, std::uint16_t port,
                             std::string& message);
void gubsy_confirm_lobby_direct_join(GubsyRuntime& runtime, const std::string& message);
void gubsy_fail_lobby_direct_join(GubsyRuntime& runtime, const std::string& message);
bool gubsy_host_lobby_room(GubsyRuntime& runtime, std::uint16_t port, std::string& message);
bool gubsy_join_lobby_room_code(GubsyRuntime& runtime, const std::string& room_code,
                                std::string& message);
bool gubsy_leave_lobby_room(GubsyRuntime& runtime, std::string& message);
void gubsy_set_main_menu_commands(GubsyRuntime& runtime, GubsyMainMenuCommands commands);
void gubsy_set_in_game_menu_commands(GubsyRuntime& runtime, GubsyInGameMenuCommands commands);
void gubsy_set_lobby_commands(GubsyRuntime& runtime, GubsyLobbyCommands commands);
void gubsy_set_lobby_config_provider(GubsyRuntime& runtime, GubsyLobbyConfigProvider provider);
void gubsy_set_lobby_matchmaking_backend(GubsyRuntime& runtime, IMatchmaking* matchmaking);
const GubsyLobbyState& gubsy_get_lobby_state(GubsyRuntime& runtime);
void gubsy_set_lobby_direct_members(GubsyRuntime& runtime,
                                    const std::vector<MatchmakingMember>& members,
                                    bool alert_changes = true);
bool gubsy_show_main_menu(GubsyRuntime& runtime);
bool gubsy_open_in_game_menu(GubsyRuntime& runtime);
void gubsy_close_in_game_menu(GubsyRuntime& runtime);
bool gubsy_in_game_menu_open(const GubsyRuntime& runtime);
bool gubsy_push_menu_screen(GubsyRuntime& runtime, MenuScreenId screen_id);
void gubsy_pop_menu_screen(GubsyRuntime& runtime);
void gubsy_clear_menu_stack(GubsyRuntime& runtime);
void gubsy_set_menu_input(GubsyRuntime& runtime, const MenuInputState& input);
void gubsy_process_sdl_event(GubsyRuntime& runtime, const SDL_Event& event);
void gubsy_update_device_state(GubsyRuntime& runtime);
bool gubsy_menu_text_edit_active(GubsyRuntime& runtime);
void gubsy_update_menu(GubsyRuntime& runtime, float dt, int screen_width, int screen_height);
void gubsy_render_menu(GubsyRuntime& runtime, SDL_Renderer* renderer, int screen_width,
                       int screen_height);
void gubsy_render_alerts(GubsyRuntime& runtime, SDL_Renderer* renderer, int screen_width);
void gubsy_begin_debug_frame(GubsyRuntime& runtime, float dt);
void gubsy_render_debug(GubsyRuntime& runtime, SDL_Renderer* renderer, int screen_width,
                        int screen_height);
void gubsy_shutdown_debug(GubsyRuntime& runtime);
void gubsy_add_alert(GubsyRuntime& runtime, const std::string& text,
                     GubsyAlertSeverity severity = GubsyAlertSeverity::Info);
