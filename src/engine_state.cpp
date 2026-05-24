#include "engine_state.hpp"

#include "src/audio.hpp"
#include "src/audio_settings.hpp"
#include "src/data.hpp"
#include "src/graphics.hpp"
#include "src/gubsy_runtime_internal.hpp"
#include "src/imgui_debug/imgui_debug.hpp"
#include "src/input_system.hpp"
#include "src/layout_editor/layout_editor.hpp"
#include "src/lobby_state.hpp"
#include "src/menu/menu_system.hpp"
#include "src/menu/menu_system_state.hpp"
#include "src/menu/screens/binds_action_editor_screen.hpp"
#include "src/menu/screens/binds_choose_input_screen.hpp"
#include "src/menu/screens/binds_profile_editor_screen.hpp"
#include "src/menu/screens/binds_profiles_screen.hpp"
#include "src/menu/screens/lobby_game_config_screen.hpp"
#include "src/menu/screens/lobby_local_players_screen.hpp"
#include "src/menu/screens/lobby_online_screens.hpp"
#include "src/menu/screens/lobby_picker_screens.hpp"
#include "src/menu/screens/lobby_player_settings_screen.hpp"
#include "src/menu/screens/main_menu_screen.hpp"
#include "src/menu/screens/mods_screen.hpp"
#include "src/menu/screens/profiles_screen.hpp"
#include "src/menu/screens/settings_hub_screen.hpp"
#include "src/menu/screens/shell_lobby_screen.hpp"
#include "src/menu/settings_category_registry.hpp"
#include "src/mods.hpp"
#include "src/project_paths.hpp"
#include "src/render.hpp"
#include "src/sdl_shim.hpp"
#include "src/settings_defaults.hpp"
#include "src/ui_layouts.hpp"

#include <algorithm>
#include <cmath>

namespace {

void load_shell_data_pools(EngineState& engine) {
    load_audio_settings(engine, data_path("settings_profiles/audio.lisp").string());

    load_user_profiles_pool(engine);
    load_input_settings_profiles_pool(engine);
    load_game_settings_pool(engine);
    load_top_level_game_settings_into_state(engine);
}

} // namespace

GubsyRuntime::GubsyRuntime() : engine_(new EngineState()) {
}

GubsyRuntime::~GubsyRuntime() {
    if (engine_)
        cleanup_engine_state(*engine_);
    delete engine_;
}

GubsyRuntime::GubsyRuntime(GubsyRuntime&& other) noexcept : engine_(other.engine_) {
    other.engine_ = nullptr;
}

GubsyRuntime& GubsyRuntime::operator=(GubsyRuntime&& other) noexcept {
    if (this == &other)
        return *this;
    if (engine_)
        cleanup_engine_state(*engine_);
    delete engine_;
    engine_ = other.engine_;
    other.engine_ = nullptr;
    return *this;
}

EngineState& gubsy_runtime_engine(GubsyRuntime& runtime) {
    return *runtime.engine_;
}

const EngineState& gubsy_runtime_engine(const GubsyRuntime& runtime) {
    return *runtime.engine_;
}

bool init_engine_state(EngineState& engine, const GubsyAppConfig& config) {
    engine.app_config = normalize_gubsy_app_config(config);
    configure_project_paths(engine.app_config);
    engine.menu_manager.set_command_registry(&engine.menu_commands);
    register_engine_settings_schema_entries(engine);
    register_main_menu_screen(engine);
    register_shell_lobby_screen(engine);
    register_lobby_local_players_screen(engine);
    register_lobby_player_settings_screen(engine);
    register_lobby_picker_screens(engine);
    register_lobby_online_screens(engine);
    register_lobby_game_config_screen(engine);
    register_settings_category_screens(engine);
    register_settings_hub_screen(engine);
    register_profiles_screen(engine);
    register_binds_profiles_screen(engine);
    register_binds_profile_editor_screen(engine);
    register_binds_action_editor_screen(engine);
    register_binds_choose_input_screen(engine);
    if (engine.app_config.enable_mod_browser)
        register_mods_menu_screen(engine);
    ensure_data_folder_structure();
    load_ui_layouts_pool(engine);
    load_shell_data_pools(engine);
    return true;
}

void cleanup_engine_state(EngineState& engine) {
    if (engine.mod_manager) {
        delete engine.mod_manager;
        engine.mod_manager = nullptr;
    }
    if (engine.audio)
        cleanup_audio(engine);
    if (engine.graphics)
        cleanup_graphics(engine);
}

bool init_gubsy_runtime(GubsyRuntime& runtime, const GubsyAppConfig& config) {
    return init_engine_state(gubsy_runtime_engine(runtime), config);
}

void cleanup_gubsy_runtime(GubsyRuntime& runtime) {
    cleanup_engine_state(gubsy_runtime_engine(runtime));
}

const GubsyAppConfig& gubsy_runtime_config(const GubsyRuntime& runtime) {
    return gubsy_runtime_engine(runtime).app_config;
}

bool gubsy_runtime_has_menu_screen(const GubsyRuntime& runtime, MenuScreenId screen_id) {
    return gubsy_runtime_engine(runtime).menu_manager.find_screen(screen_id) != nullptr;
}

bool gubsy_attach_sdl_renderer(GubsyRuntime& runtime, SDL_Window* window, SDL_Renderer* renderer,
                               int render_width, int render_height) {
    return attach_external_graphics(gubsy_runtime_engine(runtime), window, renderer, render_width,
                                    render_height);
}

bool gubsy_init_sdl_renderer(GubsyRuntime& runtime) {
    return init_graphics(gubsy_runtime_engine(runtime));
}

GubsyFrame gubsy_get_frame(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    Graphics* graphics = current_graphics(engine);
    if (!graphics) {
        return {};
    }

    int window_w = 0;
    int window_h = 0;
    if (graphics->window) {
        SDL_GetWindowSize(graphics->window, &window_w, &window_h);
        if (window_w > 0 && window_h > 0) {
            graphics->window_dims = {static_cast<unsigned int>(window_w),
                                     static_cast<unsigned int>(window_h)};
        }
    }
    sync_matched_render_resolution(engine);

    return GubsyFrame{
        .backend = GubsyRenderBackend::SDLRenderer,
        .window = graphics->window,
        .renderer = graphics->renderer,
        .render_target = graphics->render_target,
        .window_width = static_cast<int>(graphics->window_dims.x),
        .window_height = static_cast<int>(graphics->window_dims.y),
        .render_width = static_cast<int>(graphics->render_dims.x),
        .render_height = static_cast<int>(graphics->render_dims.y),
    };
}

bool gubsy_draw_frame_to_window(GubsyRuntime& runtime) {
    return render_frame_to_window(gubsy_runtime_engine(runtime));
}

void gubsy_present_frame(GubsyRuntime& runtime) {
    present_frame(gubsy_runtime_engine(runtime));
}

int gubsy_configured_frame_cap_fps(GubsyRuntime& runtime) {
    return configured_frame_cap_fps(gubsy_runtime_engine(runtime));
}

MenuCommandId gubsy_register_menu_command(GubsyRuntime& runtime, GubsyHostMenuCommandFn fn,
                                          void* user_data) {
    return gubsy_runtime_engine(runtime).menu_commands.register_host_command(fn, user_data);
}

void gubsy_register_binds_schema(GubsyRuntime& runtime, const BindsSchema& schema) {
    register_binds_schema(gubsy_runtime_engine(runtime), schema);
}

void gubsy_set_main_menu_commands(GubsyRuntime& runtime, GubsyMainMenuCommands commands) {
    gubsy_runtime_engine(runtime).main_menu_commands = commands;
}

void gubsy_set_lobby_commands(GubsyRuntime& runtime, GubsyLobbyCommands commands) {
    gubsy_runtime_engine(runtime).lobby_commands = commands;
}

void gubsy_set_lobby_config_provider(GubsyRuntime& runtime, GubsyLobbyConfigProvider provider) {
    gubsy_runtime_engine(runtime).lobby_config_provider = provider;
}

const GubsyLobbyState& gubsy_get_lobby_state(GubsyRuntime& runtime) {
    return gubsy_runtime_engine(runtime).lobby;
}

bool gubsy_show_main_menu(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    engine.menu_manager.clear();
    return engine.menu_manager.push_screen(MenuScreenID::SHELL_MAIN);
}

bool gubsy_push_menu_screen(GubsyRuntime& runtime, MenuScreenId screen_id) {
    return gubsy_runtime_engine(runtime).menu_manager.push_screen(screen_id);
}

void gubsy_pop_menu_screen(GubsyRuntime& runtime) {
    gubsy_runtime_engine(runtime).menu_manager.pop_screen();
}

void gubsy_clear_menu_stack(GubsyRuntime& runtime) {
    gubsy_runtime_engine(runtime).menu_manager.clear();
}

void gubsy_set_menu_input(GubsyRuntime& runtime, const MenuInputState& input) {
    menu_system_set_input(gubsy_runtime_engine(runtime), input);
}

void gubsy_process_sdl_event(GubsyRuntime& runtime, const SDL_Event& event) {
    process_gubsy_sdl_event(gubsy_runtime_engine(runtime), event);
}

void gubsy_update_device_state(GubsyRuntime& runtime) {
    update_device_state_from_sdl(gubsy_runtime_engine(runtime));
}

bool gubsy_menu_text_edit_active(GubsyRuntime& runtime) {
    return menu_system_internal::runtime_state(gubsy_runtime_engine(runtime)).text_edit_active;
}

void gubsy_update_menu(GubsyRuntime& runtime, float dt, int screen_width, int screen_height) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    engine.dt = dt;
    engine.now += static_cast<double>(std::max(dt, 0.0f));
    gubsy_lobby_tick_online(engine);
    menu_system_update(engine, dt, screen_width, screen_height);
}

void gubsy_render_menu(GubsyRuntime& runtime, SDL_Renderer* renderer, int screen_width,
                       int screen_height) {
    menu_system_render(gubsy_runtime_engine(runtime), renderer, screen_width, screen_height);
}

void gubsy_begin_debug_frame(GubsyRuntime& runtime, float dt) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    engine.dt = dt;
    engine.fps_accumulator += std::max(dt, 0.0f);
    engine.fps_frame_count += 1;
    if (engine.fps_accumulator >= 0.5f) {
        engine.displayed_fps = static_cast<int>(
            std::round(static_cast<float>(engine.fps_frame_count) / engine.fps_accumulator));
        engine.fps_accumulator = 0.0f;
        engine.fps_frame_count = 0;
    }
    layout_editor_begin_frame(engine, dt);
    imgui_debug_begin_frame(dt);
}

void gubsy_render_debug(GubsyRuntime& runtime, SDL_Renderer* renderer, int screen_width,
                        int screen_height) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    if (layout_editor_is_active(engine)) {
        const Graphics* graphics = current_graphics(engine);
        if (graphics) {
            const SDL_FRect rect = graphics->present_rect;
            layout_editor_render(engine, renderer, static_cast<int>(rect.w),
                                 static_cast<int>(rect.h), rect.x, rect.y);
        } else {
            layout_editor_render(engine, renderer, screen_width, screen_height);
        }
    }
    imgui_debug_render(engine);
}

void gubsy_shutdown_debug(GubsyRuntime& runtime) {
    EngineState& engine = gubsy_runtime_engine(runtime);
    layout_editor_shutdown(engine);
    imgui_debug_shutdown();
}
