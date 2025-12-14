#include "main_menu/menu_objects.hpp"

#include "globals.hpp"
#include "main_menu/menu_internal.hpp"
#include "main_menu/menu_layout.hpp"
#include "state.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace {

constexpr const char* kObjectsFilePath = "config/menu_objects.ini";

using GuiObjectDesc = State::MenuState::GuiObjectDesc;

struct ObjectsRegistry {
    bool loaded{false};
    std::unordered_map<std::string, std::vector<GuiObjectDesc>> sections;
};

ObjectsRegistry& registry() {
    static ObjectsRegistry reg;
    return reg;
}

std::string section_for_page(const std::string& page) {
    unsigned w = gg ? gg->window_dims.x : 1280u;
    unsigned h = gg ? gg->window_dims.y : 720u;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %ux%u", page.c_str(), w, h);
    return std::string(buf);
}

RectNdc ndc_from_pixels(float px, float py, float pw, float ph, int width, int height) {
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
    out.w = std::max(0.005f, std::min(1.0f + MENU_SAFE_MARGIN - out.x, out.w));
    out.h = std::max(0.005f, std::min(1.0f + MENU_SAFE_MARGIN - out.y, out.h));
    return out;
}

void ensure_registry_loaded() {
    auto& reg = registry();
    if (reg.loaded)
        return;
    std::ifstream in(kObjectsFilePath);
    if (in.is_open()) {
        std::string line;
        std::string section;
        while (std::getline(in, line)) {
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
            std::string rest = trim_copy(line.substr(eq + 1));
            std::string rect_part = rest;
            std::string label_part;
            auto pipe = rest.find('|');
            if (pipe != std::string::npos) {
                rect_part = trim_copy(rest.substr(0, pipe));
                label_part = trim_copy(rest.substr(pipe + 1));
            }
            RectNdc rect;
            {
                std::array<float, 4> vals{};
                size_t pos = 0;
                bool ok = true;
                for (int i = 0; i < 4; ++i) {
                    size_t comma = rect_part.find(',', pos);
                    std::string token = (comma == std::string::npos) ? rect_part.substr(pos)
                                                                     : rect_part.substr(pos, comma - pos);
                    token = trim_copy(token);
                    if (token.empty()) { ok = false; break; }
                    try {
                        vals[static_cast<size_t>(i)] = std::stof(token);
                    } catch (...) {
                        ok = false;
                        break;
                    }
                    if (comma == std::string::npos) {
                        if (i != 3) ok = false;
                    } else {
                        pos = comma + 1;
                    }
                }
                if (!ok)
                    continue;
                rect = RectNdc{vals[0], vals[1], vals[2], vals[3]};
            }
            GuiObjectDesc desc;
            desc.id = key;
            desc.rect = rect;
            desc.label = label_part;
            reg.sections[section].push_back(desc);
        }
    }
    reg.loaded = true;
}

void persist_registry() {
    auto& reg = registry();
    std::filesystem::path path = kObjectsFilePath;
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
        const auto& items = reg.sections[sec];
        for (const auto& obj : items) {
            out << obj.id << '=' << obj.rect.x << ',' << obj.rect.y << ','
                << obj.rect.w << ',' << obj.rect.h;
            if (!obj.label.empty())
                out << " | " << obj.label;
            out << "\n";
        }
        out << "\n";
    }
}

void persist_cache() {
    if (!ss)
        return;
    auto& cache = ss->menu.objects_cache;
    if (!cache.dirty || cache.section.empty())
        return;
    ensure_registry_loaded();
    auto& reg = registry();
    reg.sections[cache.section] = cache.items;
    persist_registry();
    cache.dirty = false;
}

} // namespace

void gui_objects_set_page(const std::string& page_key) {
    if (!ss)
        return;
    ensure_registry_loaded();
    auto& cache = ss->menu.objects_cache;
    cache.page = page_key;
    cache.section = section_for_page(page_key);
    auto& reg = registry();
    auto it = reg.sections.find(cache.section);
    if (it != reg.sections.end())
        cache.items = it->second;
    else
        cache.items.clear();
    cache.dirty = false;
    cache.next_id = std::max(1, static_cast<int>(cache.items.size()) + 1);
}

void gui_objects_save_if_dirty() {
    persist_cache();
}

std::vector<LayoutItem> gui_objects_layout_items(int width, int height) {
    std::vector<LayoutItem> out;
    if (!ss)
        return out;
    const auto& cache = ss->menu.objects_cache;
    out.reserve(cache.items.size());
    for (const auto& obj : cache.items) {
        SDL_FRect pf = ndc_to_pixels(obj.rect, width, height);
        SDL_Rect rect{
            static_cast<int>(std::floor(pf.x)),
            static_cast<int>(std::floor(pf.y)),
            static_cast<int>(std::ceil(pf.w)),
            static_cast<int>(std::ceil(pf.h))
        };
        out.push_back(LayoutItem{obj.id, rect});
    }
    return out;
}

std::vector<GuiObjectPixel> gui_objects_pixel_items(int width, int height) {
    std::vector<GuiObjectPixel> out;
    if (!ss)
        return out;
    const auto& cache = ss->menu.objects_cache;
    out.reserve(cache.items.size());
    for (const auto& obj : cache.items) {
        SDL_FRect pf = ndc_to_pixels(obj.rect, width, height);
        GuiObjectPixel px{};
        px.id = obj.id;
        px.label = obj.label;
        px.rect = SDL_Rect{
            static_cast<int>(std::floor(pf.x)),
            static_cast<int>(std::floor(pf.y)),
            static_cast<int>(std::ceil(pf.w)),
            static_cast<int>(std::ceil(pf.h))
        };
        out.push_back(px);
    }
    return out;
}

bool gui_objects_rename(const std::string& id, const std::string& new_label) {
    if (!ss)
        return false;
    auto& cache = ss->menu.objects_cache;
    for (auto& obj : cache.items) {
        if (obj.id == id) {
            obj.label = new_label;
            cache.dirty = true;
            persist_cache();
            return true;
        }
    }
    return false;
}

RectNdc gui_objects_rect_from_pixels(const SDL_Rect& rect, int width, int height) {
    return ndc_from_pixels(static_cast<float>(rect.x), static_cast<float>(rect.y),
                           static_cast<float>(rect.w), static_cast<float>(rect.h),
                           width, height);
}
