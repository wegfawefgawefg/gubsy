
#pragma once

#include <SDL3/SDL.h>
#if GUB_ENABLE_SDL_MIXER
#include <SDL3_mixer/SDL_mixer.h>
#endif
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// Struct-only audio store; functions operate on it.
struct Audio {
#if GUB_ENABLE_SDL_MIXER
    MIX_Mixer* mixer{nullptr};
    std::vector<MIX_Track*> tracks;
    std::unordered_map<std::string, MIX_Audio*> chunks;
#endif
};

struct EngineState;

// Initialize SDL_mixer and allocate the global Audio instance.
bool init_audio(EngineState& engine);

// Free all loaded chunks, shutdown SDL_mixer, and destroy the global instance.
void cleanup_audio(EngineState& engine);

// Load a sound file (.wav/.ogg) into the global store with a key.
bool load_sound(EngineState& engine, const std::string& key, const std::string& path);

// Play a sound by key from the global store. Optional loops/channel/volume.
void play_sound(EngineState& engine,
                const std::string& key,
                int loops = 0,
                int channel = -1,
                int volume = -1);

// Scan the active mod root for audio assets and load them into the global store.
void load_mod_sounds(EngineState& engine, const std::filesystem::path& mods_root = {});

// Load engine-owned built-in sounds.
void load_builtin_sounds(EngineState& engine);
