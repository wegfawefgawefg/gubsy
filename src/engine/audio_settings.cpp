#include "engine/audio_settings.hpp"

#include "engine/globals.hpp"
#include "engine/utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

std::string trim(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return text.substr(begin, end - begin);
}

std::unordered_map<std::string, float> parse_ini_lines(std::istream& in) {
    std::unordered_map<std::string, float> values;
    std::string line;
    while (std::getline(in, line)) {
        // Strip comments starting with ';' or '#'
        size_t comment = line.find_first_of(";#");
        if (comment != std::string::npos)
            line = line.substr(0, comment);
        line = trim(line);
        if (line.empty())
            continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key.empty() || value.empty())
            continue;
        try {
            float f = std::stof(value);
            values[key] = f;
        } catch (const std::exception&) {
            continue;
        }
    }
    return values;
}

void apply_volume_if_present(const std::unordered_map<std::string, float>& values,
                             const std::string& key, float& target) {
    if (auto it = values.find(key); it != values.end()) {
        target = std::clamp(it->second, 0.0f, 1.0f);
    }
}

} // namespace

bool load_audio_settings(const std::string& path) {
    if (!es)
        return false;
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    auto values = parse_ini_lines(in);
    apply_volume_if_present(values, "master", es->audio_settings.vol_master);
    apply_volume_if_present(values, "music", es->audio_settings.vol_music);
    apply_volume_if_present(values, "sfx", es->audio_settings.vol_sfx);
    return true;
}

bool save_audio_settings_to_ini(const std::string& path) {
    if (!es)
        return false;

    namespace fs = std::filesystem;
    fs::path target(path);
    if (target.has_parent_path()) {
        if (!ensure_dir(target.parent_path().string()))
            return false;
    }

    std::ofstream out(path);
    if (!out.is_open())
        return false;

    auto clamp01 = [](float value) {
        return std::clamp(value, 0.0f, 1.0f);
    };
    out << "master=" << clamp01(es->audio_settings.vol_master) << "\n";
    out << "music=" << clamp01(es->audio_settings.vol_music) << "\n";
    out << "sfx=" << clamp01(es->audio_settings.vol_sfx) << "\n";
    return out.good();
}
