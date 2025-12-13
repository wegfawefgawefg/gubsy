#include "config.hpp"
#include "demo_content.hpp"
#include "demo_items.hpp"
#include "engine/audio.hpp"
#include "engine/graphics.hpp"
#include "engine/input.hpp"
#include "game_modes.hpp"
#include "globals.hpp"
#include "main_menu/menu.hpp"
#include "mods.hpp"
#include "render.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "step.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {
void ensure_default_input_profile() {
    migrate_legacy_input_config();
    ensure_input_profiles_dir();
    auto profiles = list_input_profiles();
    if (profiles.empty()) {
        InputBindings defaults{};
        save_input_profile(default_input_profile_name(), defaults, true);
        profiles = list_input_profiles();
    }
    auto profile_exists = [&](const std::string& name) {
        return std::any_of(profiles.begin(), profiles.end(),
                           [&](const InputProfileInfo& info) { return info.name == name; });
    };

    std::string active_profile = load_active_input_profile_name();
    if (active_profile.empty() || !profile_exists(active_profile))
        active_profile = default_input_profile_name();

    InputBindings loaded_binds{};
    if (!load_input_profile(active_profile, &loaded_binds)) {
        active_profile = default_input_profile_name();
        load_input_profile(active_profile, &loaded_binds);
    }

    ss->input_binds = loaded_binds;
    ss->menu.binds_current_preset = active_profile;
    ss->menu.binds_snapshot = loaded_binds;
    ss->menu.binds_dirty = false;
    refresh_binds_profiles();
    save_active_input_profile_name(active_profile);
}
} // namespace

int main(int argc, char** argv) {
    bool arg_headless = false;
    long arg_frames = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "--headless")
            arg_headless = true;
        else if (a.rfind("--frames=", 0) == 0) {
            std::string v = a.substr(9);
            try {
                arg_frames = std::stol(v);
            } catch (...) {
                arg_frames = -1;
            }
        }
    }

    if (!init_graphics(arg_headless)) {
        SDL_Quit();
        return 1;
    }

    if (!init_state()) {
        cleanup_graphics();
        SDL_Quit();
        return 1;
    }

    load_audio_settings_from_ini("config/audio.ini");

    if (!init_audio())
        std::fprintf(stderr, "[audio] SDL_mixer init failed: %s\n", Mix_GetError());

    if (!init_mods_manager()) {
        cleanup_audio();
        cleanup_state();
        cleanup_graphics();
        SDL_Quit();
        return 1;
    }

    discover_mods();
    scan_mods_for_sprite_defs();
    if (!arg_headless)
        load_all_textures_in_sprite_lookup();
    load_mod_sounds();
    load_demo_content();

    ensure_default_input_profile();

    register_game_modes();

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 t_last = SDL_GetPerformanceCounter();
    float accum_sec = 0.0f;
    int frame_counter = 0;
    int last_fps = 0;
    std::string title_buf;

    while (ss->running) {
        Uint64 t_now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(t_now - t_last) / static_cast<float>(perf_freq);
        ss->dt = dt;
        t_last = t_now;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            process_event(ev);
            if (ev.type == SDL_QUIT)
                ss->running = false;
        }

        poll_fs_mods_hot_reload();

        collect_inputs();
        process_inputs();
        step();

        if (!arg_headless)
            render();

        accum_sec += dt;
        frame_counter += 1;
        if (accum_sec >= 1.0f) {
            last_fps = frame_counter;
            frame_counter = 0;
            accum_sec -= 1.0f;
            title_buf = "gubsy demo - FPS: " + std::to_string(last_fps);
            if (!arg_headless && gg && gg->window)
                SDL_SetWindowTitle(gg->window, title_buf.c_str());
        }

        if (arg_frames >= 0) {
            if (--arg_frames <= 0)
                ss->running = false;
        }
    }

    save_audio_settings_to_ini("config/audio.ini");
    unload_demo_item_defs();
    cleanup_mods_manager();
    cleanup_audio();
    cleanup_state();
    cleanup_graphics();
    SDL_Quit();
    return 0;
}
