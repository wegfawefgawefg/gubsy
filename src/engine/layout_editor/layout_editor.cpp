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

void draw_grid(SDL_Renderer* renderer, int width, int height) {
    const float step = std::clamp(g_grid_step, 0.01f, 0.5f);
    const float px_step_x = step * static_cast<float>(width);
    const float px_step_y = step * static_cast<float>(height);
    SDL_BlendMode old_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 120);

    const float fwidth = static_cast<float>(width);
    const float fheight = static_cast<float>(height);
    for (float x = 0.0f; x <= fwidth; x += px_step_x) {
        SDL_RenderDrawLineF(renderer, x, 0.0f, x, static_cast<float>(height));
    }
    for (float y = 0.0f; y <= fheight; y += px_step_y) {
        SDL_RenderDrawLineF(renderer, 0.0f, y, static_cast<float>(width), y);
    }

    SDL_SetRenderDrawBlendMode(renderer, old_mode);
}

void draw_layout_overlay(SDL_Renderer* renderer,
                         const UILayout& layout,
                         int width,
                         int height) {
    SDL_BlendMode old_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const auto& obj : layout.objects) {
        SDL_FRect rect;
        rect.x = obj.x * static_cast<float>(width);
        rect.y = obj.y * static_cast<float>(height);
        rect.w = obj.w * static_cast<float>(width);
        rect.h = obj.h * static_cast<float>(height);

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

    const UILayout* layout = selected_layout();
    if (layout) {
        ImGui::Separator();
        ImGui::Text("Objects: %zu", layout->objects.size());
        ImGui::Text("Resolution: %dx%d",
                    layout->resolution_width, layout->resolution_height);
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
    draw_grid(renderer, screen_width, screen_height);
    if (const UILayout* layout = selected_layout())
        draw_layout_overlay(renderer, *layout, screen_width, screen_height);
}

void layout_editor_shutdown() {
    g_active = false;
    g_status_text.clear();
    g_status_timer = 0.0f;
}
