
#include "run.hpp"

#include <SDL.h>
#include "graphics.hpp"
#include "engine_state.hpp"
#include <SDL_mixer.h>
#include "audio.hpp"
#include "mods.hpp"
#include "step.hpp"
#include "globals.hpp"

bool do_the_gubsy(){
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

    load_user_profiles();
    load_input_profiles();

    discover_mods();
    scan_mods_for_sprite_defs();
    load_all_textures_in_sprite_lookup();
    load_mod_sounds();

    ensure_default_input_profile();

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

        collect_inputs();
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