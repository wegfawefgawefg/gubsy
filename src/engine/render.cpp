#include "render.hpp"

#include "engine/mode_registry.hpp"
#include "engine/graphics.hpp"
#include "engine/imgui_layer.hpp"
#include "engine/imgui_debug/imgui_debug.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/input_sources.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <glm/glm.hpp>
#include <engine/globals.hpp>

namespace {

SDL_FRect compute_letterbox_rect(const glm::uvec2& render_dims,
                                 const glm::uvec2& window_dims) {
    SDL_FRect rect{};
    if (window_dims.x == 0 || window_dims.y == 0 ||
        render_dims.x == 0 || render_dims.y == 0) {
        rect.w = static_cast<float>(window_dims.x);
        rect.h = static_cast<float>(window_dims.y);
        return rect;
    }
    float src_aspect = static_cast<float>(render_dims.x) / static_cast<float>(render_dims.y);
    float dst_aspect = static_cast<float>(window_dims.x) / static_cast<float>(window_dims.y);
    if (dst_aspect >= src_aspect) {
        rect.h = static_cast<float>(window_dims.y);
        rect.w = rect.h * src_aspect;
        rect.x = (static_cast<float>(window_dims.x) - rect.w) * 0.5f;
        rect.y = 0.0f;
    } else {
        rect.w = static_cast<float>(window_dims.x);
        rect.h = rect.w / src_aspect;
        rect.x = 0.0f;
        rect.y = (static_cast<float>(window_dims.y) - rect.h) * 0.5f;
    }
    return rect;
}

} // namespace

ScreenSpace make_space(int width, int height) {
    ScreenSpace space{};
    space.scale = static_cast<float>(std::min(width, height)) * 0.08f;
    space.cx = static_cast<float>(width) * 0.5f;
    space.cy = static_cast<float>(height) * 0.5f;
    return space;
}

SDL_FRect rect_for(const glm::vec2& pos, const glm::vec2& half, const ScreenSpace& space) {
    float sx = space.cx + pos.x * space.scale - half.x * space.scale;
    float sy = space.cy + pos.y * space.scale - half.y * space.scale;
    float sw = half.x * 2.0f * space.scale;
    float sh = half.y * 2.0f * space.scale;
    return SDL_FRect{sx, sy, sw, sh};
}

void fill_and_outline(SDL_Renderer* renderer, const SDL_FRect& rect,
                      SDL_Color fill, SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRectF(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRectF(renderer, &rect);
}

Uint8 channel_from_vec(float v) {
    float clamped = std::clamp(v, 0.0f, 1.0f);
    return static_cast<Uint8>(std::round(clamped * 255.0f));
}

SDL_Color color_from_vec3(const glm::vec3& v, Uint8 alpha) {
    return SDL_Color{channel_from_vec(v.r), channel_from_vec(v.g), channel_from_vec(v.b), alpha};
}

glm::vec3 brighten(const glm::vec3& base, float amount) {
    return glm::clamp(base + glm::vec3(amount), glm::vec3(0.0f), glm::vec3(1.0f));
}

void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    if (!gg || !gg->ui_font || text.empty())
        return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(gg->ui_font, text.c_str(), color);
    if (!surf)
        return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex)
        return;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

void render_alerts(SDL_Renderer* renderer, int width) {
    if (es->alerts.empty())
        return;
    int y = 20;
    for (const auto& alert : es->alerts) {
        draw_text(renderer, alert.text, 24, y, SDL_Color{255, 235, 160, 255});
        y += 22;
        if (y > 200)
            break;
    }

    // Mode label
    std::string mode = "Mode: " + es->mode;
    draw_text(renderer, mode, width - 220, 20, SDL_Color{180, 180, 200, 255});
}

void render() {
    SDL_Renderer* renderer = (gg ? gg->renderer : nullptr);
    if (!renderer)
        return;

    SDL_Texture* target = (gg ? gg->render_target : nullptr);
    if (target)
        SDL_SetRenderTarget(renderer, target);

    if (const ModeDesc* mode = find_mode(es->mode)) {
        if (mode->render_fn) {
            mode->render_fn();
        }
    }

    if (target)
        SDL_SetRenderTarget(renderer, nullptr);

    int window_w = 0;
    int window_h = 0;
    SDL_GetRendererOutputSize(renderer, &window_w, &window_h);
    if (gg)
        gg->window_dims = {static_cast<unsigned int>(window_w),
                           static_cast<unsigned int>(window_h)};

    if (target) {
        SDL_SetRenderDrawColor(renderer, 5, 5, 10, 255);
        SDL_RenderClear(renderer);
        if (gg->render_scale_mode == RenderScaleMode::Stretch) {
            SDL_RenderCopy(renderer, target, nullptr, nullptr);
        } else {
            SDL_FRect dst = compute_letterbox_rect(gg->render_dims, gg->window_dims);
            SDL_RenderCopyF(renderer, target, nullptr, &dst);
        }
    }

    if (es->draw_input_device_overlay) {
        draw_input_devices_overlay(renderer);
    }

    layout_editor_render(renderer, window_w, window_h);

    imgui_debug_render();
    imgui_render_layer();
    SDL_RenderPresent(renderer);
}
