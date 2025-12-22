#pragma once

#include <string>

inline constexpr const char* kAudioSettingsPath = "data/settings_profiles/audio.ini";

// Load master/music/sfx volumes from a simple key=value .ini file.
// Missing files leave current values untouched.
bool load_audio_settings(const std::string& path);

// Persist the current audio settings back to disk in the same format.
bool save_audio_settings_to_ini(const std::string& path);
