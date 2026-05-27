#include "src/audio.hpp"
#include "src/engine_state.hpp"
#include "src/mods.hpp"
#include "src/project_paths.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <system_error>
#if GUB_ENABLE_SDL_MIXER
#include <SDL3_mixer/SDL_mixer.h>
#endif

namespace {

Audio* current_audio(EngineState& engine) {
    return engine.audio;
}

} // namespace

bool init_audio(EngineState& engine) {
    if (!engine.audio)
        engine.audio = new Audio();
#if GUB_ENABLE_SDL_MIXER
    Audio* audio = current_audio(engine);
    audio->mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!audio->mixer)
        return false;

    audio->tracks.reserve(64);
    for (int i = 0; i < 64; ++i) {
        MIX_Track* track = MIX_CreateTrack(audio->mixer);
        if (!track)
            return false;
        audio->tracks.push_back(track);
    }

    return true;
#else
    return false;
#endif
}

void cleanup_audio(EngineState& engine) {
    Audio* audio = current_audio(engine);
    if (!audio)
        return;
#if GUB_ENABLE_SDL_MIXER
    for (MIX_Track* track : audio->tracks) {
        MIX_StopTrack(track, 0);
        MIX_DestroyTrack(track);
    }
    audio->tracks.clear();
    for (auto& kv : audio->chunks)
        MIX_DestroyAudio(kv.second);
    audio->chunks.clear();
    MIX_DestroyMixer(audio->mixer);
    audio->mixer = nullptr;
#endif
    delete audio;
    engine.audio = nullptr;
}

bool load_sound(EngineState& engine, const std::string& key, const std::string& path) {
#if GUB_ENABLE_SDL_MIXER
    Audio* audio = current_audio(engine);
    if (!audio || !audio->mixer)
        return false;
    MIX_Audio* loaded = MIX_LoadAudio(audio->mixer, path.c_str(), false);
    if (!loaded)
        return false;
    if (auto it = audio->chunks.find(key); it != audio->chunks.end()) {
        MIX_DestroyAudio(it->second);
        it->second = loaded;
    } else {
        audio->chunks.emplace(key, loaded);
    }
    return true;
#else
    (void)engine;
    (void)key;
    (void)path;
    return false;
#endif
}

void play_sound(EngineState& engine, const std::string& key, int loops, int /*channel_hint*/, int volume) {
#if GUB_ENABLE_SDL_MIXER
    Audio* audio = current_audio(engine);
    if (!audio || !audio->mixer)
        return;
    auto it = audio->chunks.find(key);
    if (it == audio->chunks.end())
        return;

    MIX_Track* track = nullptr;
    for (MIX_Track* candidate : audio->tracks) {
        if (!MIX_TrackPlaying(candidate)) {
            track = candidate;
            break;
        }
    }
    if (!track && !audio->tracks.empty()) {
        track = audio->tracks.front();
        MIX_StopTrack(track, 0);
    }
    if (!track)
        return;

    int base_volume = (volume >= 0) ? volume : 128;
    base_volume = std::clamp(base_volume, 0, 128);
    float master = 1.0f;
    float sfx = 1.0f;
    master = std::clamp(engine.audio_settings.vol_master, 0.0f, 1.0f);
    sfx = std::clamp(engine.audio_settings.vol_sfx, 0.0f, 1.0f);
    float scaled = static_cast<float>(base_volume) * master * sfx;
    const float gain = std::clamp(scaled / 128.0f, 0.0f, 1.0f);

    if (!MIX_SetTrackAudio(track, it->second))
        return;
    MIX_SetTrackGain(track, gain);

    SDL_PropertiesID props = SDL_CreateProperties();
    if (props != 0)
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    MIX_PlayTrack(track, props);
    if (props != 0)
        SDL_DestroyProperties(props);
#else
    (void)engine;
    (void)key;
    (void)loops;
    (void)volume;
#endif
}

void load_mod_sounds(EngineState& engine, const std::filesystem::path& mods_root) {

    std::error_code ec;
    std::filesystem::path mroot = mods_root;
    if (mroot.empty()) {
        const ModManager* manager = current_mod_manager_const(engine);
        if (manager && !manager->root.empty())
            mroot = std::filesystem::path(manager->root);
        else
            mroot = runtime_mods_path();
    }
    if (!std::filesystem::exists(mroot, ec) || !std::filesystem::is_directory(mroot, ec)) {
        return;
    }

    for (auto const& mod : std::filesystem::directory_iterator(mroot, ec)) {
        if (ec) { ec.clear(); continue; }
        if (!mod.is_directory()) continue;

        std::string modname = mod.path().filename().string();
        auto sp = mod.path() / "sounds";
        if (!std::filesystem::exists(sp, ec) || !std::filesystem::is_directory(sp, ec)) {
            continue;
        }

        for (auto const& f : std::filesystem::directory_iterator(sp, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!f.is_regular_file()) continue;

            auto p = f.path();
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            if (ext == ".wav" || ext == ".ogg") {
                std::string stem = p.stem().string();
                std::string key = modname + ":" + stem;
                (void)load_sound(engine, key, p.string());
            }
        }
    }
}

void load_builtin_sounds(EngineState& engine) {
    if (!current_audio(engine))
        return;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path base = engine_assets_path("sounds");
    if (!fs::exists(base, ec) || !fs::is_directory(base, ec))
        return;
    for (const auto& entry : fs::directory_iterator(base, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file())
            continue;
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".wav" && ext != ".ogg")
            continue;
        std::string key = "base:" + entry.path().stem().string();
        load_sound(engine, key, entry.path().string());
    }
}
