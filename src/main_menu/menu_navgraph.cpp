#include "main_menu/menu_navgraph.hpp"

#include "globals.hpp"
#include "main_menu/menu_internal.hpp"
#include "state.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

constexpr const char* kNavFilePath = "config/menu_nav.ini";
constexpr const float kStatusDuration = 2.0f;

enum class NavDir : int { Up = 0, Down = 1, Left = 2, Right = 3 };

struct NavRegistry {
    bool loaded{false};
    std::unordered_map<std::string, std::unordered_map<int, MenuNavEdges>> sections;
};

NavRegistry& registry() {
    static NavRegistry reg;
    return reg;
}

int dir_index(NavDir d) {
    return static_cast<int>(d);
}

MenuNavEdges make_empty_edges() {
    MenuNavEdges edges;
    edges.targets.fill(-1);
    edges.set.fill(false);
    return edges;
}

void ensure_registry_loaded() {
    auto& reg = registry();
    if (reg.loaded)
        return;
    std::ifstream f(kNavFilePath);
    if (!f.is_open()) {
        reg.loaded = true;
        return;
    }
    std::string line;
    std::string section;
    while (std::getline(f, line)) {
        auto comment = line.find('#');
        if (comment != std::string::npos)
            line = line.substr(0, comment);
        auto trim = [](const std::string& s) {
            size_t b = 0, e = s.size();
            while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
            while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
            return s.substr(b, e - b);
        };
        line = trim(line);
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        if (section.empty())
            continue;
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key.empty() || value.empty())
            continue;
        int id = std::atoi(key.c_str());
        if (id == 0 && key != "0")
            continue;
        MenuNavEdges edges = make_empty_edges();
        std::stringstream ss(value);
        std::string token;
        while (std::getline(ss, token, ',')) {
            token = trim(token);
            if (token.size() < 3)
                continue;
            char dir = static_cast<char>(std::toupper(token[0]));
            auto colon = token.find(':');
            if (colon == std::string::npos)
                continue;
            std::string val = trim(token.substr(colon + 1));
            if (val.empty())
                continue;
            int target = std::atoi(val.c_str());
            if (target == 0 && val != "0")
                continue;
            int idx = -1;
            switch (dir) {
                case 'U': idx = 0; break;
                case 'D': idx = 1; break;
                case 'L': idx = 2; break;
                case 'R': idx = 3; break;
                default: break;
            }
            if (idx >= 0) {
                std::size_t sidx = static_cast<std::size_t>(idx);
                edges.targets[sidx] = target;
                edges.set[sidx] = true;
            }
        }
        reg.sections[section][id] = edges;
    }
    reg.loaded = true;
}

void save_registry() {
    auto& reg = registry();
    std::filesystem::path path = kNavFilePath;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return;
    std::vector<std::string> sections;
    sections.reserve(reg.sections.size());
    for (const auto& kv : reg.sections)
        sections.push_back(kv.first);
    std::sort(sections.begin(), sections.end());
    for (const auto& sec : sections) {
        out << '[' << sec << "]\n";
        const auto& entries = reg.sections[sec];
        std::vector<int> ids;
        ids.reserve(entries.size());
        for (const auto& kv : entries)
            ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (int id : ids) {
            const MenuNavEdges& edges = entries.at(id);
            bool any = false;
            std::ostringstream value;
            static const char* labels[4] = {"U", "D", "L", "R"};
            for (int i = 0; i < 4; ++i) {
                std::size_t idx = static_cast<std::size_t>(i);
                if (!edges.set[idx])
                    continue;
                if (any)
                    value << ", ";
                value << labels[idx] << ':' << edges.targets[idx];
                any = true;
            }
            if (any)
                out << id << '=' << value.str() << "\n";
        }
        out << "\n";
    }
}

std::string section_for_page(const std::string& page) {
    unsigned w = gg ? gg->window_dims.x : 1280u;
    unsigned h = gg ? gg->window_dims.y : 720u;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %ux%u", page.c_str(), w, h);
    return std::string(buf);
}

void persist_nav_if_dirty() {
    if (!ss || !ss->menu.nav_edit.dirty)
        return;
    ensure_registry_loaded();
    auto& cache = ss->menu.nav_cache;
    if (!cache.section.empty())
        registry().sections[cache.section] = cache.edges;
    save_registry();
    ss->menu.nav_edit.dirty = false;
}

MenuNavEdges* find_edges_mut(int id) {
    if (!ss)
        return nullptr;
    auto& cache = ss->menu.nav_cache;
    auto it = cache.edges.find(id);
    if (it == cache.edges.end()) {
        auto inserted = cache.edges.emplace(id, make_empty_edges());
        ss->menu.nav_edit.dirty = true;
        return &inserted.first->second;
    }
    return &it->second;
}

std::unordered_map<int, SDL_Rect> build_button_rects(const std::vector<ButtonDesc>& buttons, int width, int height) {
    std::unordered_map<int, SDL_Rect> rects;
    rects.reserve(buttons.size());
    for (const auto& b : buttons) {
        SDL_FRect fr = ndc_to_pixels(b.rect, width, height);
        SDL_Rect r{static_cast<int>(std::round(fr.x)), static_cast<int>(std::round(fr.y)),
                   static_cast<int>(std::round(fr.w)), static_cast<int>(std::round(fr.h))};
        rects[b.id] = r;
    }
    return rects;
}

SDL_FPoint rect_center(const SDL_Rect& r) {
    return SDL_FPoint{static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f,
                      static_cast<float>(r.y) + static_cast<float>(r.h) * 0.5f};
}

void set_status(const std::string& msg) {
    if (!ss)
        return;
    ss->menu.nav_edit.status = msg;
    ss->menu.nav_edit.status_timer = kStatusDuration;
}

void set_edge_internal(int source, int dir_idx, int target) {
    if (!ss)
        return;
    auto& cache = ss->menu.nav_cache;
    if (target < 0) {
        auto it = cache.edges.find(source);
        if (it != cache.edges.end()) {
            std::size_t didx = static_cast<std::size_t>(dir_idx);
            it->second.set[didx] = false;
            it->second.targets[didx] = -1;
            bool any = false;
            for (int i = 0; i < 4; ++i) {
                if (it->second.set[static_cast<std::size_t>(i)]) { any = true; break; }
            }
            if (!any)
                cache.edges.erase(it);
        }
    } else {
        MenuNavEdges* edges = find_edges_mut(source);
        if (!edges)
            return;
        std::size_t didx = static_cast<std::size_t>(dir_idx);
        edges->set[didx] = true;
        edges->targets[didx] = target;
    }
    ss->menu.nav_edit.dirty = true;
}

bool is_mod_down(SDL_Keymod mods) {
    return (mods & (KMOD_LALT | KMOD_RALT)) != 0;
}

bool handle_direction_press() {
    if (!ss)
        return false;
    auto& edit = ss->menu.nav_edit;
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    SDL_Keymod mods = SDL_GetModState();
    if (!is_mod_down(mods)) {
        edit.dir_held.fill(false);
        return false;
    }
    static const SDL_Scancode kScancodes[4] = {
        SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D
    };
    static const SDL_Scancode kArrowCodes[4] = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT
    };
    for (int i = 0; i < 4; ++i) {
        bool down = keys[kScancodes[i]] || keys[kArrowCodes[i]];
        bool prev = edit.dir_held[static_cast<std::size_t>(i)];
        edit.dir_held[static_cast<std::size_t>(i)] = down;
        if (down && !prev) {
            if (edit.selected_id < 0) {
                set_status("Select a button first.");
                edit.pending_dir = -1;
            } else {
                edit.pending_dir = i;
                set_status("Click a target (or empty space to clear).");
            }
            return true;
        }
    }
    return false;
}

bool delete_pressed() {
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    return keys[SDL_SCANCODE_DELETE] || keys[SDL_SCANCODE_BACKSPACE];
}

void update_status_timer(float dt) {
    if (!ss)
        return;
    if (ss->menu.nav_edit.status_timer > 0.0f) {
        ss->menu.nav_edit.status_timer = std::max(0.0f, ss->menu.nav_edit.status_timer - dt);
        if (ss->menu.nav_edit.status_timer == 0.0f)
            ss->menu.nav_edit.status.clear();
    }
}

SDL_Rect reset_button_rect(int width, int height) {
    int top = static_cast<int>(MENU_SAFE_MARGIN * static_cast<float>(height)) + 10;
    return SDL_Rect{width - 210, top, 190, 32};
}

} // namespace

void navgraph_set_page(const std::string& page_key) {
    if (!ss)
        return;
    ensure_registry_loaded();
    auto& cache = ss->menu.nav_cache;
    cache.page = page_key;
    cache.section = section_for_page(page_key);
    auto& reg = registry();
    auto it = reg.sections.find(cache.section);
    if (it != reg.sections.end())
        cache.edges = it->second;
    else
        cache.edges.clear();
}

NavNode navgraph_apply(int button_id, const NavNode& fallback) {
    if (!ss)
        return fallback;
    auto it = ss->menu.nav_cache.edges.find(button_id);
    if (it == ss->menu.nav_cache.edges.end())
        return fallback;
    NavNode node = fallback;
    const MenuNavEdges& edges = it->second;
    std::size_t up_idx = static_cast<std::size_t>(dir_index(NavDir::Up));
    std::size_t down_idx = static_cast<std::size_t>(dir_index(NavDir::Down));
    std::size_t left_idx = static_cast<std::size_t>(dir_index(NavDir::Left));
    std::size_t right_idx = static_cast<std::size_t>(dir_index(NavDir::Right));
    if (edges.set[up_idx])    node.up = edges.targets[up_idx];
    if (edges.set[down_idx])  node.down = edges.targets[down_idx];
    if (edges.set[left_idx])  node.left = edges.targets[left_idx];
    if (edges.set[right_idx]) node.right = edges.targets[right_idx];
    return node;
}

void navgraph_toggle_edit_mode() {
    if (!ss)
        return;
    auto& edit = ss->menu.nav_edit;
    edit.enabled = !edit.enabled;
    edit.pending_dir = -1;
    edit.selected_id = -1;
    edit.dir_held.fill(false);
    set_status(edit.enabled ? "Select a button (Alt+WASD to choose direction)." : "");
    if (!edit.enabled)
        persist_nav_if_dirty();
}

bool navgraph_edit_enabled() {
    return ss && ss->menu.nav_edit.enabled;
}

void navgraph_handle_editor(const std::vector<ButtonDesc>& buttons, int width, int height) {
    if (!ss || !navgraph_edit_enabled())
        return;
    auto& edit = ss->menu.nav_edit;
    update_status_timer(ss->dt);
    auto rects = build_button_rects(buttons, width, height);

    handle_direction_press();

    bool click = ss->mouse_inputs.left && !ss->menu.mouse_left_prev;
    bool release = !ss->mouse_inputs.left && ss->menu.mouse_left_prev;
    auto find_button_at = [&](int x, int y) -> int {
        for (const auto& kv : rects) {
            const SDL_Rect& r = kv.second;
            if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h)
                return kv.first;
        }
        return -1;
    };
    int mx = ss->mouse_inputs.pos.x;
    int my = ss->mouse_inputs.pos.y;
    SDL_Rect reset_rect = reset_button_rect(width, height);
    if (click && mx >= reset_rect.x && mx <= reset_rect.x + reset_rect.w &&
        my >= reset_rect.y && my <= reset_rect.y + reset_rect.h) {
        navgraph_reset_current();
        set_status("Navigation graph reset.");
        return;
    }
    if (click) {
        int hit = find_button_at(mx, my);
        if (edit.pending_dir >= 0) {
            if (edit.selected_id < 0) {
                set_status("Select a button first.");
            } else if (hit >= 0) {
                set_edge_internal(edit.selected_id, edit.pending_dir, hit);
                set_status("Direction mapped.");
            } else {
                set_edge_internal(edit.selected_id, edit.pending_dir, -1);
                set_status("Direction cleared.");
            }
            edit.pending_dir = -1;
        } else if (hit >= 0) {
            edit.selected_id = hit;
            set_status("Selected button " + std::to_string(hit));
        } else {
            edit.selected_id = -1;
            set_status("No button selected.");
        }
    }
    if (edit.pending_dir >= 0 && delete_pressed() && !ss->mouse_inputs.left) {
        set_edge_internal(edit.selected_id, edit.pending_dir, -1);
        set_status("Direction cleared.");
        edit.pending_dir = -1;
    }
    // consume release to avoid standard menu logic
    (void)release;
}

void navgraph_render_overlay(SDL_Renderer* r, const std::vector<ButtonDesc>& buttons, int width, int height) {
    if (!navgraph_edit_enabled() || !ss || !r)
        return;
    auto rects = build_button_rects(buttons, width, height);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    auto draw_text = [&](const std::string& text, int x, int y, SDL_Color c){
        if (!gg || !gg->ui_font || text.empty()) return;
        if (SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), c)) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
            SDL_FreeSurface(surf);
            if (tex) {
                int tw=0, th=0;
                SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                SDL_Rect dst{x, y, tw, th};
                SDL_RenderCopy(r, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
        }
    };
    // Highlight buttons and draw arrows
    auto center_of = [&](int id) -> SDL_FPoint {
        auto it = rects.find(id);
        if (it == rects.end())
            return SDL_FPoint{0,0};
        return rect_center(it->second);
    };
    SDL_Color highlight{250, 220, 120, 255};
    if (ss->menu.nav_edit.selected_id >= 0) {
        auto it = rects.find(ss->menu.nav_edit.selected_id);
        if (it != rects.end()) {
            SDL_SetRenderDrawColor(r, highlight.r, highlight.g, highlight.b, highlight.a);
            SDL_RenderDrawRect(r, &it->second);
        }
    }
    SDL_SetRenderDrawColor(r, 80, 110, 200, 220);
    for (const auto& entry : ss->menu.nav_cache.edges) {
        int source = entry.first;
        const MenuNavEdges& edges = entry.second;
        SDL_FPoint src = center_of(source);
        for (int d = 0; d < 4; ++d) {
            std::size_t idx = static_cast<std::size_t>(d);
            if (!edges.set[idx])
                continue;
            int target = edges.targets[idx];
            auto dest_it = rects.find(target);
            if (dest_it == rects.end())
                continue;
            SDL_FPoint dst = rect_center(dest_it->second);
            SDL_RenderDrawLine(r, static_cast<int>(src.x), static_cast<int>(src.y),
                                  static_cast<int>(dst.x), static_cast<int>(dst.y));
            float angle = std::atan2(dst.y - src.y, dst.x - src.x);
            float arrow_len = 10.0f;
            SDL_FPoint arrow1{dst.x - arrow_len * std::cos(angle - 0.4f),
                              dst.y - arrow_len * std::sin(angle - 0.4f)};
            SDL_FPoint arrow2{dst.x - arrow_len * std::cos(angle + 0.4f),
                              dst.y - arrow_len * std::sin(angle + 0.4f)};
            SDL_RenderDrawLine(r, static_cast<int>(dst.x), static_cast<int>(dst.y),
                                  static_cast<int>(arrow1.x), static_cast<int>(arrow1.y));
            SDL_RenderDrawLine(r, static_cast<int>(dst.x), static_cast<int>(dst.y),
                                  static_cast<int>(arrow2.x), static_cast<int>(arrow2.y));
            SDL_FPoint mid{(src.x + dst.x) * 0.5f, (src.y + dst.y) * 0.5f};
            static const char* labels[4] = {"Up", "Down", "Left", "Right"};
            draw_text(labels[d], static_cast<int>(mid.x) + 6, static_cast<int>(mid.y) + 6,
                      SDL_Color{230, 230, 240, 255});
        }
    }
    SDL_Rect reset_rect = reset_button_rect(width, height);
    SDL_SetRenderDrawColor(r, 40, 40, 60, 230);
    SDL_RenderFillRect(r, &reset_rect);
    SDL_SetRenderDrawColor(r, 200, 200, 220, 255);
    SDL_RenderDrawRect(r, &reset_rect);
    draw_text("Reset Navigation", reset_rect.x + 12, reset_rect.y + 6, SDL_Color{220, 220, 230, 255});
    std::string status = ss->menu.nav_edit.status;
    if (!status.empty())
        draw_text(status, 20, reset_rect.y + reset_rect.h + 10, SDL_Color{240, 220, 80, 255});
    draw_text(ss->menu.nav_cache.section, 20,
              static_cast<int>(MENU_SAFE_MARGIN * static_cast<float>(height)) + 10,
              SDL_Color{200, 200, 210, 255});
    draw_text("Ctrl+N to exit  •  Alt+WASD to bind", 20,
              reset_rect.y - 28, SDL_Color{190, 190, 205, 255});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void navgraph_reset_current() {
    if (!ss)
        return;
    ensure_registry_loaded();
    auto& cache = ss->menu.nav_cache;
    if (!cache.section.empty())
        registry().sections.erase(cache.section);
    cache.edges.clear();
    ss->menu.nav_edit.dirty = true;
    persist_nav_if_dirty();
}
