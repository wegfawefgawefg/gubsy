#pragma once

#include <SDL2/SDL.h>
#include <optional>
#include <string>
#include <vector>

// Loads a simple key=value .ini for input bindings. Lines starting with # are comments.
bool load_input_bindings_from_ini(const std::string& path);

// Saves input bindings to a simple key=value .ini file (creates parent dir if needed).
bool save_input_bindings_to_ini(const std::string& path, const struct InputBindings& b);

// Load/save audio slider settings (master/music/sfx) from config/audio.ini style files.
bool load_audio_settings_from_ini(const std::string& path);
bool save_audio_settings_to_ini(const std::string& path);

// Ensure a directory exists (mkdir -p behavior). Returns true on success or already exists.
bool ensure_dir(const std::string& dir);

// List available preset names (without extension) in a directory (default: "binds").
std::vector<std::string> list_bind_presets(const std::string& dir = "binds");
