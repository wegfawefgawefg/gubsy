#include "main_menu/menu_layout.hpp"

#include "globals.hpp"
#include "main_menu/menu_internal.hpp"
#include "state.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <SDL2/SDL_ttf.h>

namespace {

constexpr const char* kLayoutFilePath = "config/menu_layouts.ini";
constexpr float kMinRectSize = 0.02f;

struct LayoutRegistry {
    bool loaded{false};
    std::unordered_map<std::string, std::unordered_map<std::string, RectNdc>> sections;
};

LayoutRegistry& registry() {
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

bool parse_rect(const std::string& text, RectNdc& out) {
    std::array<float, 4> vals{};
    size_t pos = 0;
    for (int i = 0; i < 4; ++i) {
        size_t comma = text.find(',', pos);
        std::string token = (comma == std::string::npos) ? text.substr(pos) : text.substr(pos, comma - pos);
        token = trim_copy(token);
        if (token.empty()) return false;
        try {
            vals[static_cast<size_t>(i)] = std::stof(token);
        } catch (...) {
            return false;
        }
        if (comma == std::string::npos) {
            if (i != 3) return false;
        } else {
            pos = comma + 1;
        }
    }
    out = RectNdc{vals[0], vals[1], vals[2], vals[3]};
    return true;
}

void ensure_registry_loaded() {
    auto& reg = registry();
    if (reg.loaded)
        return;
    std::ifstream f(kLayoutFilePath);
    if (!f.is_open()) {
        reg.loaded = true;
        return;
    }
    std::string line;
    std::string section;
    while (std::getline(f, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);
        line = trim_copy(line);
        if (line.empty())
            continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim_copy(line.substr(1, line.size() - 2));
            continue;
        }
        if (section.empty())
            continue;
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim_copy(line.substr(0, eq));
        std::string value = trim_copy(line.substr(eq + 1));
        RectNdc rect;
        if (parse_rect(value, rect))
            reg.sections[section][key] = rect;
    }
    reg.loaded = true;
}

void save_registry() {
    auto& reg = registry();
    std::filesystem::path path = kLayoutFilePath;
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
        const auto& rects = reg.sections[sec];
        std::vector<std::string> keys;
        keys.reserve(rects.size());
        for (const auto& kv : rects)
            keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            const RectNdc& r = rects.at(key);
            out << key << '=' << r.x << ',' << r.y << ',' << r.w << ',' << r.h << "\n";
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

RectNdc pixels_to_ndc(float px, float py, float pw, float ph, int width, int height) {
    float inv_w = width > 0 ? 1.0f / static_cast<float>(width) : 0.0f;
    float inv_h = height > 0 ? 1.0f / static_cast<float>(height) : 0.0f;
    RectNdc out;
    out.x = px * inv_w - MENU_SAFE_MARGIN;
    out.y = py * inv_h - MENU_SAFE_MARGIN;
    out.w = pw * inv_w;
    out.h = ph * inv_h;
    float min_pos = -MENU_SAFE_MARGIN;
    out.x = std::clamp(out.x, min_pos, 1.0f);
    out.y = std::clamp(out.y, min_pos, 1.0f);
    out.w = std::max(kMinRectSize, std::min(1.0f + MENU_SAFE_MARGIN - out.x, out.w));
    out.h = std::max(kMinRectSize, std::min(1.0f + MENU_SAFE_MARGIN - out.y, out.h));
    return out;
}

SDL_Rect reset_button_rect(int width, int height) {
    int top = static_cast<int>(MENU_SAFE_MARGIN * static_cast<float>(height)) + 10;
    return SDL_Rect{width - 190, top, 170, 30};
}

SDL_Rect clamp_handle_rect(SDL_Rect rect, int width, int height) {
    rect.x = std::clamp(rect.x, 0, std::max(0, width - rect.w));
    rect.y = std::clamp(rect.y, 0, std::max(0, height - rect.h));
    return rect;
}

void persist_layout_if_dirty() {
    if (!ss || !ss->menu.layout_edit.dirty)
        return;
    ensure_registry_loaded();
    auto& reg = registry();
    const auto& cache = ss->menu.layout_cache;
    if (!cache.section.empty())
        reg.sections[cache.section] = cache.rects;
    save_registry();
    ss->menu.layout_edit.dirty = false;
}

} // namespace

void layout_set_page(const std::string& page_key) {
    if (!ss)
        return;
    ensure_registry_loaded();
    auto& cache = ss->menu.layout_cache;
    cache.page = page_key;
    cache.section = section_for_page(page_key);
    auto& reg = registry();
    auto it = reg.sections.find(cache.section);
    if (it != reg.sections.end())
        cache.rects = it->second;
    else
        cache.rects.clear();
}

RectNdc layout_rect(const std::string& id, const RectNdc& fallback) {
    if (!ss)
        return fallback;
    auto& cache = ss->menu.layout_cache;
    if (cache.rects.empty())
        return fallback;
    auto it = cache.rects.find(id);
    return (it != cache.rects.end()) ? it->second : fallback;
}

static void layout_set_rect_internal(const std::string& id, const RectNdc& rect) {
    if (!ss)
        return;
    ss->menu.layout_cache.rects[id] = rect;
    ss->menu.layout_edit.dirty = true;
}

bool layout_edit_enabled() {
    return ss && ss->menu.layout_edit.enabled;
}

void layout_toggle_edit_mode() {
    if (!ss)
        return;
    auto& edit = ss->menu.layout_edit;
    edit.enabled = !edit.enabled;
    edit.dragging = false;
    edit.resizing = false;
    edit.active_id.clear();
    edit.grab_offset = glm::vec2(0.0f);
    if (!edit.enabled)
        persist_layout_if_dirty();
}

static void clamp_rect(SDL_Rect& rect, int width, int height) {
    rect.x = std::clamp(rect.x, 0, std::max(0, width - rect.w));
    rect.y = std::clamp(rect.y, 0, std::max(0, height - rect.h));
    if (rect.x + rect.w > width)
        rect.w = width - rect.x;
    if (rect.y + rect.h > height)
        rect.h = height - rect.y;
}

void layout_handle_editor(std::vector<LayoutItem> items, int width, int height) {
    if (!ss || !layout_edit_enabled())
        return;
    auto& edit = ss->menu.layout_edit;
    bool mouse_pressed = ss->mouse_inputs.left && !ss->menu.mouse_left_prev;
    bool mouse_released = !ss->mouse_inputs.left && ss->menu.mouse_left_prev;
    int mx = ss->mouse_inputs.pos.x;
    int my = ss->mouse_inputs.pos.y;
    auto point_in = [&](const SDL_Rect& r) {
        return mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h;
    };
    SDL_Rect reset_rect = reset_button_rect(width, height);
    if (mouse_pressed && point_in(reset_rect)) {
        layout_reset_current();
        return;
    }
    const int handle = 12;
    if (mouse_pressed) {
        for (auto& item : items) {
            SDL_Rect move = clamp_handle_rect(SDL_Rect{item.rect.x - handle - 2, item.rect.y - handle - 2, handle, handle}, width, height);
            SDL_Rect resize = clamp_handle_rect(SDL_Rect{item.rect.x + item.rect.w + 2, item.rect.y + item.rect.h + 2, handle, handle}, width, height);
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
                               [&](const LayoutItem& li) { return li.id == edit.active_id; });
        auto snap_value = [&](int value) {
            if (!edit.snap)
                return value;
            const int step = 8;
            return (value / step) * step;
        };
        if (it != items.end()) {
            SDL_Rect rect = it->rect;
            if (!edit.resizing) {
                double new_x = static_cast<double>(mx) - static_cast<double>(edit.grab_offset.x);
                double new_y = static_cast<double>(my) - static_cast<double>(edit.grab_offset.y);
                rect.x = snap_value(static_cast<int>(std::round(new_x)));
                rect.y = snap_value(static_cast<int>(std::round(new_y)));
            } else {
                double base_x = static_cast<double>(rect.x);
                double base_y = static_cast<double>(rect.y);
                double new_br_x = static_cast<double>(mx) - static_cast<double>(edit.grab_offset.x);
                double new_br_y = static_cast<double>(my) - static_cast<double>(edit.grab_offset.y);
                rect.w = snap_value(static_cast<int>(std::round(std::max(40.0, new_br_x - base_x))));
                rect.h = snap_value(static_cast<int>(std::round(std::max(40.0, new_br_y - base_y))));
            }
            clamp_rect(rect, width, height);
            it->rect = rect;
            RectNdc ndc = pixels_to_ndc(static_cast<float>(rect.x), static_cast<float>(rect.y),
                                        static_cast<float>(rect.w), static_cast<float>(rect.h),
                                        width, height);
            layout_set_rect_internal(edit.active_id, ndc);
        }
    }
    if (mouse_released && edit.dragging) {
        edit.dragging = false;
        edit.resizing = false;
        edit.active_id.clear();
        persist_layout_if_dirty();
    }
}

void layout_render_overlay(SDL_Renderer* r, const std::vector<LayoutItem>& items, int width, int height) {
    if (!layout_edit_enabled() || !ss || !r)
        return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const int handle = 12;
    for (const auto& item : items) {
        SDL_Rect rect = item.rect;
        SDL_Color border = (ss->menu.layout_edit.active_id == item.id && ss->menu.layout_edit.dragging)
                               ? SDL_Color{250, 210, 120, 255}
                               : SDL_Color{150, 150, 200, 255};
        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(r, &rect);
        SDL_Rect move = clamp_handle_rect(SDL_Rect{rect.x - handle - 2, rect.y - handle - 2, handle, handle}, width, height);
        SDL_Rect resize = clamp_handle_rect(SDL_Rect{rect.x + rect.w + 2, rect.y + rect.h + 2, handle, handle}, width, height);
        SDL_SetRenderDrawColor(r, 40, 40, 50, 220);
        SDL_RenderFillRect(r, &move);
        SDL_RenderFillRect(r, &resize);
        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(r, &move);
        SDL_RenderDrawRect(r, &resize);
    }
    SDL_Rect reset_rect = reset_button_rect(width, height);
    SDL_SetRenderDrawColor(r, 40, 40, 60, 230);
    SDL_RenderFillRect(r, &reset_rect);
    SDL_SetRenderDrawColor(r, 200, 200, 220, 255);
    SDL_RenderDrawRect(r, &reset_rect);
    if (gg && gg->ui_font) {
        auto draw_text = [&](const std::string& text, int x, int y, SDL_Color c){
            if (text.empty()) return;
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
        std::string label = layout_current_section();
        draw_text(label.empty() ? "Layout Editor" : ("Layout: " + label),
                  20, static_cast<int>(MENU_SAFE_MARGIN * static_cast<float>(height)) + 10,
                  SDL_Color{240, 220, 80, 255});
        draw_text("Reset Layout", reset_rect.x + 12, reset_rect.y + 6, SDL_Color{220, 220, 230, 255});
        draw_text("Ctrl+L to exit", 20, reset_rect.y + reset_rect.h + 8, SDL_Color{200, 200, 210, 255});
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void layout_reset_current() {
    if (!ss)
        return;
    ensure_registry_loaded();
    auto& cache = ss->menu.layout_cache;
    auto& reg = registry();
    if (!cache.section.empty())
        reg.sections.erase(cache.section);
    cache.rects.clear();
    ss->menu.layout_edit.dirty = true;
    persist_layout_if_dirty();
}

std::string layout_current_section() {
    return ss ? ss->menu.layout_cache.section : std::string{};
}
