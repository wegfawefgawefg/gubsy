#pragma once

#include <string>

struct EngineState;

bool load_audio_settings(EngineState& engine, const std::string& path);
bool save_audio_settings(const EngineState& engine, const std::string& path);
