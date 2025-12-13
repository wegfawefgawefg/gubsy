#include "main_menu/menu_internal.hpp"

#include "demo_items.hpp"
#include "engine/audio.hpp"
#include "globals.hpp"
#include "mods.hpp"
#include "state.hpp"

#include <algorithm>
#include <random>
#include <string>
#include <vector>
#include <unordered_set>

namespace {

constexpr int kMaxLobbyPlayers = 64;
constexpr int kMinLobbyPlayers = 1;

LobbySettings& lobby() {
    return ss->menu.lobby;
}

const LobbySettings& lobby_const() {
    return ss->menu.lobby;
}

const char* difficulty_label(int idx) {
    switch (idx) {
        case 0: return "Easy";
        case 2: return "Hard";
        default: return "Normal";
    }
}

const char* privacy_label(int idx) {
    switch (idx) {
        case 1: return "Friends";
        case 2: return "Public";
        default: return "Solo";
    }
}

std::string scenario_label(int idx) {
    switch (idx) {
        case 0:
        default:
            return "Demo Pad Arena";
    }
}

bool is_required_mod(const ModInfo& info) {
    return info.required || info.name == "base";
}

void ensure_lobby_session_name() {
    if (lobby().session_name.empty())
        lobby().session_name = "Local Session";
}

void refresh_lobby_mod_entries() {
    ensure_lobby_session_name();
    std::vector<LobbyModEntry> fresh;
    if (mm) {
        fresh.reserve(mm->mods.size());
        for (const auto& mod : mm->mods) {
            LobbyModEntry entry;
            entry.id = mod.name;
            entry.title = mod.title.empty() ? mod.name : mod.title;
            entry.author = mod.author;
            entry.description = mod.description;
            entry.dependencies = mod.deps;
            entry.required = is_required_mod(mod);
            entry.enabled = true;
            fresh.push_back(std::move(entry));
        }
    }
    auto& existing = lobby().mods;
    for (auto& entry : fresh) {
        auto it = std::find_if(existing.begin(), existing.end(),
                               [&](const LobbyModEntry& e) { return e.id == entry.id; });
        if (it != existing.end()) {
            entry.enabled = it->enabled || entry.required;
        }
    }
    lobby().mods = std::move(fresh);
    lobby().mod_page = 0;
    lobby().selected_mod = lobby().mods.empty() ? -1 : 0;
}

void draw_section(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color fill, SDL_Color border) {
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(r, &rect);
}

SDL_Rect to_pixels(const RectNdc& ndc, int width, int height) {
    SDL_FRect fr = ndc_to_pixels(ndc, width, height);
    return SDL_Rect{static_cast<int>(fr.x), static_cast<int>(fr.y),
                    static_cast<int>(fr.w), static_cast<int>(fr.h)};
}

void randomize_seed() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 15);
    std::string seed;
    for (int i = 0; i < 8; ++i) {
        int v = dist(rng);
        seed.push_back("0123456789ABCDEF"[v]);
    }
    lobby().seed = seed;
    lobby().seed_randomized = true;
}

RectNdc mod_row_rect(int row) {
    const float start_y = 0.30f;
    const float row_h = 0.095f;
    const float gap = 0.02f;
    return RectNdc{0.10f, start_y + static_cast<float>(row) * (row_h + gap), 0.78f, row_h};
}

std::vector<int> visible_mod_indices() {
    std::vector<int> out;
    const auto& mods = lobby_const().mods;
    if (mods.empty())
        return out;
    int start = lobby_const().mod_page * kLobbyModsPerPage;
    for (int i = 0; i < kLobbyModsPerPage; ++i) {
        int idx = start + i;
        if (idx < (int)mods.size())
            out.push_back(idx);
    }
    return out;
}

std::vector<std::string> enabled_mod_ids() {
    std::vector<std::string> ids;
    for (const auto& entry : lobby_const().mods) {
        if (entry.enabled || entry.required)
            ids.push_back(entry.id);
    }
    return ids;
}

int enabled_mod_count() {
    int count = 0;
    for (const auto& entry : lobby_const().mods) {
        if (entry.enabled || entry.required)
            ++count;
    }
    return count;
}

bool dependencies_satisfied(std::string& error) {
    auto enabled = enabled_mod_ids();
    std::unordered_set<std::string> enabled_set(enabled.begin(), enabled.end());
    for (const auto& entry : lobby_const().mods) {
        if (!(entry.enabled || entry.required))
            continue;
        for (const auto& dep : entry.dependencies) {
            if (enabled_set.find(dep) == enabled_set.end()) {
                error = entry.title + " requires " + dep;
                return false;
            }
        }
    }
    return true;
}

} // namespace

void enter_lobby_page() {
    if (!ss)
        return;
    refresh_lobby_mod_entries();
    ss->menu.page = LOBBY;
    ss->menu.focus_id = -1;
}

void enter_lobby_mods_page() {
    if (!ss)
        return;
    refresh_lobby_mod_entries();
    ss->menu.page = LOBBY_MODS;
    ss->menu.focus_id = -1;
}

std::vector<ButtonDesc> build_lobby_buttons() {
    std::vector<ButtonDesc> buttons;
    buttons.reserve(32);
    auto row_rect = [&](int idx) {
        const float x = 0.06f;
        const float w = 0.32f;
        const float y0 = 0.20f;
        const float row_h = 0.065f;
        const float gap = 0.02f;
        return RectNdc{x, y0 + static_cast<float>(idx) * (row_h + gap), w, row_h};
    };
    buttons.push_back(ButtonDesc{1400, row_rect(0), "Lobby Name", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1401, row_rect(1), "Privacy", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1402, row_rect(2), "Invite Friends", ButtonKind::Button, 0.0f, false});
    buttons.push_back(ButtonDesc{1403, row_rect(3), "Scenario", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1404, row_rect(4), "Difficulty", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1405, row_rect(5), "Max Players", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1406, row_rect(6), "Allow Join", ButtonKind::Toggle, lobby_const().allow_join ? 1.0f : 0.0f, true});
    buttons.push_back(ButtonDesc{1407, row_rect(7), "Seed", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1408, RectNdc{0.06f, 0.72f, 0.20f, 0.065f}, "Randomize Seed", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1409, RectNdc{0.52f, 0.70f, 0.36f, 0.07f}, "Manage Mods", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1440, RectNdc{0.56f, 0.85f, 0.32f, 0.08f}, "Start Game", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1441, RectNdc{0.06f, 0.82f, 0.22f, 0.07f}, "Back", ButtonKind::Button, 0.0f, true});
    return buttons;
}

std::vector<ButtonDesc> build_lobby_mod_buttons() {
    std::vector<ButtonDesc> buttons;
    buttons.reserve(kLobbyModsPerPage + 4);
    const auto& mods = lobby_const().mods;
    int total_pages = std::max(1, (static_cast<int>(mods.size()) + kLobbyModsPerPage - 1) / kLobbyModsPerPage);
    bool prev_enabled = lobby_const().mod_page > 0;
    bool next_enabled = lobby_const().mod_page + 1 < total_pages;
    buttons.push_back(ButtonDesc{1430, RectNdc{0.10f, 0.18f, 0.14f, 0.07f}, "Prev Page", ButtonKind::Button, 0.0f, prev_enabled});
    buttons.push_back(ButtonDesc{1431, RectNdc{0.28f, 0.18f, 0.14f, 0.07f}, "Next Page", ButtonKind::Button, 0.0f, next_enabled});
    auto visible = visible_mod_indices();
    for (int i = 0; i < kLobbyModsPerPage; ++i) {
        RectNdc rect = mod_row_rect(i);
        bool has_entry = i < (int)visible.size();
        float value = has_entry ? static_cast<float>(visible[(size_t)i]) : -1.0f;
        buttons.push_back(ButtonDesc{1420 + i, rect, "", ButtonKind::Button, value, has_entry});
    }
    buttons.push_back(ButtonDesc{1450, RectNdc{0.06f, 0.86f, 0.25f, 0.09f}, "Back", ButtonKind::Button, 0.0f, true});
    return buttons;
}

void render_lobby(int width, int height, const std::vector<ButtonDesc>& buttons) {
    SDL_Renderer* r = gg ? gg->renderer : nullptr;
    if (!r || !gg || !gg->ui_font)
        return;
    SDL_SetRenderDrawColor(r, 12, 10, 18, 255);
    SDL_RenderClear(r);
    auto draw_text = [&](const std::string& text, const SDL_Rect& rect, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex) return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{rect.x + 12, rect.y + (rect.h - th)/2, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };
    auto draw_value_right = [&](const SDL_Rect& rect, const std::string& text, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex) return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{rect.x + rect.w - tw - 12, rect.y + (rect.h - th)/2, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };
    auto draw_button = [&](int id, const std::string& label, const std::string& value) {
        auto it = std::find_if(buttons.begin(), buttons.end(),
                               [&](const ButtonDesc& b) { return b.id == id; });
        if (it == buttons.end())
            return;
        SDL_Rect rect = to_pixels(it->rect, width, height);
        bool focused = (ss->menu.focus_id == it->id);
        SDL_Color fill = focused ? SDL_Color{45, 55, 75, 255} : SDL_Color{30, 33, 40, 255};
        SDL_Color border = focused ? SDL_Color{200, 220, 90, 255} : SDL_Color{120, 120, 135, 255};
        draw_section(r, rect, fill, border);
        draw_text(label, rect, SDL_Color{220, 220, 230, 255});
        if (!value.empty())
            draw_value_right(rect, value, SDL_Color{190, 190, 205, 255});
    };
    draw_button(1400, "Lobby Name", lobby_const().session_name);
    draw_button(1401, "Privacy", privacy_label(lobby_const().privacy));
    draw_button(1402, "Invite Friends", "Unavailable");
    draw_button(1403, "Scenario", scenario_label(lobby_const().scenario_index));
    draw_button(1404, "Difficulty", difficulty_label(lobby_const().difficulty));
    draw_button(1405, "Max Players", std::to_string(lobby_const().max_players));
    draw_button(1406, "Allow Join", lobby_const().allow_join ? "Enabled" : "Disabled");
    std::string seed = lobby_const().seed.empty() ? "Random" : lobby_const().seed;
    draw_button(1407, "Seed", seed);
    draw_button(1408, "Randomize Seed", "");

    auto draw_abs = [&](const std::string& text, int x, int y, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex) return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{x, y, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };

    int title_x = static_cast<int>(MENU_SAFE_MARGIN * static_cast<float>(width)) + 24;
    int title_y = static_cast<int>(MENU_SAFE_MARGIN * static_cast<float>(height)) + 16;
    draw_abs("Game Options", title_x, title_y, SDL_Color{240, 220, 80, 255});

    SDL_Rect mods_panel = to_pixels(RectNdc{0.52f, 0.22f, 0.40f, 0.46f}, width, height);
    draw_section(r, mods_panel, SDL_Color{30, 30, 42, 255}, SDL_Color{110, 110, 130, 255});
    int text_x = mods_panel.x + 18;
    int text_y = mods_panel.y + 16;
    draw_abs("Active Mods", text_x, text_y, SDL_Color{240, 230, 140, 255});
    char summary_buf[64];
    int enabled = enabled_mod_count();
    int total = static_cast<int>(lobby_const().mods.size());
    std::snprintf(summary_buf, sizeof(summary_buf), "%d of %d enabled", enabled, total);
    draw_abs(summary_buf, text_x, text_y + 26, SDL_Color{190, 200, 210, 255});
    text_y += 56;
    std::vector<std::string> enabled_titles;
    enabled_titles.reserve(static_cast<size_t>(std::max(0, enabled)));
    for (const auto& entry : lobby_const().mods) {
        if (entry.enabled || entry.required)
            enabled_titles.push_back(entry.title);
    }
    if (enabled_titles.empty()) {
        draw_abs("No optional mods enabled.", text_x, text_y, SDL_Color{170, 170, 190, 255});
    } else {
        int max_lines = 4;
        for (int i = 0; i < max_lines && i < (int)enabled_titles.size(); ++i) {
            std::string line = std::string("- ") + enabled_titles[(size_t)i];
            draw_abs(line, text_x, text_y + i * 20, SDL_Color{210, 210, 225, 255});
        }
        if ((int)enabled_titles.size() > max_lines) {
            int remain = (int)enabled_titles.size() - max_lines;
            char more_buf[48];
            std::snprintf(more_buf, sizeof(more_buf), "...and %d more", remain);
            draw_abs(more_buf, text_x, text_y + max_lines * 20, SDL_Color{180, 180, 200, 255});
        }
    }
    draw_button(1409, "Manage Mods", "");

    draw_button(1440, "Start Game", "");
    draw_button(1441, "Back", "");
}

void render_lobby_mods(int width, int height, const std::vector<ButtonDesc>& buttons) {
    SDL_Renderer* r = gg ? gg->renderer : nullptr;
    if (!r || !gg || !gg->ui_font)
        return;
    SDL_SetRenderDrawColor(r, 10, 12, 18, 255);
    SDL_RenderClear(r);
    auto draw_text = [&](const std::string& text, const SDL_Rect& rect, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex) return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{rect.x + 12, rect.y + (rect.h - th)/2, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };
    auto draw_value_right = [&](const SDL_Rect& rect, const std::string& text, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex) return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{rect.x + rect.w - tw - 12, rect.y + (rect.h - th)/2, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };
    auto draw_button = [&](int id, const std::string& label) {
        auto it = std::find_if(buttons.begin(), buttons.end(),
                               [&](const ButtonDesc& b) { return b.id == id; });
        if (it == buttons.end())
            return;
        SDL_Rect rect = to_pixels(it->rect, width, height);
        bool focused = (ss->menu.focus_id == it->id);
        SDL_Color fill = focused ? SDL_Color{45, 55, 75, 255} : SDL_Color{30, 33, 40, 255};
        SDL_Color border = focused ? SDL_Color{200, 220, 90, 255} : SDL_Color{120, 120, 135, 255};
        if (!it->enabled) {
            fill = SDL_Color{35, 38, 45, 255};
            border = SDL_Color{90, 90, 110, 255};
        }
        draw_section(r, rect, fill, border);
        draw_text(label, rect, SDL_Color{220, 220, 230, 255});
    };
    auto draw_abs = [&](const std::string& text, int x, int y, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FreeSurface(surf);
        if (!tex) return;
        int tw=0, th=0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst{x, y, tw, th};
        SDL_RenderCopy(r, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };

    draw_abs("Session Mods", static_cast<int>((MENU_SAFE_MARGIN + 0.01f) * static_cast<float>(width)),
             static_cast<int>((MENU_SAFE_MARGIN + 0.02f) * static_cast<float>(height)),
             SDL_Color{240, 220, 80, 255});
    const auto& mods = lobby_const().mods;
    int total_pages = std::max(1, (static_cast<int>(mods.size()) + kLobbyModsPerPage - 1) / kLobbyModsPerPage);
    char page_buf[32];
    std::snprintf(page_buf, sizeof(page_buf), "Page %d/%d", lobby_const().mod_page + 1, total_pages);
    draw_abs(page_buf, width - 220, static_cast<int>((MENU_SAFE_MARGIN + 0.02f) * static_cast<float>(height)),
             SDL_Color{210, 210, 225, 255});

    draw_button(1430, "Prev Page");
    draw_button(1431, "Next Page");

    auto visible = visible_mod_indices();
    for (int i = 0; i < kLobbyModsPerPage; ++i) {
        SDL_Rect rect = to_pixels(mod_row_rect(i), width, height);
        bool focused = (ss->menu.focus_id == 1420 + i);
        SDL_Color fill = focused ? SDL_Color{60, 70, 90, 255} : SDL_Color{35, 38, 50, 255};
        SDL_Color border = SDL_Color{120, 120, 135, 255};
        if (i >= (int)visible.size()) {
            draw_section(r, rect, fill, border);
            draw_text("Empty Slot", rect, SDL_Color{110, 110, 130, 255});
            continue;
        }
        int global_idx = visible[(size_t)i];
        if (global_idx < 0 || global_idx >= (int)mods.size()) {
            draw_section(r, rect, fill, border);
            continue;
        }
        const auto& mod = mods[(size_t)global_idx];
        border = mod.required ? SDL_Color{170, 120, 60, 255} : SDL_Color{120, 120, 135, 255};
        draw_section(r, rect, fill, border);
        std::string label = mod.title;
        if (!mod.author.empty())
            label += "  •  " + mod.author;
        draw_text(label, rect, SDL_Color{220, 220, 230, 255});
        std::string status = mod.required ? "Core" : (mod.enabled ? "Enabled" : "Disabled");
        SDL_Color status_col = mod.required ? SDL_Color{220, 200, 150, 255}
                                            : (mod.enabled ? SDL_Color{120, 200, 140, 255}
                                                           : SDL_Color{200, 160, 120, 255});
        draw_value_right(rect, status, status_col);
        if (!mod.description.empty()) {
            SDL_Rect desc_rect{rect.x + 12, rect.y + rect.h - 20, rect.w - 24, 16};
            draw_text(mod.description, desc_rect, SDL_Color{180, 180, 200, 255});
        }
        if (!mod.dependencies.empty()) {
            std::string deps = "Requires: ";
            for (size_t d = 0; d < mod.dependencies.size(); ++d) {
                if (d > 0) deps += ", ";
                deps += mod.dependencies[d];
            }
            SDL_Rect deps_rect{rect.x + 12, rect.y + rect.h - 36, rect.w - 24, 16};
            draw_text(deps, deps_rect, SDL_Color{160, 170, 200, 255});
        }
    }

    draw_button(1450, "Back");
}

void lobby_handle_button(const ButtonDesc& b, bool activated, bool play_sound_on_success) {
    if (!ss || !activated)
        return;
    auto& lobby_state = lobby();
    auto play = [&](const char* key) {
        if (play_sound_on_success)
            play_sound(key);
    };
    switch (b.id) {
        case 1400:
            open_text_input(TEXT_INPUT_LOBBY_NAME, kLobbyNameMaxLen, lobby_state.session_name,
                            std::string{}, "Lobby Name");
            play("base:ui_confirm");
            break;
        case 1401:
            lobby_state.privacy = (lobby_state.privacy + 1) % 3;
            play("base:ui_right");
            break;
        case 1402:
            ss->alerts.push_back({"Invites not implemented yet", 0.0f, 1.5f, false});
            play("base:ui_cant");
            break;
        case 1403:
            lobby_state.scenario_index = (lobby_state.scenario_index + 1) % 1;
            play("base:ui_right");
            break;
        case 1404:
            lobby_state.difficulty = (lobby_state.difficulty + 1) % 3;
            play("base:ui_right");
            break;
        case 1405:
            lobby_state.max_players += 1;
            if (lobby_state.max_players > kMaxLobbyPlayers)
                lobby_state.max_players = kMinLobbyPlayers;
            play("base:ui_right");
            break;
        case 1406:
            lobby_state.allow_join = !lobby_state.allow_join;
            play(lobby_state.allow_join ? "base:ui_confirm" : "base:ui_left");
            break;
        case 1407:
            open_text_input(TEXT_INPUT_LOBBY_SEED, kLobbySeedMaxLen, lobby_state.seed,
                            std::string{}, "World Seed");
            play("base:ui_confirm");
            break;
        case 1408:
            randomize_seed();
            play("base:ui_confirm");
            break;
        case 1409:
            enter_lobby_mods_page();
            play("base:ui_confirm");
            break;
        case 1441:
            ss->menu.page = MAIN;
            ss->menu.focus_id = -1;
            play("base:ui_confirm");
            break;
        case 1440: {
            std::string err;
            if (!dependencies_satisfied(err)) {
                ss->alerts.push_back({err, 0.0f, 1.8f, false});
                play("base:ui_cant");
                break;
            }
            if (!lobby_start_session()) {
                play("base:ui_cant");
            }
            break;
        }
        default:
            break;
    }
}

void lobby_mods_handle_button(const ButtonDesc& b, bool activated, bool play_sound_on_success) {
    if (!ss || !activated)
        return;
    auto& lobby_state = lobby();
    auto play = [&](const char* key) {
        if (play_sound_on_success)
            play_sound(key);
    };
    switch (b.id) {
        case 1430:
            if (lobby_state.mod_page > 0) {
                lobby_state.mod_page -= 1;
                if (ss) ss->menu.focus_id = 1420;
                play("base:ui_left");
            } else {
                play("base:ui_cant");
            }
            break;
        case 1431: {
            int total_pages = std::max(1, (static_cast<int>(lobby_state.mods.size()) + kLobbyModsPerPage - 1) / kLobbyModsPerPage);
            if (lobby_state.mod_page + 1 < total_pages) {
                lobby_state.mod_page += 1;
                if (ss) ss->menu.focus_id = 1420;
                play("base:ui_right");
            } else {
                play("base:ui_cant");
            }
            break;
        }
        case 1450:
            ss->menu.page = LOBBY;
            ss->menu.focus_id = -1;
            play("base:ui_confirm");
            break;
        default:
            if (b.id >= 1420 && b.id < 1420 + kLobbyModsPerPage) {
                auto visible = visible_mod_indices();
                int idx = b.id - 1420;
                if (idx >= 0 && idx < (int)visible.size()) {
                    auto& entry = lobby_state.mods[(size_t)visible[(size_t)idx]];
                    if (!entry.required) {
                        entry.enabled = !entry.enabled;
                        play(entry.enabled ? "base:ui_confirm" : "base:ui_left");
                    } else {
                        play("base:ui_cant");
                    }
                } else {
                    play("base:ui_cant");
                }
            }
            break;
    }
}

bool lobby_handle_nav_direction(int fid, int delta) {
    auto& lobby_state = lobby();
    if (fid == 1401) {
        lobby_state.privacy = (lobby_state.privacy + delta + 3) % 3;
        play_sound(delta > 0 ? "base:ui_right" : "base:ui_left");
        return true;
    }
    if (fid == 1404) {
        lobby_state.difficulty = (lobby_state.difficulty + delta + 3) % 3;
        play_sound(delta > 0 ? "base:ui_right" : "base:ui_left");
        return true;
    }
    if (fid == 1405) {
        lobby_state.max_players += delta;
        if (lobby_state.max_players < kMinLobbyPlayers)
            lobby_state.max_players = kMaxLobbyPlayers;
        else if (lobby_state.max_players > kMaxLobbyPlayers)
            lobby_state.max_players = kMinLobbyPlayers;
        play_sound(delta > 0 ? "base:ui_right" : "base:ui_left");
        return true;
    }
    return false;
}

bool lobby_mods_handle_nav_direction(int fid, int delta) {
    auto& lobby_state = lobby();
    auto change_page = [&](int dir) -> bool {
        int total_pages = std::max(1, (static_cast<int>(lobby_state.mods.size()) + kLobbyModsPerPage - 1) / kLobbyModsPerPage);
        int next = lobby_state.mod_page + dir;
        if (next < 0 || next >= total_pages)
            return false;
        lobby_state.mod_page = next;
        play_sound(dir > 0 ? "base:ui_right" : "base:ui_left");
        if (ss)
            ss->menu.focus_id = 1420;
        return true;
    };
    if (fid >= 1420 && fid < 1420 + kLobbyModsPerPage) {
        if (delta < 0)
            return change_page(-1);
        if (delta > 0)
            return change_page(1);
    }
    return false;
}

bool lobby_start_session() {
    auto ids = enabled_mod_ids();
    set_demo_item_mod_filter(ids);
    if (!load_demo_item_defs()) {
        ss->alerts.push_back({"Failed to load mods", 0.0f, 1.8f, false});
        return false;
    }
    ss->mode = modes::PLAYING;
    return true;
}
