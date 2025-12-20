#include "engine/layout_editor/layout_editor_overlay.hpp"

#include "engine/layout_editor/layout_editor_interaction.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <array>
#include <algorithm>

namespace {

struct HandleDrawInfo {
    HandleType type;
    SDL_FRect rect;
};

std::array<HandleDrawInfo, 8> build_handle_rects(const SDL_FRect& rect) {
    std::array<HandleDrawInfo, 8> handles{};
    float half_corner = kHandleSize * 0.5f;
    float edge_len_v = std::min(rect.h, kEdgeHandleLength);
    float edge_len_h = std::min(rect.w, kEdgeHandleLength);

    handles[0] = {HandleType::CornerTopLeft,
                  SDL_FRect{rect.x - half_corner, rect.y - half_corner,
                            kHandleSize, kHandleSize}};
    handles[1] = {HandleType::CornerTopRight,
                  SDL_FRect{rect.x + rect.w - half_corner, rect.y - half_corner,
                            kHandleSize, kHandleSize}};
    handles[2] = {HandleType::CornerBottomLeft,
                  SDL_FRect{rect.x - half_corner, rect.y + rect.h - half_corner,
                            kHandleSize, kHandleSize}};
    handles[3] = {HandleType::CornerBottomRight,
                  SDL_FRect{rect.x + rect.w - half_corner,
                            rect.y + rect.h - half_corner,
                            kHandleSize, kHandleSize}};

    handles[4] = {HandleType::EdgeTop,
                  SDL_FRect{rect.x + rect.w * 0.5f - edge_len_h * 0.5f,
                            rect.y - kEdgeHandleThickness * 0.5f,
                            edge_len_h,
                            kEdgeHandleThickness}};
    handles[5] = {HandleType::EdgeBottom,
                  SDL_FRect{rect.x + rect.w * 0.5f - edge_len_h * 0.5f,
                            rect.y + rect.h - kEdgeHandleThickness * 0.5f,
                            edge_len_h,
                            kEdgeHandleThickness}};
    handles[6] = {HandleType::EdgeLeft,
                  SDL_FRect{rect.x - kEdgeHandleThickness * 0.5f,
                            rect.y + rect.h * 0.5f - edge_len_v * 0.5f,
                            kEdgeHandleThickness,
                            edge_len_v}};
    handles[7] = {HandleType::EdgeRight,
                  SDL_FRect{rect.x + rect.w - kEdgeHandleThickness * 0.5f,
                            rect.y + rect.h * 0.5f - edge_len_v * 0.5f,
                            kEdgeHandleThickness,
                            edge_len_v}};
    return handles;
}

void draw_handles(SDL_Renderer* renderer,
                  const SDL_FRect& rect,
                  HandleType highlight) {
    auto handles = build_handle_rects(rect);
    SDL_Color base_fill{110, 170, 255, 140};
    SDL_Color base_border{40, 120, 230, 220};
    SDL_Color highlight_fill{250, 230, 120, 170};
    SDL_Color highlight_border{255, 250, 140, 240};
    for (const auto& handle : handles) {
        bool active = (handle.type == highlight);
        fill_and_outline(renderer,
                         handle.rect,
                         active ? highlight_fill : base_fill,
                         active ? highlight_border : base_border);
    }
}

} // namespace

void layout_editor_draw_grid(SDL_Renderer* renderer,
                             int width,
                             int height,
                             float origin_x,
                             float origin_y,
                             float grid_step) {
    if (width <= 0 || height <= 0)
        return;
    const float step = std::clamp(grid_step, 0.01f, 0.5f);
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

void layout_editor_draw_layout(SDL_Renderer* renderer,
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

        const bool is_selected = static_cast<int>(idx) == selected_index;
        const bool is_dragging = static_cast<int>(idx) == dragging_index;

        SDL_Color fill{60, 170, 255, 40};
        SDL_Color border{60, 170, 255, 200};
        if (is_selected) {
            fill = SDL_Color{120, 210, 120, 70};
            border = SDL_Color{140, 240, 140, 230};
        }
        if (is_dragging) {
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

        if (is_selected) {
            HandleType handle = is_dragging ? layout_editor_drag_handle()
                                            : HandleType::Center;
            draw_handles(renderer, rect, handle);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, old_mode);
}
