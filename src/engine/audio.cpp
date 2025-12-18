#include "engine/audio.hpp"
#include "globals.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <system_error>
#include <SDL_mixer.h>

bool init_audio() {
    if (!aa) aa = new Audio();
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0)
        return false;

    Mix_AllocateChannels(64);
    // SFX playback expects channels grouped under id 1; assign them up front.
    Mix_GroupChannels(0, 63, 1);
    // Mix_ReserveChannels(2);                  // e.g., 0-1 for UI or music
    // Mix_GroupChannels(2, 63, 1);             // group 1 = SFX

    return true;
}

void cleanup_audio() {
    if (!aa) return;
    for (auto& kv : aa->chunks)
        Mix_FreeChunk(kv.second);
    aa->chunks.clear();
    if (Mix_QuerySpec(nullptr, nullptr, nullptr))
        Mix_CloseAudio();
    delete aa;
    aa = nullptr;
}

bool load_sound(const std::string& key, const std::string& path) {
    if (!aa) return false;
    Mix_Chunk* ch = Mix_LoadWAV(path.c_str());
    if (!ch)
        return false;
    aa->chunks[key] = ch;
    return true;
}

void play_sound(const std::string& key, int loops, int /*channel_hint*/, int volume) {
    if (!aa) return;
    auto it = aa->chunks.find(key);
    if (it == aa->chunks.end()) return;

    int ch = Mix_GroupAvailable(1);
    if (ch == -1) {
        ch = Mix_GroupOldest(1);
        if (ch != -1) Mix_HaltChannel(ch);
    }
    if (ch == -1) return; // nothing to steal

    if (Mix_PlayChannel(ch, it->second, loops) == -1) return;

    int base_volume = (volume >= 0) ? volume : it->second->volume;
    base_volume = std::clamp(base_volume, 0, MIX_MAX_VOLUME);
    float master = 1.0f;
    float sfx = 1.0f;
    master = std::clamp(es->audio_settings.vol_master, 0.0f, 1.0f);
    sfx = std::clamp(es->audio_settings.vol_sfx, 0.0f, 1.0f);
    float scaled = static_cast<float>(base_volume) * master * sfx;
    int final_volume = static_cast<int>(std::round(std::clamp(scaled, 0.0f, static_cast<float>(MIX_MAX_VOLUME))));
    Mix_Volume(ch, final_volume); // per-channel, not global
}


void load_mod_sounds(const std::string& mods_root) {

    std::error_code ec;
    std::filesystem::path mroot = std::filesystem::path(mods_root);
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
                (void)load_sound(key, p.string());
            }
        }
    }
}

void load_builtin_sounds(const std::string& root) {
    if (!aa)
        return;
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path base = fs::path(root);
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
        load_sound(key, entry.path().string());
    }
}
