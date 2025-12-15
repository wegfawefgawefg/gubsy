#pragma once

#include <SDL2/SDL.h>
#include <optional>
#include <string>
#include <vector>

struct InputBindings;

struct InputProfileInfo {
    std::string name;
    bool read_only{false};
};

// Loads a simple key=value .ini for input bindings. Lines starting with # are comments.
bool load_input_bindings_from_ini(const std::string& path);

// Saves input bindings to a simple key=value .ini file (creates parent dir if needed).
bool save_input_bindings_to_ini(const std::string& path, const struct InputBindings& b);

// Load/save audio slider settings (master/music/sfx) from config/audio.ini style files.
bool load_audio_settings(const std::string& path);
bool save_audio_settings_to_ini(const std::string& path);

// Ensure a directory exists (mkdir -p behavior). Returns true on success or already exists.
bool ensure_dir(const std::string& dir);

// Input profiles management (config/input_profiles)
std::string input_profiles_dir();
bool ensure_input_profiles_dir();
std::string sanitize_profile_name(const std::string& raw);
std::string make_unique_profile_name(const std::string& base);
std::vector<InputProfileInfo> list_input_profiles();
bool load_input_profile(const std::string& name, struct InputBindings* out);
bool save_input_profile(const std::string& name, const struct InputBindings& b, bool allow_overwrite = true);
bool delete_input_profile(const std::string& name);
bool rename_input_profile(const std::string& from, const std::string& to);
std::string default_input_profile_name();
std::string load_active_input_profile_name();
void save_active_input_profile_name(const std::string& name);
void migrate_legacy_input_config();
