#include "engine/layout_editor/layout_editor.hpp"
#include "engine/layout_editor/layout_editor_hooks.hpp"
#include "engine/layout_editor/layout_editor_interaction.hpp"

#include "engine/globals.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"
#include "engine/graphics.hpp"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdio>
#include <string>
#include <vector>

namespace {

bool g_active = false;
int g_selected_layout = 0;
float g_grid_step = 0.2f;
bool g_snap_enabled = true;
std::string g_status_text;
float g_status_timer = 0.0f;
bool g_follow_active_layout = true;
bool g_mouse_was_down = false;

struct PendingLayoutRequest {
    bool valid{false};
    int id{-1};
    int width{0};
    int height{0};
};

PendingLayoutRequest g_last_request{};
bool g_layout_dirty = false;

bool has_layouts() {
    return es && !es->ui_layouts_pool.empty();
}

UILayout* selected_layout_mutable() {
    if (!has_layouts())
        return nullptr;
    g_selected_layout = std::clamp(g_selected_layout, 0,
                                    static_cast<int>(es->ui_layouts_pool.size()) - 1);
    return &es->ui_layouts_pool[static_cast<std::size_t>(g_selected_layout)];
}

const UILayout* selected_layout() {
    return selected_layout_mutable();
}

void append_status(const std::string& text) {
    g_status_text = text;
    g_status_timer = 3.0f;
}

bool select_layout_exact(int id, int width, int height) {
    if (!es)
        return false;
    for (std::size_t i = 0; i < es->ui_layouts_pool.size(); ++i) {
        const auto& layout = es->ui_layouts_pool[i];
        if (layout.id == id &&
            layout.resolution_width == width &&
            layout.resolution_height == height) {
            g_selected_layout = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

void auto_follow_selection() {
    if (!g_follow_active_layout)
        return;
    if (g_last_request.valid) {
        if (select_layout_exact(g_last_request.id,
                                g_last_request.width,
                                g_last_request.height))
            return;
    }
    if (!gg || !has_layouts())
        return;
    int target_w = static_cast<int>(gg->render_dims.x);
    int target_h = static_cast<int>(gg->render_dims.y);
    if (target_w <= 0 || target_h <= 0)
        return;
    int best_idx = g_selected_layout;
    int best_score = INT_MAX;
    for (std::size_t i = 0; i < es->ui_layouts_pool.size(); ++i) {
        const auto& layout = es->ui_layouts_pool[i];
        int dw = layout.resolution_width - target_w;
        int dh = layout.resolution_height - target_h;
        int score = dw * dw + dh * dh;
        if (score < best_score) {
            best_score = score;
            best_idx = static_cast<int>(i);
        }
    }
    g_selected_layout = best_idx;
}

void draw_grid(SDL_Renderer* renderer,
               int width,
               int height,
               float origin_x,
               float origin_y) {
    if (width <= 0 || height <= 0)
        return;
    const float step = std::clamp(g_grid_step, 0.01f, 0.5f);
    SDL_BlendMode old_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 40, 40, 50, 120);

    SDL_Color label_color{150, 160, 190, 210};

    const float epsilon = 1e-4f;
    const int steps_x = std::max(1, static_cast<int>(std::ceil(1.0f / step)));
    for (int i = 0; i <= steps_x; ++i) {
        float norm = std::min(step * static_cast<float>(i), 1.0f);
        float x = norm * static_cast<float>(width);
        SDL_RenderDrawLineF(renderer,
                            origin_x + x,
                            origin_y,
                            origin_x + x,
                            origin_y + static_cast<float>(height));
        char label[16];
        std::snprintf(label, sizeof(label), "%.3f", static_cast<double>(norm));
        int text_x = static_cast<int>(x) - 14;
        text_x = std::clamp(text_x, 0, std::max(0, width - 28));
        draw_text(renderer, label,
                  static_cast<int>(origin_x) + text_x,
                  static_cast<int>(origin_y) + 2,
                  label_color);
        if (norm > epsilon && norm < 1.0f - epsilon)
            draw_text(renderer, label,
                      static_cast<int>(origin_x) + text_x,
                      static_cast<int>(origin_y) + std::max(height - 18, 0),
                      label_color);
    }
    const int steps_y = std::max(1, static_cast<int>(std::ceil(1.0f / step)));
    for (int i = 0; i <= steps_y; ++i) {
        float norm = std::min(step * static_cast<float>(i), 1.0f);
        float y = norm * static_cast<float>(height);
        SDL_RenderDrawLineF(renderer,
                            origin_x,
                            origin_y + y,
                            origin_x + static_cast<float>(width),
                            origin_y + y);
        char label[16];
        std::snprintf(label, sizeof(label), "%.3f", static_cast<double>(norm));
        int text_y = static_cast<int>(y) - 8;
        text_y = std::clamp(text_y, 0, std::max(0, height - 16));
        if (norm > epsilon && norm < 1.0f - epsilon) {
            draw_text(renderer, label,
                      static_cast<int>(origin_x) + 2,
                      static_cast<int>(origin_y) + text_y,
                      label_color);
            draw_text(renderer, label,
                      static_cast<int>(origin_x) + std::max(width - 60, 2),
                      static_cast<int>(origin_y) + text_y,
                      label_color);
        }
    }

    SDL_SetRenderDrawBlendMode(renderer, old_mode);
}

void draw_layout_overlay(SDL_Renderer* renderer,
                         const UILayout& layout,
                         int width,
                         int height,
                         float origin_x,
                         float origin_y,
                         int selected_index,
                         int dragging_index) {
    SDL_BlendMode old_mode;
    SDL_GetRenderDrawBlendMode(renderer, &old_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (std::size_t idx = 0; idx < layout.objects.size(); ++idx) {
        const auto& obj = layout.objects[idx];
        SDL_FRect rect;
        rect.x = origin_x + obj.x * static_cast<float>(width);
        rect.y = origin_y + obj.y * static_cast<float>(height);
        rect.w = obj.w * static_cast<float>(width);
        rect.h = obj.h * static_cast<float>(height);

        SDL_Color fill{60, 170, 255, 40};
        SDL_Color border{60, 170, 255, 200};
        if (static_cast<int>(idx) == selected_index) {
            fill = SDL_Color{120, 210, 120, 70};
            border = SDL_Color{140, 240, 140, 230};
        }
        if (static_cast<int>(idx) == dragging_index) {
            fill = SDL_Color{220, 200, 90, 70};
            border = SDL_Color{250, 230, 110, 230};
        }
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
    ImGui::Text("Grid %.3f (%s)",
                static_cast<double>(g_grid_step),
                g_snap_enabled ? "snap ON" : "snap OFF");
    if (!has_layouts()) {
        ImGui::TextUnformatted("No layouts loaded.");
        ImGui::End();
        return;
    }

    const char* factor_labels[] = {"Desktop", "Tablet", "Phone"};
    std::vector<const char*> labels;
    labels.reserve(es->ui_layouts_pool.size());
    static std::vector<std::string> label_storage;
    label_storage.clear();
    for (const auto& layout : es->ui_layouts_pool) {
        std::string label = layout.label + " (ID " + std::to_string(layout.id) + ") " +
                            std::to_string(layout.resolution_width) + "x" +
                            std::to_string(layout.resolution_height) + " [" +
                            factor_labels[static_cast<int>(layout.form_factor)] + "]";
        label_storage.push_back(label);
    }
    for (const auto& s : label_storage)
        labels.push_back(s.c_str());

    ImGui::Checkbox("Follow active layout", &g_follow_active_layout);
    if (ImGui::ListBox("Layouts", &g_selected_layout, labels.data(),
                       static_cast<int>(labels.size()), 6)) {
        g_follow_active_layout = false;
    }

    if (const UILayout* layout = selected_layout()) {
        ImGui::Separator();
        ImGui::Text("Objects: %zu", layout->objects.size());
        ImGui::Text("Layout: %dx%d",
                    layout->resolution_width, layout->resolution_height);
    }
    if (gg) {
        glm::ivec2 dims = get_render_dimensions();
        ImGui::Text("Render target: %dx%d", dims.x, dims.y);
    }

    if (!g_status_text.empty() && g_status_timer > 0.0f) {
        g_status_timer = std::max(0.0f, g_status_timer - dt);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "%s", g_status_text.c_str());
    }

    ImGui::End();
}

void handle_mouse_input() {
    if (!g_active || !es)
        return;
    LayoutEditorViewport viewport = layout_editor_get_viewport();
    if (viewport.width <= 0.0f || viewport.height <= 0.0f)
        return;
    UILayout* layout = selected_layout_mutable();
    if (!layout)
        return;

    float mouse_x = static_cast<float>(es->device_state.mouse_x);
    float mouse_y = static_cast<float>(es->device_state.mouse_y);
    bool mouse_down = (es->device_state.mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    if (mouse_down && !g_mouse_was_down) {
        int hit_index = -1;
        if (layout_editor_hit_test(*layout, viewport, mouse_x, mouse_y, hit_index)) {
            layout_editor_select(hit_index);
            layout_editor_begin_drag(*layout, hit_index, mouse_x, mouse_y, viewport);
        } else {
            layout_editor_clear_selection();
            layout_editor_end_drag();
        }
    } else if (mouse_down && layout_editor_is_dragging()) {
        if (layout_editor_update_drag(*layout, mouse_x, mouse_y, g_snap_enabled, g_grid_step))
            g_layout_dirty = true;
    } else if (!mouse_down && g_mouse_was_down) {
        layout_editor_end_drag();
    }

    g_mouse_was_down = mouse_down;
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

    handle_mouse_input();
}

} // namespace

void layout_editor_notify_active_layout(int layout_id,
                                        int resolution_width,
                                        int resolution_height) {
    g_last_request.valid = true;
    g_last_request.id = layout_id;
    g_last_request.width = resolution_width;
    g_last_request.height = resolution_height;
    if (g_follow_active_layout)
        auto_follow_selection();
}

bool layout_editor_consume_dirty_flag() {
    bool dirty = g_layout_dirty;
    g_layout_dirty = false;
    return dirty;
}

void layout_editor_begin_frame(float dt) {
    if (g_follow_active_layout)
        auto_follow_selection();
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

void layout_editor_render(SDL_Renderer* renderer,
                          int screen_width,
                          int screen_height,
                          float origin_x,
                          float origin_y) {
    if (!g_active || !renderer)
        return;
    if (!has_layouts())
        return;
    layout_editor_set_viewport(LayoutEditorViewport{origin_x,
                                                    origin_y,
                                                    static_cast<float>(screen_width),
                                                    static_cast<float>(screen_height)});
    draw_grid(renderer, screen_width, screen_height, origin_x, origin_y);
    if (const UILayout* layout = selected_layout()) {
        int selected_idx = layout_editor_selected_index();
        int dragging_idx = layout_editor_dragging_index();
        draw_layout_overlay(renderer, *layout, screen_width, screen_height,
                            origin_x, origin_y, selected_idx, dragging_idx);
    }
}

void layout_editor_shutdown() {
    g_active = false;
    g_status_text.clear();
    g_status_timer = 0.0f;
}
