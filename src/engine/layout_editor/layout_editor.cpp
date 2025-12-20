#include "engine/layout_editor/layout_editor.hpp"

#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool g_active = false;
int g_selected_layout = 0;
float g_grid_step = 0.1f;
bool g_snap_enabled = true;
std::string g_status_text;
float g_status_timer = 0.0f;
bool g_use_virtual_resolution = false;
int g_virtual_width = 1920;
int g_virtual_height = 1080;
int g_pending_width = 1920;
int g_pending_height = 1080;

bool has_layouts() {
    return es && !es->ui_layouts_pool.empty();
}

const UILayout* selected_layout() {
    if (!has_layouts())
        return nullptr;
    g_selected_layout = std::clamp(g_selected_layout, 0,
                                    static_cast<int>(es->ui_layouts_pool.size()) - 1);
    return &es->ui_layouts_pool[static_cast<std::size_t>(g_selected_layout)];
}

void append_status(const std::string& text) {
    g_status_text = text;
    g_status_timer = 3.0f;
}

struct ViewportInfo {
    SDL_FRect rect;
    float source_w;
    float source_h;
};

ViewportInfo compute_viewport(int screen_w, int screen_h, const UILayout& layout) {
    float src_w = g_use_virtual_resolution && g_virtual_width > 0
                      ? static_cast<float>(g_virtual_width)
                      : static_cast<float>(layout.resolution_width);
    float src_h = g_use_virtual_resolution && g_virtual_height > 0
                      ? static_cast<float>(g_virtual_height)
                      : static_cast<float>(layout.resolution_height);
    if (src_w <= 0.0f) src_w = 1.0f;
    if (src_h <= 0.0f) src_h = 1.0f;
    float src_aspect = src_w / src_h;
    float dst_aspect = static_cast<float>(screen_w) / static_cast<float>(screen_h);
    SDL_FRect rect{};
    if (dst_aspect >= src_aspect) {
        rect.h = static_cast<float>(screen_h);
        rect.w = rect.h * src_aspect;
        rect.x = (static_cast<float>(screen_w) - rect.w) * 0.5f;
        rect.y = 0.0f;
    } else {
        rect.w = static_cast<float>(screen_w);
        rect.h = rect.w / src_aspect;
        rect.x = 0.0f;
        rect.y = (static_cast<float>(screen_h) - rect.h) * 0.5f;
    }
    return {rect, src_w, src_h};
}

void shade_letterbox(SDL_Renderer* renderer, const SDL_FRect& rect, int width, int height) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 5, 5, 10, 150);
    auto fill_rect = [&](float x, float y, float w, float h) {
        SDL_FRect r{x, y, w, h};
        SDL_RenderFillRectF(renderer, &r);
    };
    fill_rect(0.0f, 0.0f, rect.x, static_cast<float>(height));
    fill_rect(rect.x + rect.w, 0.0f,
              static_cast<float>(width) - (rect.x + rect.w),
              static_cast<float>(height));
    fill_rect(0.0f, 0.0f, static_cast<float>(width), rect.y);
    fill_rect(0.0f, rect.y + rect.h, static_cast<float>(width),
              static_cast<float>(height) - (rect.y + rect.h));
    SDL_SetRenderDrawColor(renderer, 90, 90, 120, 200);
    SDL_RenderDrawRectF(renderer, &rect);
}

void draw_grid(SDL_Renderer* renderer, const SDL_FRect& view_rect) {
    const float step = std::clamp(g_grid_step, 0.01f, 0.5f);
    const float px_step_x = step * view_rect.w;
    const float px_step_y = step * view_rect.h;
    SDL_BlendMode old_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 120);

    for (float x = view_rect.x; x <= view_rect.x + view_rect.w + 0.5f; x += px_step_x) {
        SDL_RenderDrawLineF(renderer, x, view_rect.y, x, view_rect.y + view_rect.h);
    }
    for (float y = view_rect.y; y <= view_rect.y + view_rect.h + 0.5f; y += px_step_y) {
        SDL_RenderDrawLineF(renderer, view_rect.x, y, view_rect.x + view_rect.w, y);
    }

    SDL_SetRenderDrawBlendMode(renderer, old_mode);
}

void draw_layout_overlay(SDL_Renderer* renderer,
                         const UILayout& layout,
                         const SDL_FRect& view_rect) {
    SDL_BlendMode old_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const auto& obj : layout.objects) {
        SDL_FRect rect;
        rect.x = view_rect.x + obj.x * view_rect.w;
        rect.y = view_rect.y + obj.y * view_rect.h;
        rect.w = obj.w * view_rect.w;
        rect.h = obj.h * view_rect.h;

        SDL_Color fill{60, 170, 255, 40};
        SDL_Color border{60, 170, 255, 200};
        fill_and_outline(renderer, rect, fill, border);

        std::string text = obj.label.empty()
                               ? std::to_string(obj.id)
                               : obj.label + " (" + std::to_string(obj.id) + ")";
        draw_text(renderer, text, static_cast<int>(rect.x) + 6,
                  static_cast<int>(rect.y) + 6,
                  SDL_Color{255, 255, 255, 200});

        char coords[64];
        std::snprintf(coords, sizeof(coords), "x%.3f y%.3f w%.3f h%.3f",
                      static_cast<double>(obj.x),
                      static_cast<double>(obj.y),
                      static_cast<double>(obj.w),
                      static_cast<double>(obj.h));
        draw_text(renderer, coords, static_cast<int>(rect.x) + 6,
                  static_cast<int>(rect.y) + 24,
                  SDL_Color{210, 220, 240, 200});
    }
    SDL_SetRenderDrawBlendMode(renderer, old_mode);
}

void render_panel(float dt) {
    if (!g_active)
        return;
    if (!ImGui::GetCurrentContext())
        return;
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings;
    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.93f);
    if (!ImGui::Begin("Layout Editor", &g_active, flags)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("Ctrl+L toggle | Ctrl+S save | G snap");
    ImGui::Text("Grid %.3f", static_cast<double>(g_grid_step));
    if (!has_layouts()) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }

    std::vector<const char*> labels;
    labels.reserve(es->ui_layouts_pool.size());
    static std::vector<std::string> label_storage;
    label_storage.clear();
    for (const auto& layout : es->ui_layouts_pool) {
        std::string label = layout.label + " (ID " + std::to_string(layout.id) + ") " +
                            std::to_string(layout.resolution_width) + "x" +
                            std::to_string(layout.resolution_height);
        label_storage.push_back(label);
    }
    for (const auto& s : label_storage)
        labels.push_back(s.c_str());

    ImGui::ListBox("Layouts", &g_selected_layout, labels.data(),
                   static_cast<int>(labels.size()), 6);

    if (const UILayout* layout = selected_layout()) {
        ImGui::Separator();
        ImGui::Text("Objects: %zu", layout->objects.size());
        ImGui::Text("Layout: %dx%d",
                    layout->resolution_width, layout->resolution_height);
        ImGui::Text("Preview: %dx%d",
                    g_use_virtual_resolution ? g_virtual_width : layout->resolution_width,
                    g_use_virtual_resolution ? g_virtual_height : layout->resolution_height);
    }

    ImGui::Separator();
    ImGui::Checkbox("Override resolution", &g_use_virtual_resolution);
    if (g_use_virtual_resolution) {
        ImGui::InputInt("Width", &g_pending_width);
        ImGui::InputInt("Height", &g_pending_height);
        if (ImGui::Button("Apply")) {
            g_virtual_width = std::max(16, g_pending_width);
            g_virtual_height = std::max(16, g_pending_height);
        }
        ImGui::SameLine();
        if (ImGui::Button("Match layout")) {
            if (const UILayout* layout = selected_layout()) {
                g_virtual_width = layout->resolution_width;
                g_virtual_height = layout->resolution_height;
                g_pending_width = g_virtual_width;
                g_pending_height = g_virtual_height;
            }
        }
        ImGui::TextUnformatted("Presets:");
        if (ImGui::Button("1080p")) {
            g_virtual_width = 1920;
            g_virtual_height = 1080;
            g_pending_width = g_virtual_width;
            g_pending_height = g_virtual_height;
        }
        ImGui::SameLine();
        if (ImGui::Button("1440p")) {
            g_virtual_width = 2560;
            g_virtual_height = 1440;
            g_pending_width = g_virtual_width;
            g_pending_height = g_virtual_height;
        }
        ImGui::SameLine();
        if (ImGui::Button("2560x1080")) {
            g_virtual_width = 2560;
            g_virtual_height = 1080;
            g_pending_width = g_virtual_width;
            g_pending_height = g_virtual_height;
        }
        ImGui::SameLine();
        if (ImGui::Button("1600x1200")) {
            g_virtual_width = 1600;
            g_virtual_height = 1200;
            g_pending_width = g_virtual_width;
            g_pending_height = g_virtual_height;
        }
    }

    if (!g_status_text.empty() && g_status_timer > 0.0f) {
        g_status_timer = std::max(0.0f, g_status_timer - dt);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", g_status_text.c_str());
    }

    ImGui::End();
}

void handle_hotkeys() {
    if (!ImGui::GetCurrentContext())
        return;
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_L) && io.KeyCtrl) {
        g_active = !g_active;
        if (!g_active)
            append_status("Layout editor deactivated");
        else
            append_status("Layout editor activated");
    }
    if (!g_active)
        return;
    if (has_layouts() && ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
        if (const UILayout* layout = selected_layout()) {
            if (save_ui_layout(*layout))
                append_status("Layout saved");
            else
                append_status("Failed to save layout");
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G))
        g_snap_enabled = !g_snap_enabled;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal))
        g_grid_step = std::max(0.01f, g_grid_step - 0.01f);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus))
        g_grid_step = std::min(0.5f, g_grid_step + 0.01f);
}

} // namespace

void layout_editor_begin_frame(float dt) {
    handle_hotkeys();
    if (g_active)
        render_panel(dt);
    else
        g_status_timer = std::max(0.0f, g_status_timer - dt);
}

bool layout_editor_is_active() {
    return g_active;
}

bool layout_editor_wants_input() {
    return g_active;
}

void layout_editor_render(SDL_Renderer* renderer, int screen_width, int screen_height) {
    if (!g_active || !renderer)
        return;
    if (!has_layouts())
        return;
    if (const UILayout* layout = selected_layout()) {
        ViewportInfo view = compute_viewport(screen_width, screen_height, *layout);
        shade_letterbox(renderer, view.rect, screen_width, screen_height);
        draw_grid(renderer, view.rect);
        draw_layout_overlay(renderer, *layout, view.rect);
    }
}

void layout_editor_shutdown() {
    g_active = false;
    g_status_text.clear();
    g_status_timer = 0.0f;
}
