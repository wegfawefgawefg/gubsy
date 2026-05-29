#include "render.hpp"

#include "src/engine_state.hpp"
#include "src/graphics.hpp"
#include "src/imgui_debug/imgui_debug.hpp"
#include "src/imgui_layer.hpp"
#include "src/input_sources.hpp"
#include "src/layout_editor/layout_editor.hpp"
#include "src/mode_registry.hpp"
#include "src/project_paths.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <string>
#include <variant>

namespace {

bool setting_enabled(const EngineState& engine, const char* key) {
    auto it = engine.top_level_game_settings.settings.find(key);
    if (it == engine.top_level_game_settings.settings.end())
        return false;
    if (const int* iv = std::get_if<int>(&it->second))
        return *iv != 0;
    if (const float* fv = std::get_if<float>(&it->second))
        return *fv >= 0.5f;
    return false;
}

SDL_FRect compute_letterbox_rect(const glm::uvec2& render_dims, const glm::uvec2& window_dims) {
    SDL_FRect rect{};
    if (window_dims.x == 0 || window_dims.y == 0 || render_dims.x == 0 || render_dims.y == 0) {
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

std::filesystem::path find_ui_font_path(const std::filesystem::path& fonts_dir) {
    std::error_code ec;
    if (!std::filesystem::exists(fonts_dir, ec) || !std::filesystem::is_directory(fonts_dir, ec)) {
        return {};
    }

    std::filesystem::path fallback_path;
    for (const auto& entry : std::filesystem::directory_iterator(fonts_dir, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file())
            continue;
        std::filesystem::path path = entry.path();
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".ttf" && ext != ".otf")
            continue;
        if (ext == ".ttf")
            return path;
        if (fallback_path.empty())
            fallback_path = path;
    }
    return fallback_path;
}

TTF_Font* fallback_draw_font() {
    static TTF_Font* font = []() -> TTF_Font* {
        if (!TTF_WasInit() && !TTF_Init()) {
            std::fprintf(stderr, "TTF_Init failed in draw_text: %s\n", SDL_GetError());
            return nullptr;
        }
        std::filesystem::path font_path = find_ui_font_path(engine_assets_path("fonts"));
        if (font_path.empty()) {
            std::fprintf(stderr, "No UI font found in %s\n",
                         engine_assets_path("fonts").string().c_str());
            return nullptr;
        }
        TTF_Font* loaded = TTF_OpenFont(font_path.string().c_str(), 20);
        if (!loaded) {
            std::fprintf(stderr, "TTF_OpenFont failed in draw_text: %s\n", SDL_GetError());
            return nullptr;
        }
        return loaded;
    }();
    return font;
}

void draw_text_with_font(TTF_Font* font, SDL_Renderer* renderer, const std::string& text, int x,
                         int y, SDL_Color color) {
    if (!font || !renderer || text.empty())
        return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), 0, color);
    if (!surf)
        return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (!tex)
        return;
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    float w = 0.0f;
    float h = 0.0f;
    SDL_GetTextureSize(tex, &w, &h);
    SDL_FRect dst{static_cast<float>(x), static_cast<float>(y), w, h};
    SDL_RenderTexture(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

int measure_text_width(TTF_Font* font, const std::string& text) {
    if (!font || text.empty())
        return 0;
    int w = 0;
    int h = 0;
    if (!TTF_GetStringSize(font, text.c_str(), 0, &w, &h))
        return 0;
    return w;
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

void fill_and_outline(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color fill,
                      SDL_Color border) {
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderRect(renderer, &rect);
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
    draw_text_with_font(fallback_draw_font(), renderer, text, x, y, color);
}

SDL_Color alert_color(AlertSeverity severity) {
    switch (severity) {
    case AlertSeverity::Success:
        return SDL_Color{130, 235, 155, 255};
    case AlertSeverity::Warning:
        return SDL_Color{245, 205, 105, 255};
    case AlertSeverity::Error:
        return SDL_Color{245, 115, 125, 255};
    case AlertSeverity::Debug:
        return SDL_Color{185, 165, 235, 255};
    case AlertSeverity::Info:
    default:
        return SDL_Color{210, 230, 255, 255};
    }
}

void render_alerts(const EngineState& engine, SDL_Renderer* renderer, int width) {
    if (engine.alerts.empty())
        return;
    Graphics* graphics = engine.graphics;
    if (!graphics)
        return;
    TTF_Font* font = graphics->ui_font ? graphics->ui_font : fallback_draw_font();
    if (!font)
        return;

    SDL_BlendMode previous_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previous_blend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int y = 18;
    for (const auto& alert : engine.alerts) {
        constexpr int kPaddingX = 14;
        constexpr int kPaddingY = 7;
        constexpr int kToastHeight = 34;
        const int text_width = measure_text_width(font, alert.text);
        const int toast_width = std::clamp(text_width + kPaddingX * 2, 220, std::max(220, width - 48));
        const int x = std::max(12, (width - toast_width) / 2);
        SDL_FRect rect{static_cast<float>(x),
                       static_cast<float>(y),
                       static_cast<float>(toast_width),
                       static_cast<float>(kToastHeight)};
        SDL_SetRenderDrawColor(renderer, 16, 18, 26, 205);
        SDL_RenderFillRect(renderer, &rect);
        SDL_Color edge = alert_color(alert.severity);
        SDL_SetRenderDrawColor(renderer, edge.r, edge.g, edge.b, 230);
        SDL_RenderRect(renderer, &rect);
        draw_text_with_font(font, renderer, alert.text, x + kPaddingX, y + kPaddingY,
                            alert_color(alert.severity));
        y += kToastHeight + 6;
        if (y > 260)
            break;
    }

    SDL_SetRenderDrawBlendMode(renderer, previous_blend);
}

void render_fps_counter(const EngineState& engine, SDL_Renderer* renderer) {
    if (!setting_enabled(engine, "gubsy.video.show_fps"))
        return;
    const Graphics* graphics = current_graphics(engine);
    if (!graphics)
        return;
    draw_text_with_font(graphics->ui_font ? graphics->ui_font : fallback_draw_font(), renderer,
                        "FPS: " + std::to_string(engine.displayed_fps), 16, 14,
                        SDL_Color{235, 245, 210, 255});
}

bool render_frame_to_window(EngineState& engine) {
    Graphics* graphics = current_graphics(engine);
    if (!graphics || !graphics->renderer || !graphics->render_target)
        return false;

    SDL_Renderer* renderer = graphics->renderer;
    SDL_Texture* target = graphics->render_target;

    SDL_SetRenderTarget(renderer, nullptr);
    int window_w = 0;
    int window_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &window_w, &window_h);
    graphics->window_dims = {static_cast<unsigned int>(std::max(window_w, 1)),
                             static_cast<unsigned int>(std::max(window_h, 1))};

    SDL_SetRenderDrawColor(renderer, 5, 5, 10, 255);
    SDL_RenderClear(renderer);

    SDL_FRect drawn_rect{0.0f, 0.0f, static_cast<float>(window_w), static_cast<float>(window_h)};
    if (graphics->render_scale_mode == RenderScaleMode::Stretch) {
        SDL_RenderTexture(renderer, target, nullptr, nullptr);
    } else {
        SDL_FRect dst = compute_letterbox_rect(graphics->render_dims, graphics->window_dims);
        const glm::vec4 safe = graphics->safe_area;
        float pad_left = std::clamp(safe.x, 0.0f, 0.45f) * static_cast<float>(window_w);
        float pad_right = std::clamp(safe.y, 0.0f, 0.45f) * static_cast<float>(window_w);
        float pad_top = std::clamp(safe.z, 0.0f, 0.45f) * static_cast<float>(window_h);
        float pad_bottom = std::clamp(safe.w, 0.0f, 0.45f) * static_cast<float>(window_h);
        dst.x += pad_left;
        dst.y += pad_top;
        dst.w = std::max(4.0f, dst.w - (pad_left + pad_right));
        dst.h = std::max(4.0f, dst.h - (pad_top + pad_bottom));

        float zoom = std::max(0.1f, graphics->preview_zoom);
        float cx = dst.x + dst.w * 0.5f;
        float cy = dst.y + dst.h * 0.5f;
        float new_w = dst.w * zoom;
        float new_h = dst.h * zoom;
        dst.x = cx - new_w * 0.5f + graphics->preview_pan.x;
        dst.y = cy - new_h * 0.5f + graphics->preview_pan.y;
        dst.w = new_w;
        dst.h = new_h;

        SDL_RenderTexture(renderer, target, nullptr, &dst);
        drawn_rect = dst;
    }

    graphics->present_rect = drawn_rect;
    render_fps_counter(engine, renderer);
    return true;
}

void present_frame(EngineState& engine) {
    Graphics* graphics = current_graphics(engine);
    if (graphics && graphics->renderer)
        SDL_RenderPresent(graphics->renderer);
}

void render(EngineState& engine) {
    SDL_Renderer* renderer =
        (current_graphics(engine) ? current_graphics(engine)->renderer : nullptr);
    if (!renderer)
        return;

    SDL_Texture* target =
        (current_graphics(engine) ? current_graphics(engine)->render_target : nullptr);
    if (target)
        SDL_SetRenderTarget(renderer, target);

    if (const ModeDesc* mode = find_mode(engine, engine.mode)) {
        if (mode->render_fn)
            mode->render_fn(engine, engine.app_context);
    }

    if (target)
        SDL_SetRenderTarget(renderer, nullptr);

    render_frame_to_window(engine);
    int window_w =
        current_graphics(engine) ? static_cast<int>(current_graphics(engine)->window_dims.x) : 0;
    SDL_FRect drawn_rect = current_graphics(engine) ? current_graphics(engine)->present_rect
                                                    : SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};

    render_alerts(engine, renderer, window_w);

    if (layout_editor_is_active(engine)) {
        int overlay_w = std::max(0, static_cast<int>(std::round(drawn_rect.w)));
        int overlay_h = std::max(0, static_cast<int>(std::round(drawn_rect.h)));
        layout_editor_render(engine, renderer, overlay_w, overlay_h, drawn_rect.x, drawn_rect.y);
    }

    if (engine.draw_input_device_overlay) {
        draw_input_devices_overlay(engine, renderer);
    }

    imgui_debug_render(engine);
    imgui_render_layer();
    present_frame(engine);
}
