#pragma once

#include <string>

// Load master/music/sfx volumes from a sexp file.
// Missing files leave current values untouched.
bool load_audio_settings(const std::string& path);

// Persist the current audio settings back to disk in the same format.
bool save_audio_settings(const std::string& path);
