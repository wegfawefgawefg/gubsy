#include "main_menu/menu_internal.hpp"

#include "demo_items.hpp"
#include "engine/audio.hpp"
#include "globals.hpp"
#include "mods.hpp"
#include "state.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kLayoutFilePath = "config/menu_layouts.ini";
constexpr RectNdc kModsPanelFallback{0.52f, 0.22f, 0.40f, 0.46f};

struct LayoutRegistry {
    bool loaded{false};
    std::unordered_map<std::string, std::unordered_map<std::string, RectNdc>> sections;
};

LayoutRegistry& layout_registry() {
    static LayoutRegistry reg;
    return reg;
}

std::string trim_copy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])))
        ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(start, end - start);
}

bool parse_rect_values(const std::string& text, RectNdc& out) {
    std::array<float, 4> vals{};
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        size_t comma = text.find(',', start);
        std::string token = (comma == std::string::npos) ? text.substr(start) : text.substr(start, comma - start);
        token = trim_copy(token);
        if (token.empty())
            return false;
        try {
            vals[static_cast<size_t>(i)] = std::stof(token);
        } catch (...) {
            return false;
        }
        if (comma == std::string::npos) {
            start = std::string::npos;
            if (i != 3)
                return false;
        } else {
            start = comma + 1;
        }
    }
    out = RectNdc{vals[0], vals[1], vals[2], vals[3]};
    return true;
}

void ensure_layout_registry_loaded() {
    auto& reg = layout_registry();
    if (reg.loaded)
        return;
    std::ifstream f(kLayoutFilePath);
    if (!f.is_open()) {
        reg.loaded = true;
        return;
    }
    std::string line;
    std::string current_section;
    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        line = trim_copy(line);
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']') {
            current_section = trim_copy(line.substr(1, line.size() - 2));
            continue;
        }
        if (current_section.empty())
            continue;
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim_copy(line.substr(0, eq));
        std::string value = trim_copy(line.substr(eq + 1));
        RectNdc rect;
        if (parse_rect_values(value, rect))
            reg.sections[current_section][key] = rect;
    }
    reg.loaded = true;
}

void save_layout_registry() {
    auto& reg = layout_registry();
    std::filesystem::path path = kLayoutFilePath;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return;
    std::vector<std::string> sections;
    sections.reserve(reg.sections.size());
    for (auto const& kv : reg.sections)
        sections.push_back(kv.first);
    std::sort(sections.begin(), sections.end());
    for (const auto& section : sections) {
        out << '[' << section << "]\n";
        const auto& rects = reg.sections[section];
        std::vector<std::string> keys;
        keys.reserve(rects.size());
        for (auto const& kv : rects)
            keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            const RectNdc& r = rects.at(key);
            out << key << '=' << r.x << ',' << r.y << ',' << r.w << ',' << r.h << "\n";
        }
        out << "\n";
    }
}

std::string lobby_layout_section() {
    unsigned w = gg ? gg->window_dims.x : 1280u;
    unsigned h = gg ? gg->window_dims.y : 720u;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "lobby %ux%u", w, h);
    return std::string(buf);
}

void ensure_lobby_layout_loaded() {
    if (!ss)
        return;
    ensure_layout_registry_loaded();
    auto& reg = layout_registry();
    std::string section = lobby_layout_section();
    ss->menu.lobby_layout.section = section;
    auto it = reg.sections.find(section);
    if (it != reg.sections.end())
        ss->menu.lobby_layout.rects = it->second;
    else
        ss->menu.lobby_layout.rects.clear();
}

RectNdc apply_layout_rect(const char* id, const RectNdc& fallback) {
    if (!ss || !id)
        return fallback;
    auto it = ss->menu.lobby_layout.rects.find(id);
    if (it != ss->menu.lobby_layout.rects.end())
        return it->second;
    return fallback;
}

void persist_lobby_layouts_if_dirty() {
    if (!ss || !ss->menu.layout_edit.dirty)
        return;
    ensure_layout_registry_loaded();
    auto& reg = layout_registry();
    reg.sections[ss->menu.lobby_layout.section] = ss->menu.lobby_layout.rects;
    save_layout_registry();
    ss->menu.layout_edit.dirty = false;
}

void set_lobby_layout_rect(const std::string& id, const RectNdc& rect) {
    if (!ss)
        return;
    ss->menu.lobby_layout.rects[id] = rect;
    ss->menu.layout_edit.dirty = true;
}

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

RectNdc pixels_to_ndc(float px, float py, float pw, float ph, int width, int height) {
    float inv_w = width > 0 ? 1.0f / static_cast<float>(width) : 0.0f;
    float inv_h = height > 0 ? 1.0f / static_cast<float>(height) : 0.0f;
    RectNdc out;
    out.x = (px * inv_w) - MENU_SAFE_MARGIN;
    out.y = (py * inv_h) - MENU_SAFE_MARGIN;
    out.w = pw * inv_w;
    out.h = ph * inv_h;
    auto clamp01 = [](float v) { return std::clamp(v, 0.0f, 1.0f); };
    out.x = clamp01(out.x);
    out.y = clamp01(out.y);
    out.w = clamp01(out.w);
    out.h = clamp01(out.h);
    if (out.x + out.w > 1.0f)
        out.w = std::max(0.01f, 1.0f - out.x);
    if (out.y + out.h > 1.0f)
        out.h = std::max(0.01f, 1.0f - out.y);
    return out;
}

struct LayoutItem {
    std::string id;
    SDL_Rect rect;
};

const char* layout_id_for_button(int id) {
    switch (id) {
        case 1400: return "lobby_name";
        case 1401: return "privacy";
        case 1402: return "invite_friends";
        case 1403: return "scenario";
        case 1404: return "difficulty";
        case 1405: return "max_players";
        case 1406: return "allow_join";
        case 1407: return "seed";
        case 1408: return "randomize_seed";
        case 1409: return "manage_mods";
        case 1440: return "start_game";
        case 1441: return "back";
        default: return nullptr;
    }
}

std::vector<LayoutItem> gather_layout_items(const std::vector<ButtonDesc>& buttons, int width, int height) {
    std::vector<LayoutItem> items;
    for (const auto& b : buttons) {
        const char* layout_id = layout_id_for_button(b.id);
        if (!layout_id)
            continue;
        SDL_Rect rect = to_pixels(b.rect, width, height);
        items.push_back({layout_id, rect});
    }
    LayoutItem panel{"mods_panel", to_pixels(apply_layout_rect("mods_panel", kModsPanelFallback), width, height)};
    items.push_back(panel);
    return items;
}

void handle_lobby_layout_edit(const std::vector<ButtonDesc>& buttons, int width, int height) {
    if (!ss)
        return;
    auto items = gather_layout_items(buttons, width, height);
    auto& edit = ss->menu.layout_edit;
    bool mouse_down = ss->mouse_inputs.left;
    bool pressed = ss->mouse_inputs.left && !ss->menu.mouse_left_prev;
    bool released = !ss->mouse_inputs.left && ss->menu.mouse_left_prev;
    int mx = ss->mouse_inputs.pos.x;
    int my = ss->mouse_inputs.pos.y;
    auto point_in = [&](const SDL_Rect& r) {
        return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
    };
    const int handle = 12;
    if (pressed) {
        for (const auto& item : items) {
            SDL_Rect move{item.rect.x - handle, item.rect.y - handle, handle, handle};
            SDL_Rect resize{item.rect.x + item.rect.w - handle, item.rect.y + item.rect.h - handle, handle, handle};
            if (point_in(move)) {
                edit.dragging = true;
                edit.resizing = false;
                edit.active_id = item.id;
                edit.grab_offset = glm::vec2(static_cast<float>(mx - item.rect.x),
                                             static_cast<float>(my - item.rect.y));
                break;
            }
            if (point_in(resize)) {
                edit.dragging = true;
                edit.resizing = true;
                edit.active_id = item.id;
                edit.grab_offset = glm::vec2(static_cast<float>(mx - (item.rect.x + item.rect.w)),
                                             static_cast<float>(my - (item.rect.y + item.rect.h)));
                break;
            }
        }
    }
    if (edit.dragging && !edit.active_id.empty()) {
        auto it = std::find_if(items.begin(), items.end(),
                               [&](const LayoutItem& item) { return item.id == edit.active_id; });
        if (it != items.end()) {
            SDL_Rect current = it->rect;
            if (!edit.resizing) {
                float new_x = static_cast<float>(mx) - edit.grab_offset.x;
                float new_y = static_cast<float>(my) - edit.grab_offset.y;
                RectNdc rect = pixels_to_ndc(new_x, new_y, static_cast<float>(current.w),
                                             static_cast<float>(current.h), width, height);
                set_lobby_layout_rect(edit.active_id, rect);
            } else {
                float base_x = static_cast<float>(current.x);
                float base_y = static_cast<float>(current.y);
                float new_br_x = static_cast<float>(mx) - edit.grab_offset.x;
                float new_br_y = static_cast<float>(my) - edit.grab_offset.y;
                float new_w = std::max(8.0f, new_br_x - base_x);
                float new_h = std::max(8.0f, new_br_y - base_y);
                RectNdc rect = pixels_to_ndc(base_x, base_y, new_w, new_h, width, height);
                set_lobby_layout_rect(edit.active_id, rect);
            }
        }
    }
    if (released && edit.dragging) {
        edit.dragging = false;
        edit.resizing = false;
        edit.active_id.clear();
        persist_lobby_layouts_if_dirty();
    }
    if (!mouse_down && !edit.dragging) {
        edit.grab_offset = glm::vec2(0.0f, 0.0f);
    }
}

void render_lobby_layout_overlay(SDL_Renderer* r, const std::vector<ButtonDesc>& buttons, int width, int height) {
    if (!ss || !r)
        return;
    auto items = gather_layout_items(buttons, width, height);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const int handle = 12;
    for (const auto& item : items) {
        SDL_Color border = (ss->menu.layout_edit.active_id == item.id && ss->menu.layout_edit.dragging)
                               ? SDL_Color{250, 210, 120, 255}
                               : SDL_Color{150, 150, 200, 255};
        SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
        SDL_RenderDrawRect(r, &item.rect);
        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(r, &item.rect);
        SDL_Rect move{item.rect.x - handle, item.rect.y - handle, handle, handle};
        SDL_Rect resize{item.rect.x + item.rect.w - handle, item.rect.y + item.rect.h - handle, handle, handle};
        SDL_SetRenderDrawColor(r, 40, 40, 50, 200);
        SDL_RenderFillRect(r, &move);
        SDL_RenderFillRect(r, &resize);
        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(r, &move);
        SDL_RenderDrawRect(r, &resize);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    if (gg && gg->ui_font) {
        const char* msg = "Layout edit mode - drag handles, Ctrl+L to exit";
        SDL_Color c{240, 220, 80, 255};
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, msg, c)) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            SDL_FreeSurface(surf);
            if (tex) {
                int tw = 0, th = 0;
                SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                SDL_Rect dst{20, 20, tw, th};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
        }
    }
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
    ensure_lobby_layout_loaded();
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
    buttons.push_back(ButtonDesc{1400, apply_layout_rect("lobby_name", row_rect(0)), "Lobby Name", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1401, apply_layout_rect("privacy", row_rect(1)), "Privacy", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1402, apply_layout_rect("invite_friends", row_rect(2)), "Invite Friends", ButtonKind::Button, 0.0f, false});
    buttons.push_back(ButtonDesc{1403, apply_layout_rect("scenario", row_rect(3)), "Scenario", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1404, apply_layout_rect("difficulty", row_rect(4)), "Difficulty", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1405, apply_layout_rect("max_players", row_rect(5)), "Max Players", ButtonKind::OptionCycle, 0.0f, true});
    buttons.push_back(ButtonDesc{1406, apply_layout_rect("allow_join", row_rect(6)), "Allow Join", ButtonKind::Toggle, lobby_const().allow_join ? 1.0f : 0.0f, true});
    buttons.push_back(ButtonDesc{1407, apply_layout_rect("seed", row_rect(7)), "Seed", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1408, apply_layout_rect("randomize_seed", RectNdc{0.06f, 0.72f, 0.20f, 0.065f}), "Randomize Seed", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1409, apply_layout_rect("manage_mods", RectNdc{0.52f, 0.70f, 0.36f, 0.07f}), "Manage Mods", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1440, apply_layout_rect("start_game", RectNdc{0.56f, 0.85f, 0.32f, 0.08f}), "Start Game", ButtonKind::Button, 0.0f, true});
    buttons.push_back(ButtonDesc{1441, apply_layout_rect("back", RectNdc{0.06f, 0.82f, 0.22f, 0.07f}), "Back", ButtonKind::Button, 0.0f, true});
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

    SDL_Rect mods_panel = to_pixels(apply_layout_rect("mods_panel", kModsPanelFallback), width, height);
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
    if (ss->menu.layout_edit.enabled)
        render_lobby_layout_overlay(r, buttons, width, height);
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

bool lobby_layout_edit_enabled() {
    return ss && ss->menu.layout_edit.enabled;
}

void toggle_lobby_layout_edit_mode() {
    if (!ss)
        return;
    auto& edit = ss->menu.layout_edit;
    edit.enabled = !edit.enabled;
    edit.dragging = false;
    edit.resizing = false;
    edit.active_id.clear();
    if (!edit.enabled)
        persist_lobby_layouts_if_dirty();
}

void lobby_layout_editor_handle(const std::vector<ButtonDesc>& buttons, int width, int height) {
    handle_lobby_layout_edit(buttons, width, height);
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
