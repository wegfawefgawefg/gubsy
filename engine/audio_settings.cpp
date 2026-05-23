#include "engine/audio_settings.hpp"

#include "engine/engine_state.hpp"
#include "engine/sexp_helpers.hpp"
#include "engine/utils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {

void apply_volume_if_present(gsexp::FormView root, std::string_view key, float& target) {
    if (auto value = gubsy_sexp::field_to_float(root, key)) {
        target = std::clamp(*value, 0.0f, 1.0f);
    }
}

} // namespace

bool load_audio_settings(EngineState& engine, const std::string& path) {
    auto text = gubsy_sexp::read_text_file(path);
    if (!text)
        return false;

    gsexp::ParseResult parsed = gsexp::parse_owned(std::move(*text));
    if (!parsed.ok)
        return false;

    gsexp::Node root = gubsy_sexp::find_root(parsed, "audio_settings");
    if (!root.valid())
        return false;

    gsexp::FormView root_view(root);
    apply_volume_if_present(root_view, "master", engine.audio_settings.vol_master);
    apply_volume_if_present(root_view, "music", engine.audio_settings.vol_music);
    apply_volume_if_present(root_view, "sfx", engine.audio_settings.vol_sfx);
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

    auto clamp01 = [](float value) { return std::clamp(value, 0.0f, 1.0f); };

    out << "(audio_settings\n";
    out << "  (master " << clamp01(engine.audio_settings.vol_master) << ")\n";
    out << "  (music " << clamp01(engine.audio_settings.vol_music) << ")\n";
    out << "  (sfx " << clamp01(engine.audio_settings.vol_sfx) << ")\n";
    out << ")\n";
    return out.good();
}
