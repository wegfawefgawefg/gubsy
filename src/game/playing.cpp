#include "game/playing.hpp"

#include "engine/audio.hpp"
#include "engine/globals.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "demo_items.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <string>
#include <engine/alerts.hpp>
#include <game/settings.hpp>
#include <game/demo_items.hpp>
#include <engine/globals.hpp>


bool overlaps(const glm::vec2& a_pos, const glm::vec2& a_half,
              const glm::vec2& b_pos, const glm::vec2& b_half) {
    glm::vec2 delta = glm::abs(a_pos - b_pos);
    return delta.x <= (a_half.x + b_half.x) && delta.y <= (a_half.y + b_half.y);
}



void step_playing() {
    const float dt = FIXED_TIMESTEP;
    auto& player = ss->player;
    auto& target = ss->bonk;

    glm::vec2 dir = movement_direction();
    player.pos += dir * player.speed_units_per_sec * dt;

    if (target.cooldown > 0.0f)
        target.cooldown = std::max(0.0f, target.cooldown - dt);

    if (target.enabled &&
        overlaps(player.pos, player.half_size, target.pos, target.half_size) &&
        target.cooldown <= 0.0f) {
        target.cooldown = BONK_COOLDOWN_SECONDS;
        add_alert("bonk!");
        const std::string sound = target.sound_key.empty() ? "base:ui_confirm" : target.sound_key;
        play_sound(sound);
    }

    const bool use_pressed = ss->playing_inputs.use_center && !ss->use_interact_prev;
    ss->use_interact_prev = ss->playing_inputs.use_center;
    if (use_pressed) {
        const float player_radius = glm::length(player.half_size);
        for (const auto& slot : demo_item_instance_slots()) {
            if (!slot.active)
                continue;
            const DemoItemInstance& inst = slot.value;
            const DemoItemDef* def = demo_item_def(inst);
            if (!def)
                continue;
            float dist = glm::length(player.pos - inst.position);
            if (dist <= (player_radius + def->radius)) {
                trigger_demo_item_use(inst);
                break;
            }
        }
    }
}


void render_instructions(SDL_Renderer* renderer, int /*width*/, int height, const std::string& text) {
    int margin = 24;
    draw_text(renderer, text, margin, height - 30, SDL_Color{200, 200, 200, 255});
}

void render_playing() {
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
    for (const auto& slot : demo_item_instance_slots()) {
        if (!slot.active)
            continue;
        const DemoItemInstance& inst = slot.value;
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
