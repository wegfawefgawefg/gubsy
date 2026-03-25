
#include "run.hpp"

#include <SDL.h>
#include <filesystem>
#include "graphics.hpp"
#include "engine_state.hpp"
#include <SDL_mixer.h>
#include "audio.hpp"
#include "engine/audio_settings.hpp"
#include "mods.hpp"
#include "step.hpp"
#include "data.hpp"
#include "user_profiles.hpp"
#include "player.hpp"
#include "input_sources.hpp"
#include "binds_profiles.hpp"
#include "input_settings_profiles.hpp"
#include "game_settings.hpp"
#include "top_level_game_settings.hpp"
#include "sdl_shim.hpp"
#include "render.hpp"
#include "engine/mod_host.hpp"
#include "engine/input_system.hpp"
#include "engine/mode_registry.hpp"
#include "engine/imgui_layer.hpp"
#include "engine/imgui_debug/imgui_debug.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/project_paths.hpp"


bool do_the_gubsy(EngineState& engine, const GubsyAppHooks& hooks){
    ensure_data_folder_structure();
    std::error_code mods_ec;
    std::filesystem::create_directories(runtime_mods_path(), mods_ec);

    if (!init_engine_state(engine)) {
        SDL_Quit();
        return 1;
    }
    engine.app_context = hooks.app_context;

    if (!init_graphics(engine)) {
        cleanup_engine_state(engine);
        SDL_Quit();
        return 1;
    }

    if (!init_imgui_layer(current_graphics(engine)->window, current_graphics(engine)->renderer)) {
        std::fprintf(stderr, "[imgui] init failed\n");
    }

    load_audio_settings(engine, data_path("settings_profiles/audio.lisp").string());


    if (!init_audio(engine))
        std::fprintf(stderr, "[audio] SDL_mixer init failed: %s\n", Mix_GetError());

    if (!init_mods_manager(engine, runtime_mods_path().string())) {
        cleanup_audio(engine);
        cleanup_engine_state(engine);
        SDL_Quit();
        return 1;
    }
    load_builtin_sounds(engine);

    // load profiles pool from disk
    load_user_profiles_pool(engine);
    if (engine.user_profiles_pool.empty()) {
        UserProfile default_profile = create_default_user_profile();
        engine.user_profiles_pool.push_back(default_profile);
    }

    // assign first profile from pool to first player
    if (!engine.players.empty() && !engine.user_profiles_pool.empty()) {
        engine.players[0].profile = engine.user_profiles_pool[0];
        engine.players[0].has_active_profile = true;
    }

    // detect input sources
    detect_input_sources(engine);
    if (engine.input_sources.empty()) {
        std::fprintf(stderr, "[input] Warning: No input sources detected\n");
    }

    // load all profile pools
    load_binds_profiles_pool(engine);
    load_input_settings_profiles_pool(engine);
    load_game_settings_pool(engine);
    load_top_level_game_settings_into_state(engine);
    sync_graphics_from_settings(engine);

    discover_mods(engine);
    scan_mods_for_sprite_defs(engine);
    load_all_textures_in_sprite_lookup(engine);
    load_mod_sounds(engine);

    load_enabled_mods_via_host(engine);
    if (hooks.on_mods_changed)
        hooks.on_mods_changed(hooks.app_context);

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_last = SDL_GetPerformanceCounter();
    float accum_sec = 0.0f;
    int frame_counter = 0;
    int last_fps = 0;
    std::string title_buf;

    while (engine.running) {
        Uint64 t_now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(t_now - t_last) / static_cast<float>(perf_freq);
        engine.dt = dt;
        t_last = t_now;

        update_gubsy_device_inputs_system_from_sdl_events(engine);
        update_device_state_from_sdl(engine);
        imgui_new_frame();
        layout_editor_begin_frame(engine, dt);
        imgui_debug_begin_frame(dt);

        if (const ModeDesc* mode = find_mode(engine, engine.mode)) {
            if (mode->process_inputs_fn)
                mode->process_inputs_fn(engine, engine.app_context);
        }

        bool mods_changed = poll_fs_mods_hot_reload(engine);
        if (mods_changed && hooks.on_mods_changed)
            hooks.on_mods_changed(hooks.app_context);

        step(engine);

        render(engine);

        accum_sec += dt;
        frame_counter += 1;
        if (accum_sec >= 1.0f) {
            last_fps = frame_counter;
            frame_counter = 0;
            accum_sec -= 1.0f;
            title_buf = "gubsy demo - FPS: " + std::to_string(last_fps);
            SDL_SetWindowTitle(current_graphics(engine)->window, title_buf.c_str());
        }
    }

    return 0;
}

bool stop_doing_the_gubsy(EngineState& engine){
    layout_editor_shutdown();
    imgui_debug_shutdown();
    shutdown_imgui_layer();
    unload_all_mods_via_host(engine);
    cleanup_audio(engine);
    cleanup_engine_state(engine);
    SDL_Quit();
    return 1;
}
