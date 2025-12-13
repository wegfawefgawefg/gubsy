#include "main_menu/menu_internal.hpp"

#include "globals.hpp"
#include "state.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string to_lower_copy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

} // namespace

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

const std::vector<MockModEntry>& mock_mod_catalog() {
    static const std::vector<MockModEntry> catalog = {
        {"base", "Base Demo Content", "Gubsy Team", "0.1.0",
         "Core pads + engines shipped with the demo project. Always enabled.",
         {}, true},
        {"shuffle_pack", "Pad Shuffle Pack", "Gubsy Labs", "0.1.0",
         "Adds experimental pads that reposition or swap with existing ones.",
         {"base"}, false},
        {"pad_tweaks", "Pad Tweaks", "Gubsy Labs", "0.1.0",
         "Recolors the teleport pad and adds a reset console for testing patches.",
         {"base"}, false},
        {"retro_ui", "Retro UI Theme", "Community", "0.0.3",
         "Palette-swapped HUD, scanlines, and chunky font replacements.",
         {}, false},
        {"audio_plus", "Audio Plus", "SoundSmith", "0.2.1",
         "Extra SFX cues when interacting with pads. Demonstrates audio hooks.",
         {"base"}, false},
    };
    return catalog;
}

void ensure_mod_install_map() {
    if (!ss) return;
    const auto& catalog = mock_mod_catalog();
    for (const auto& entry : catalog) {
        auto it = ss->menu.mods_mock_install_state.find(entry.id);
        if (it == ss->menu.mods_mock_install_state.end()) {
            bool default_state = entry.required || entry.id == "base";
            ss->menu.mods_mock_install_state.emplace(entry.id, default_state);
        }
    }
}

bool is_mod_installed(const std::string& id) {
    if (!ss) return false;
    auto it = ss->menu.mods_mock_install_state.find(id);
    if (it == ss->menu.mods_mock_install_state.end())
        return false;
    return it->second;
}

void set_mod_installed(const std::string& id, bool installed) {
    if (!ss) return;
    ensure_mod_install_map();
    ss->menu.mods_mock_install_state[id] = installed;
}

void rebuild_mods_filter() {
    if (!ss) return;
    ensure_mod_install_map();
    const auto& catalog = mock_mod_catalog();
    ss->menu.mods_visible_indices.clear();
    std::string query = to_lower_copy(ss->menu.mods_search_query);
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const auto& entry = catalog[i];
        if (!query.empty()) {
            std::string haystack = to_lower_copy(entry.title + " " + entry.summary + " " + entry.author);
            if (haystack.find(query) == std::string::npos)
                continue;
        }
        ss->menu.mods_visible_indices.push_back(static_cast<int>(i));
    }
    ss->menu.mods_filtered_count = static_cast<int>(ss->menu.mods_visible_indices.size());
    if (ss->menu.mods_filtered_count == 0) {
        ss->menu.mods_total_pages = 1;
        ss->menu.mods_catalog_page = 0;
    } else {
        ss->menu.mods_total_pages = std::max(1, (ss->menu.mods_filtered_count + kModsPerPage - 1) / kModsPerPage);
        ss->menu.mods_catalog_page = std::clamp(ss->menu.mods_catalog_page, 0, ss->menu.mods_total_pages - 1);
    }
}

void enter_mods_page() {
    if (!ss) return;
    ensure_mod_install_map();
    ss->menu.mods_catalog_page = 0;
    ss->menu.mods_total_pages = 1;
    rebuild_mods_filter();
}
