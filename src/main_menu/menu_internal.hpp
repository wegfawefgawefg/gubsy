#pragma once

#include "main_menu/menu.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

inline constexpr float MENU_SAFE_MARGIN = 0.05f;
inline constexpr int kMaxBindsProfiles = 50;
inline constexpr float kTextInputErrorDuration = 1.5f;
inline constexpr int kModsPerPage = 6;
inline constexpr float kModsCardHeight = 0.11f;
inline constexpr float kModsCardGap = 0.015f;

enum TextInputMode {
    TEXT_INPUT_NONE = 0,
    TEXT_INPUT_BINDS_NEW = 1,
    TEXT_INPUT_BINDS_RENAME = 2,
    TEXT_INPUT_MODS_SEARCH = 3,
};

inline constexpr int kPresetNameMaxLen = 16;
inline constexpr int kModsSearchMaxLen = 32;

struct MockModEntry {
    std::string id;
    std::string title;
    std::string author;
    std::string version;
    std::string summary;
    std::vector<std::string> dependencies;
    bool required{false};
};

enum class SliderPreviewEvent { Drag, Release };

inline SDL_FRect ndc_to_pixels(const RectNdc& r, int w, int h) {
    float x0 = (MENU_SAFE_MARGIN + r.x) * static_cast<float>(w);
    float y0 = (MENU_SAFE_MARGIN + r.y) * static_cast<float>(h);
    float ww = r.w * static_cast<float>(w);
    float hh = r.h * static_cast<float>(h);
    return SDL_FRect{x0, y0, ww, hh};
}

enum Page {
    MAIN = 0,
    SETTINGS = 1,
    AUDIO = 2,
    VIDEO = 3,
    CONTROLS = 4,
    BINDS = 5,
    OTHER = 6,
    VIDEO2 = 7,
    BINDS_LOAD = 8,
    PLAYERS = 9,
    MODS = 10,
};

std::string trim_copy(const std::string& s);

const std::vector<MockModEntry>& mock_mod_catalog();
void ensure_mod_install_map();
bool is_mod_installed(const std::string& id);
void set_mod_installed(const std::string& id, bool installed);
void rebuild_mods_filter();
void enter_mods_page();

void apply_default_bindings_to_active();
void undo_active_bind_changes();
void delete_binds_profile(const std::string& name);
bool set_active_binds_profile(const std::string& name);
void open_text_input(int mode, int limit, const std::string& initial,
                     const std::string& target, const std::string& title);
void ensure_focus_default(const std::vector<ButtonDesc>& btns);
