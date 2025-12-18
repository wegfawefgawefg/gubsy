#include "game/playing.hpp"

#include "engine/audio.hpp"
#include "engine/globals.hpp"
#include "engine/input_queries.hpp"
#include "engine/input_binding_utils.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"
#include "game/actions.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "demo_items.hpp"
#include "game/ui_layout_ids.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <string>
#include <engine/alerts.hpp>
#include <game/settings.hpp>
#include <game/demo_items.hpp>
#include <engine/globals.hpp>
#include <engine/render.hpp>


bool overlaps(const glm::vec2& a_pos, const glm::vec2& a_half,
              const glm::vec2& b_pos, const glm::vec2& b_half) {
    glm::vec2 delta = glm::abs(a_pos - b_pos);
    return delta.x <= (a_half.x + b_half.x) && delta.y <= (a_half.y + b_half.y);
}



void playing_step() {
    const float dt = FIXED_TIMESTEP;
    auto& target = ss->bonk;

    // Update things that happen once per frame
    if (target.cooldown > 0.0f)
        target.cooldown = std::max(0.0f, target.cooldown - dt);

    // Update analog inputs (for player 0 for now)
    if (!ss->players.empty()) {
        float trigger = get_1d_analog(0, GameAnalog1D::BAR_HEIGHT);
        ss->bar_height = std::clamp(trigger, 0.0f, 1.0f);

        glm::vec2 stick = get_2d_analog(0, GameAnalog2D::RETICLE_POS);
        ss->reticle_pos = glm::clamp(stick, glm::vec2(-1.0f), glm::vec2(1.0f));
    } else {
        ss->reticle_pos = glm::vec2(0.0f);
    }

    if (es) {
        glm::vec2 raw_gamepad =
            sample_analog_2d(es->device_state, static_cast<int>(Gubsy2DAnalog::GP_LEFT_STICK));
        ss->reticle_pos_gamepad = glm::clamp(raw_gamepad, glm::vec2(-1.0f), glm::vec2(1.0f));
        ss->reticle_pos_mouse = normalized_mouse_coords(es->device_state);
    } else {
        ss->reticle_pos_gamepad = glm::vec2(0.0f);
        ss->reticle_pos_mouse = glm::vec2(0.0f);
    }

    // Update each player
    for (std::size_t i = 0; i < ss->players.size(); ++i) {
        auto& player = ss->players[i];
        const int player_index = static_cast<int>(i);
        
        // Handle player input and movement
        glm::vec2 dir(0.0f);
        if (is_down(player_index, GameAction::UP)) {
            dir.y -= 1.0f;
        }
        if (is_down(player_index, GameAction::DOWN)) {
            dir.y += 1.0f;
        }
        if (is_down(player_index, GameAction::LEFT)) {
            dir.x -= 1.0f;
        }
        if (is_down(player_index, GameAction::RIGHT)) {
            dir.x += 1.0f;
        }
        if (glm::length(dir) > 0.0f) {
            dir = glm::normalize(dir);
        }
        player.pos += dir * player.speed_units_per_sec * dt;

        // Handle player-specific interactions
        if (target.enabled &&
            overlaps(player.pos, player.half_size, target.pos, target.half_size) &&
            target.cooldown <= 0.0f) {
            target.cooldown = BONK_COOLDOWN_SECONDS;
            add_alert("bonk!");
            const std::string sound = target.sound_key.empty() ? "base:ui_confirm" : target.sound_key;
            play_sound(sound);
        }

        const bool use_pressed = was_pressed(player_index, GameAction::USE);
        if (use_pressed) {
            std::printf("[playing] player %d pressed USE\n", player_index);
            const float player_radius = glm::length(player.half_size);
            bool triggered = false;
            for (const auto& slot : demo_item_instance_slots()) {
                if (!slot.active)
                    continue;
                const DemoItemInstance& inst = slot.value;
                const DemoItemDef* def = demo_item_def(inst);
                if (!def)
                    continue;
                float dist = glm::length(player.pos - inst.position);
                std::printf("[playing]   candidate '%s' dist=%.2f radius=%.2f\n",
                            def->id.c_str(), static_cast<double>(dist),
                            static_cast<double>(player_radius + def->radius));
                if (dist <= (player_radius + def->radius)) {
                    std::printf("[playing] player %d triggering '%s' (dist=%.2f)\n",
                                player_index, def->id.c_str(),
                                static_cast<double>(dist));
                    trigger_demo_item_use(inst);
                    triggered = true;
                    break; // One player uses it, break for this player
                }
            }
            if (!triggered) {
                std::printf("[playing] player %d USE ignored (no pads in range)\n",
                            player_index);
            }
        }
    }
}


void render_instructions(SDL_Renderer* renderer, int /*width*/, int height, const std::string& text) {
    int margin = 24;
    draw_text(renderer, text, margin, height - 30, SDL_Color{200, 200, 200, 255});
}

void playing_draw() {
    if (!gg || !gg->renderer || !ss) {
        SDL_Delay(16);
        return;
    }

    SDL_Renderer* renderer = gg->renderer;
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    const float width_f = static_cast<float>(width);
    const float height_f = static_cast<float>(height);
    const float width_span = static_cast<float>(std::max(1, width - 1));
    const float height_span = static_cast<float>(std::max(1, height - 1));
    SDL_SetRenderDrawColor(renderer, 12, 10, 18, 255);
    SDL_RenderClear(renderer);

    // Get best matching UI layout for current resolution
    const UILayout* layout = get_ui_layout_for_resolution(UILayoutID::PLAY_SCREEN, width, height);

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

    const auto& target = ss->bonk;

    // Draw players
    for (std::size_t i = 0; i < ss->players.size(); ++i) {
        const auto& player = ss->players[i];
        // Cycle through some colors for each player
        SDL_Color player_fill = (i % 2 == 0) ? SDL_Color{80, 200, 255, 255} : SDL_Color{255, 180, 80, 255};
        SDL_Color player_border = (i % 2 == 0) ? SDL_Color{15, 40, 70, 255} : SDL_Color{70, 40, 15, 255};
        
        SDL_FRect player_rect = rect_for(player.pos, player.half_size, space);
        fill_and_outline(renderer, player_rect, player_fill, player_border);
    }

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

    SDL_FRect target_rect = rect_for(target.pos, target.half_size, space);
    if (target.enabled) {
        fill_and_outline(renderer, target_rect, target_fill, target_border);
    } else {
        SDL_Color disabled{60, 50, 40, 180};
        SDL_SetRenderDrawColor(renderer, disabled.r, disabled.g, disabled.b, disabled.a);
        SDL_RenderDrawRectF(renderer, &target_rect);
    }

    std::string nearby_label;
    // TODO: The nearby check only works for player 0 right now.
    if (!ss->players.empty()) {
        const auto& player = ss->players[0];
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
    }

    // Draw UI using layout system
    if (layout) {
        // Draw bar height indicator
        if (const UIObject* bar_obj = get_ui_object(*layout, UIObjectID::BAR_HEIGHT_INDICATOR)) {
            float bar_x = bar_obj->x * width_f;
            float bar_y = bar_obj->y * height_f;
            float bar_width = bar_obj->w * width_f;
            float bar_height = bar_obj->h * height_f;
            float bar_current_height = bar_height * ss->bar_height;

            // Background
            SDL_FRect bar_bg{bar_x, bar_y, bar_width, bar_height};
            SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
            SDL_RenderFillRectF(renderer, &bar_bg);

            // Filled portion (from bottom)
            SDL_FRect bar_fill{
                bar_x,
                bar_y + bar_height - bar_current_height,
                bar_width,
                bar_current_height};
            SDL_SetRenderDrawColor(renderer, 120, 200, 100, 255);
            SDL_RenderFillRectF(renderer, &bar_fill);

            // Border
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRectF(renderer, &bar_bg);
        }
    }

    // Draw reticles (gamepad crosshair + mouse marker)
    {
        // Gamepad-driven crosshair
        glm::vec2 gp = ss->reticle_pos_gamepad;
        float reticle_screen_x = (gp.x * 0.5f + 0.5f) * width_span;
        float reticle_screen_y = (gp.y * 0.5f + 0.5f) * height_span;

        float reticle_size = 10.0f;
        SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);

        // Horizontal line
        SDL_RenderDrawLineF(renderer,
            reticle_screen_x - reticle_size, reticle_screen_y,
            reticle_screen_x + reticle_size, reticle_screen_y);

        // Vertical line
        SDL_RenderDrawLineF(renderer,
            reticle_screen_x, reticle_screen_y - reticle_size,
            reticle_screen_x, reticle_screen_y + reticle_size);

        // Center dot
        SDL_FRect center_dot{reticle_screen_x - 2.0f, reticle_screen_y - 2.0f, 4.0f, 4.0f};
        SDL_RenderFillRectF(renderer, &center_dot);

        // Mouse pointer marker (little green circle)
        glm::vec2 mouse = ss->reticle_pos_mouse;
        float mouse_screen_x = (mouse.x * 0.5f + 0.5f) * width_span;
        float mouse_screen_y = (mouse.y * 0.5f + 0.5f) * height_span;
        SDL_SetRenderDrawColor(renderer, 120, 255, 120, 200);
        SDL_FRect mouse_dot{mouse_screen_x - 4.0f, mouse_screen_y - 4.0f, 8.0f, 8.0f};
        SDL_RenderFillRectF(renderer, &mouse_dot);
        SDL_SetRenderDrawColor(renderer, 40, 150, 60, 255);
        SDL_RenderDrawRectF(renderer, &mouse_dot);
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
