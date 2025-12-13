#pragma once

#include "main_menu/menu.hpp"
#include "main_menu/menu_layout.hpp"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

inline constexpr float MENU_SAFE_MARGIN = 0.05f;
inline constexpr int kMaxBindsProfiles = 50;
inline constexpr float kTextInputErrorDuration = 1.5f;
inline constexpr int kModsPerPage = 4;
inline constexpr float kModsCardHeight = 0.14f;
inline constexpr float kModsCardGap = 0.02f;
inline constexpr int kLobbyModsPerPage = 5;

struct ModCatalogEntry;

enum TextInputMode {
    TEXT_INPUT_NONE = 0,
    TEXT_INPUT_BINDS_NEW = 1,
    TEXT_INPUT_BINDS_RENAME = 2,
    TEXT_INPUT_MODS_SEARCH = 3,
    TEXT_INPUT_LOBBY_NAME = 4,
    TEXT_INPUT_LOBBY_SEED = 5,
};

inline constexpr int kPresetNameMaxLen = 16;
inline constexpr int kModsSearchMaxLen = 32;
inline constexpr int kLobbyNameMaxLen = 24;
inline constexpr int kLobbySeedMaxLen = 16;

struct ModCatalogEntry;

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
    LOBBY = 11,
    LOBBY_MODS = 12,
};

std::string trim_copy(const std::string& s);

const std::vector<ModCatalogEntry>& menu_mod_catalog();
ModCatalogEntry* find_mod_entry(const std::string& id);
bool ensure_mod_catalog_loaded();
bool is_mod_installed(const std::string& id);
bool install_mod_by_index(int catalog_idx, std::string& error);
bool uninstall_mod_by_index(int catalog_idx, std::string& error);
void rebuild_mods_filter();
void enter_mods_page();
void enter_lobby_page();
void enter_lobby_mods_page();
std::vector<ButtonDesc> build_lobby_buttons();
std::vector<ButtonDesc> build_lobby_mod_buttons();
void render_lobby(int width, int height, const std::vector<ButtonDesc>& buttons);
void render_lobby_mods(int width, int height, const std::vector<ButtonDesc>& buttons);
bool lobby_layout_edit_enabled();
void toggle_lobby_layout_edit_mode();
void lobby_layout_editor_handle(const std::vector<ButtonDesc>& buttons, int width, int height);
std::vector<LayoutItem> lobby_layout_items(const std::vector<ButtonDesc>& buttons, int width, int height);
bool lobby_start_session();
bool lobby_handle_nav_direction(int focus_id, int delta);
bool lobby_mods_handle_nav_direction(int focus_id, int delta);
void lobby_handle_button(const ButtonDesc& b, bool activated, bool play_sound_on_success);
void lobby_mods_handle_button(const ButtonDesc& b, bool activated, bool play_sound_on_success);

void apply_default_bindings_to_active();
void undo_active_bind_changes();
void delete_binds_profile(const std::string& name);
bool set_active_binds_profile(const std::string& name);
void open_text_input(int mode, int limit, const std::string& initial,
                     const std::string& target, const std::string& title);
void ensure_focus_default(const std::vector<ButtonDesc>& btns);
