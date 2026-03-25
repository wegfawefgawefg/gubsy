#include "engine/audio_settings.hpp"

#include "engine/engine_state.hpp"
#include "engine/parser.hpp"
#include "engine/utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

void apply_volume_if_present(const sexp::SValue& root,
                             const std::string& key,
                             float& target) {
    if (auto value = sexp::extract_float(root, key)) {
        target = std::clamp(*value, 0.0f, 1.0f);
    }
}

} // namespace

bool load_audio_settings(EngineState& engine, const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    std::stringstream buffer;
    buffer << in.rdbuf();
    auto parsed = sexp::parse_s_expressions(buffer.str());
    if (!parsed)
        return false;

    const sexp::SValue* root = nullptr;
    for (const auto& node : *parsed) {
        if (node.type != sexp::SValue::Type::List || node.list.empty())
            continue;
        if (sexp::is_symbol(node.list.front(), "audio_settings")) {
            root = &node;
            break;
        }
    }
    if (!root)
        return false;

    apply_volume_if_present(*root, "master", engine.audio_settings.vol_master);
    apply_volume_if_present(*root, "music", engine.audio_settings.vol_music);
    apply_volume_if_present(*root, "sfx", engine.audio_settings.vol_sfx);
    return true;
}

bool save_audio_settings(const EngineState& engine, const std::string& path) {
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

    out << "(audio_settings\n";
    out << "  (master " << clamp01(engine.audio_settings.vol_master) << ")\n";
    out << "  (music " << clamp01(engine.audio_settings.vol_music) << ")\n";
    out << "  (sfx " << clamp01(engine.audio_settings.vol_sfx) << ")\n";
    out << ")\n";
    return out.good();
}
