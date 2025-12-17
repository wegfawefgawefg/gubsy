
#include "run.hpp"

#include <SDL.h>
#include "graphics.hpp"
#include "engine_state.hpp"
#include <SDL_mixer.h>
#include "audio.hpp"
#include "mods.hpp"
#include "step.hpp"
#include "globals.hpp"
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



bool do_the_gubsy(){
    ensure_data_folder_structure();

    if (!init_graphics()) {
        SDL_Quit();
        return 1;
    }

    if (!init_engine_state()) {
        cleanup_graphics();
        SDL_Quit();
        return 1;
    }

    load_audio_settings("config/audio.ini");


    if (!init_audio())
        std::fprintf(stderr, "[audio] SDL_mixer init failed: %s\n", Mix_GetError());

    if (!init_mods_manager()) {
        cleanup_audio();
        cleanup_engine_state();
        cleanup_graphics();
        SDL_Quit();
        return 1;
    }

    // setup user profile state
    // add one player with no profile
    add_player();

    // load profiles pool from disk
    load_user_profiles_pool();
    if (es->user_profiles_pool.empty()) {
        UserProfile default_profile = create_default_user_profile();
        es->user_profiles_pool.push_back(default_profile);
    }

    // assign first profile from pool to first player
    if (!es->players.empty() && !es->user_profiles_pool.empty()) {
        es->players[0].profile = es->user_profiles_pool[0];
        es->players[0].has_active_profile = true;
    }

    // detect input sources
    detect_input_sources();
    if (es->input_sources.empty()) {
        std::fprintf(stderr, "[input] Warning: No input sources detected\n");
    }

    // load all profile pools
    load_binds_profiles_pool();
    load_input_settings_profiles_pool();
    load_game_settings_pool();
    load_top_level_game_settings_into_state();

    discover_mods();
    scan_mods_for_sprite_defs();
    load_all_textures_in_sprite_lookup();
    load_mod_sounds();

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_last = SDL_GetPerformanceCounter();
    float accum_sec = 0.0f;
    int frame_counter = 0;
    int last_fps = 0;
    std::string title_buf;

    while (es->running) {
        Uint64 t_now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(t_now - t_last) / static_cast<float>(perf_freq);
        es->dt = dt;
        t_last = t_now;

        update_gubsy_device_inputs_system_from_sdl_events();
        

        poll_fs_mods_hot_reload();

        process_inputs();
        step();

        render();

        accum_sec += dt;
        frame_counter += 1;
        if (accum_sec >= 1.0f) {
            last_fps = frame_counter;
            frame_counter = 0;
            accum_sec -= 1.0f;
            title_buf = "gubsy demo - FPS: " + std::to_string(last_fps);
            SDL_SetWindowTitle(gg->window, title_buf.c_str());
        }
    }

    return 0;
}

bool stop_doing_the_gubsy(){
    cleanup_audio();
    cleanup_engine_state();
    cleanup_graphics();
    SDL_Quit();
    return 1;
}