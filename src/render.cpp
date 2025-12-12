#include "render.hpp"

#include "engine/mode_registry.hpp"
#include "engine/graphics.hpp"
#include "globals.hpp"
#include "settings.hpp"
#include "menu/menu.hpp"
#include "state.hpp"
#include "demo_items.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <glm/glm.hpp>

namespace {
struct ScreenSpace {
    float scale{64.0f};
    float cx{0.0f};
    float cy{0.0f};
};

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

SDL_Color color_from_vec3(const glm::vec3& v, Uint8 alpha = 255) {
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
    if (!ss || ss->alerts.empty())
        return;
    int y = 20;
    for (const auto& alert : ss->alerts) {
        draw_text(renderer, alert.text, 24, y, SDL_Color{255, 235, 160, 255});
        y += 22;
        if (y > 200)
            break;
    }

    // Mode label
    std::string mode = "Mode: " + ss->mode;
    draw_text(renderer, mode, width - 220, 20, SDL_Color{180, 180, 200, 255});
}

void render_instructions(SDL_Renderer* renderer, int /*width*/, int height, const std::string& text) {
    int margin = 24;
    draw_text(renderer, text, margin, height - 30, SDL_Color{200, 200, 200, 255});
}

void render_playing_frame() {
    if (!gg || !gg->renderer || !ss) {
        SDL_Delay(16);
        return;
    }

    SDL_Renderer* renderer = gg->renderer;
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_SetRenderDrawColor(renderer, 12, 10, 18, 255);
    SDL_RenderClear(renderer);

    ScreenSpace space = make_space(width, height);

    // Grid backdrop
    SDL_SetRenderDrawColor(renderer, 25, 24, 35, 255);
    const int grid = 40;
    for (int x = 0; x < width; x += grid) {
        SDL_RenderDrawLine(renderer, x, 0, x, height);
    }
    for (int y = 0; y < height; y += grid) {
        SDL_RenderDrawLine(renderer, 0, y, width, y);
    }

    const auto& player = ss->player;
    const auto& target = ss->bonk;

    SDL_Color player_fill{80, 200, 255, 255};
    SDL_Color player_border{15, 40, 70, 255};
    SDL_Color target_border{50, 20, 10, 255};
    SDL_Color target_fill{230, 190, 90, 255};
    if (target.cooldown > 0.0f) {
        float t = std::clamp(target.cooldown / BONK_COOLDOWN_SECONDS, 0.0f, 1.0f);
        target_fill = SDL_Color{
            static_cast<Uint8>(180 + 60 * t),
            static_cast<Uint8>(90 + 40 * t),
            static_cast<Uint8>(40 + 50 * t),
            255};
    }

    SDL_FRect player_rect = rect_for(player.pos, player.half_size, space);
    fill_and_outline(renderer, player_rect, player_fill, player_border);

    SDL_FRect target_rect = rect_for(target.pos, target.half_size, space);
    if (target.enabled) {
        fill_and_outline(renderer, target_rect, target_fill, target_border);
    } else {
        SDL_Color disabled{60, 50, 40, 180};
        SDL_SetRenderDrawColor(renderer, disabled.r, disabled.g, disabled.b, disabled.a);
        SDL_RenderDrawRectF(renderer, &target_rect);
    }

    std::string nearby_label;
    const float player_radius = glm::length(player.half_size);
    for (const auto& inst : demo_item_instances()) {
        if (!inst.active)
            continue;
        const DemoItemDef* item = demo_item_def(inst);
        if (!item)
            continue;
        SDL_FRect item_rect = rect_for(inst.position,
                                       glm::vec2(item->radius, item->radius), space);
        float dist = glm::length(player.pos - inst.position);
        bool nearby = dist <= (player_radius + item->radius + 0.1f);
        bool drew_sprite = false;
        if (item->sprite_id >= 0) {
            if (SDL_Texture* tex = get_texture(item->sprite_id)) {
                SDL_RenderCopyF(renderer, tex, nullptr, &item_rect);
                drew_sprite = true;
            }
        }
        if (!drew_sprite) {
            glm::vec3 fill_vec = nearby ? brighten(item->color, 0.15f) : item->color;
            glm::vec3 border_vec = nearby ? glm::vec3(1.0f, 0.95f, 0.7f)
                                          : brighten(item->color, 0.05f);
            fill_and_outline(renderer, item_rect,
                             color_from_vec3(fill_vec),
                             color_from_vec3(border_vec));
        } else if (nearby) {
            SDL_Color border{255, 240, 180, 255};
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
            SDL_RenderDrawRectF(renderer, &item_rect);
        }
        draw_text(renderer, item->label, static_cast<int>(item_rect.x),
                  static_cast<int>(item_rect.y) - 18, SDL_Color{200, 200, 220, 255});
        if (nearby && nearby_label.empty())
            nearby_label = item->label;
    }

    // Alerts + instructions overlay
    render_alerts(renderer, width);
    std::string prompt_text;
    if (!nearby_label.empty())
        prompt_text = "Press Space/E to use " + nearby_label;
    else
        prompt_text = "Move with WASD. Press Space/E near a pad to run its Lua-defined action.";
    render_instructions(renderer, width, height, prompt_text);

    SDL_RenderPresent(renderer);
}

} // namespace

void render() {
    if (!ss)
        return;
    if (const ModeDesc* mode = find_mode(ss->mode)) {
        if (mode->render_fn) {
            mode->render_fn();
            return;
        }
    }
    render_mode_playing();
}

void render_mode_playing() {
    render_playing_frame();
}

void render_mode_title() {
    if (!gg || !gg->renderer) {
        SDL_Delay(16);
        return;
    }
    SDL_Renderer* renderer = gg->renderer;
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_SetRenderDrawColor(renderer, 12, 10, 18, 255);
    SDL_RenderClear(renderer);
    render_menu(width, height);
    SDL_RenderPresent(renderer);
}
