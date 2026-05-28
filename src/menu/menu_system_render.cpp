#include "src/menu/menu_system.hpp"

#include "src/engine_state.hpp"
#include "src/menu_layout_ids.hpp"
#include "src/menu/menu_system_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace msi = menu_system_internal;

namespace {

void draw_nav_button(SDL_Renderer* renderer,
                     const SDL_FRect& btn_rect,
                     bool left,
                     const MenuWidget& widget) {
    SDL_Color btn_bg{static_cast<Uint8>(widget.style.bg_r + 12),
                     static_cast<Uint8>(widget.style.bg_g + 14),
                     static_cast<Uint8>(widget.style.bg_b + 18),
                     255};
    SDL_Color btn_border{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255};
    SDL_SetRenderDrawColor(renderer, btn_bg.r, btn_bg.g, btn_bg.b, btn_bg.a);
    SDL_RenderFillRect(renderer, &btn_rect);
    SDL_SetRenderDrawColor(renderer, btn_border.r, btn_border.g, btn_border.b, btn_border.a);
    SDL_RenderRect(renderer, &btn_rect);
    float cx = btn_rect.x + btn_rect.w * 0.5f;
    float cy = btn_rect.y + btn_rect.h * 0.5f;
    float wing = btn_rect.h * 0.32f;
    float head = btn_rect.w * 0.22f;
    float dir = left ? -1.0f : 1.0f;
    SDL_FPoint tip{cx + dir * head, cy};
    SDL_FPoint wing_top{cx - dir * head * 0.4f, cy - wing};
    SDL_FPoint wing_bottom{cx - dir * head * 0.4f, cy + wing};
    SDL_RenderLine(renderer, tip.x, tip.y, wing_top.x, wing_top.y);
    SDL_RenderLine(renderer, tip.x, tip.y, wing_bottom.x, wing_bottom.y);
    SDL_RenderLine(renderer, wing_top.x, wing_top.y, wing_bottom.x, wing_bottom.y);
}

std::vector<std::string> wrap_text_lines(const EngineState& engine, const char* text, int max_width) {
    std::vector<std::string> lines;
    if (!text || !*text)
        return lines;
    if (max_width <= 0) {
        lines.emplace_back(text);
        return lines;
    }

    std::istringstream paragraph_stream(text);
    std::string paragraph;
    while (std::getline(paragraph_stream, paragraph)) {
        if (paragraph.empty()) {
            lines.emplace_back();
            continue;
        }
        std::istringstream stream(paragraph);
        std::string word;
        std::string line;
        while (stream >> word) {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && msi::measure_text_width(engine, candidate.c_str()) > max_width) {
                lines.push_back(line);
                line = word;
            } else {
                line = std::move(candidate);
            }
        }
        if (!line.empty())
            lines.push_back(line);
    }
    return lines;
}

bool is_status_slot(UILayoutObjectId slot) {
    return slot == SettingsObjectID::STATUS || slot == SettingsObjectID::STATUS_RIGHT ||
           slot == ModsObjectID::STATUS;
}

void draw_aligned_text(const EngineState& engine,
                       SDL_Renderer* renderer,
                       const char* text,
                       int x,
                       int y,
                       int max_width,
                       SDL_Color color,
                       const SDL_Rect* clip,
                       bool right_align) {
    int draw_x = x;
    if (right_align && max_width > 0)
        draw_x = x + max_width - msi::measure_text_width(engine, text);
    msi::draw_text_with_clip(engine, renderer, text, draw_x, y, color, clip);
}

void draw_widget_text(const EngineState& engine,
                      SDL_Renderer* renderer,
                      const char* text,
                      int x,
                      int& y,
                      int max_width,
                      SDL_Color color,
                      const SDL_Rect* clip,
                      bool right_align,
                      bool wrap_text) {
    if (!text)
        return;
    if (wrap_text) {
        for (const std::string& line : wrap_text_lines(engine, text, max_width)) {
            draw_aligned_text(engine, renderer, line.c_str(), x, y, max_width, color, clip, right_align);
            y += 22;
        }
    } else {
        draw_aligned_text(engine, renderer, text, x, y, max_width, color, clip, right_align);
        y += 22;
    }
}

void draw_text_input(const EngineState& engine,
                     const msi::MenuRuntimeState& state,
                     SDL_Renderer* renderer,
                     const SDL_FRect& rect,
                     const MenuWidget& widget,
                     const std::string* buffer,
                     const char* placeholder,
                     bool editing) {
    SDL_Color input_bg{24, 26, 36, 255};
    SDL_Color input_border{80, 90, 110, 255};
    if (editing)
        input_border = SDL_Color{widget.style.focus_r, widget.style.focus_g, widget.style.focus_b, 255};
    SDL_SetRenderDrawColor(renderer, input_bg.r, input_bg.g, input_bg.b, input_bg.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, input_border.r, input_border.g, input_border.b, input_border.a);
    SDL_RenderRect(renderer, &rect);
    std::string display;
    if (buffer && !buffer->empty())
        display = *buffer;
    else if (placeholder)
        display = placeholder;
    SDL_Rect input_clip{
        static_cast<int>(rect.x) + 4,
        static_cast<int>(rect.y) + 1,
        std::max(0, static_cast<int>(rect.w) - 8),
        std::max(0, static_cast<int>(rect.h) - 2)};
    menu_system_internal::draw_text_with_clip(engine,
                                              renderer,
                                              display.c_str(),
                                              static_cast<int>(rect.x) + 6,
                                              static_cast<int>(rect.y) + 2,
                                              SDL_Color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255},
                                              &input_clip);
    if (editing && state.text_edit_active) {
        bool editing_this = state.text_edit_widget == widget.id &&
                            ((state.text_edit_using_aux && buffer == widget.aux_text_buffer) ||
                             (!state.text_edit_using_aux && buffer == widget.text_buffer));
        if (editing_this && std::fmod(state.caret_time, 1.0f) < 0.5f && buffer) {
            int caret_x = static_cast<int>(rect.x) + 6 +
                          menu_system_internal::measure_text_width(engine, buffer->c_str());
            if (caret_x > static_cast<int>(rect.x + rect.w) - 6)
                caret_x = static_cast<int>(rect.x + rect.w) - 6;
            int caret_top = static_cast<int>(rect.y) + 2;
            int caret_bottom = static_cast<int>(rect.y + rect.h) - 2;
            SDL_SetRenderDrawColor(renderer, widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255);
            SDL_RenderLine(renderer, static_cast<float>(caret_x), static_cast<float>(caret_top),
                           static_cast<float>(caret_x), static_cast<float>(caret_bottom));
        }
    }
}

} // namespace

void menu_system_render(EngineState& engine, SDL_Renderer* renderer, int screen_width, int screen_height) {
    msi::MenuRuntimeState& state = msi::runtime_state(engine);
    if ((state.cache.width != screen_width || state.cache.height != screen_height) &&
        screen_width > 0 && screen_height > 0) {
        MenuInputState zero_input{};
        state.prev_input = state.current_input;
        state.current_input = zero_input;
        menu_system_update(engine, 0.0f, screen_width, screen_height);
    }
    if (!renderer || state.cache.widgets.empty())
        return;

    const UILayout* layout = get_ui_layout_for_resolution(engine,
                                                          static_cast<int>(state.cache.layout),
                                                          state.cache.width,
                                                          state.cache.height);
    for (std::size_t i = 0; i < state.cache.widgets.size(); ++i) {
        const MenuWidget& widget = state.cache.widgets[i];
        SDL_FRect rect;
        if (i < state.cache.rects.size())
            rect = state.cache.rects[i];
        else {
            const UIObject* obj = layout ? get_ui_object(*layout, static_cast<int>(widget.slot)) : nullptr;
            if (obj)
                rect = msi::rect_from_object(*obj, state.cache.width, state.cache.height);
            else
                rect = SDL_FRect{
                    static_cast<float>(state.cache.width) * 0.3f,
                    static_cast<float>(state.cache.height) * 0.3f,
                    static_cast<float>(state.cache.width) * 0.4f,
                    60.0f};
        }

        bool draw_background = widget.type != WidgetType::Label;
        if (draw_background) {
            SDL_Color bg{widget.style.bg_r, widget.style.bg_g, widget.style.bg_b, widget.style.bg_a};
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(renderer, &rect);

            SDL_Color border{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, widget.style.fg_a};
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
            SDL_RenderRect(renderer, &rect);

            if (widget.id == state.focus) {
                SDL_Color focus{widget.style.focus_r, widget.style.focus_g, widget.style.focus_b, widget.style.focus_a};
                auto adjust = [](Uint8 base, int delta) -> Uint8 {
                    return static_cast<Uint8>(std::clamp(static_cast<int>(base) + delta, 0, 255));
                };
                int luma = static_cast<int>(0.2126f * widget.style.bg_r +
                                            0.7152f * widget.style.bg_g +
                                            0.0722f * widget.style.bg_b);
                int delta = (luma > 150) ? -18 : 22;
                SDL_Color focus_overlay{
                    adjust(widget.style.bg_r, delta),
                    adjust(widget.style.bg_g, delta),
                    adjust(widget.style.bg_b, delta),
                    70};
                SDL_SetRenderDrawColor(renderer, focus_overlay.r, focus_overlay.g, focus_overlay.b, focus_overlay.a);
                SDL_RenderFillRect(renderer, &rect);

                SDL_FRect outline = rect;
                outline.x -= 2.0f;
                outline.y -= 2.0f;
                outline.w += 4.0f;
                outline.h += 4.0f;
                SDL_SetRenderDrawColor(renderer, focus.r, focus.g, focus.b, focus.a);
                SDL_RenderRect(renderer, &outline);

                SDL_FRect inner = rect;
                inner.x += 1.0f;
                inner.y += 1.0f;
                inner.w -= 2.0f;
                inner.h -= 2.0f;
                SDL_Color inner_col{
                    adjust(focus.r, 10),
                    adjust(focus.g, 10),
                    adjust(focus.b, 10),
                    focus.a};
                SDL_SetRenderDrawColor(renderer, inner_col.r, inner_col.g, inner_col.b, inner_col.a);
                SDL_RenderRect(renderer, &inner);
    }

}

        bool has_slider_visual = widget.type == WidgetType::Slider1D;
        msi::SliderLayout slider_visual{};
        if (has_slider_visual)
            slider_visual = msi::compute_slider_layout(widget, rect);
        bool slider_has_input = has_slider_visual && slider_visual.has_input;
        bool drew_option_value = false;

        std::string text_storage;
        std::string text_input_storage;
        const char* text_ptr = widget.label;
        SDL_Color text_color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255};
        const char* text_input_value_ptr = nullptr;
        SDL_Color text_input_color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255};
        bool is_text_input_widget = widget.type == WidgetType::TextInput && widget.text_buffer;
        if (is_text_input_widget) {
            if (widget.label == nullptr) {
                if (widget.text_buffer->empty() && widget.placeholder) {
                    text_ptr = widget.placeholder;
                    text_color = SDL_Color{160, 160, 180, 255};
                } else {
                    text_storage = *widget.text_buffer;
                    text_ptr = text_storage.c_str();
                }
            } else {
                if (widget.text_buffer->empty() && widget.placeholder) {
                    text_input_value_ptr = widget.placeholder;
                    text_input_color = SDL_Color{160, 160, 180, 255};
                } else {
                    text_input_storage = *widget.text_buffer;
                    text_input_value_ptr = text_input_storage.c_str();
                }
            }
        }

        bool is_label = widget.type == WidgetType::Label;
        SDL_Rect clip{
            static_cast<int>(rect.x) + 8,
            static_cast<int>(rect.y) + 4,
            std::max(0, static_cast<int>(rect.w) - 16 -
                            (slider_has_input ? static_cast<int>(slider_visual.input_rect.w) + 12 : 0)),
            std::max(0, static_cast<int>(rect.h) - 8)};
        const SDL_Rect* clip_ptr = is_label ? nullptr : &clip;
        int line_x = static_cast<int>(rect.x) + (is_label ? 0 : 16);
        int line_y = static_cast<int>(rect.y) + (is_label ? 0 : 6);
        int text_input_value_y = 0;
        int text_width = is_label ? std::max(0, static_cast<int>(rect.w))
                                  : std::max(0, clip.w);
        const bool right_align_text = widget.right_align || is_status_slot(widget.slot);
        const bool wrap_widget_text = widget.wrap_text || is_status_slot(widget.slot);

        if (text_ptr) {
            int before_y = line_y;
            draw_widget_text(engine,
                             renderer,
                             text_ptr,
                             line_x,
                             line_y,
                             text_width,
                             text_color,
                             clip_ptr,
                             right_align_text,
                             wrap_widget_text);
            if (is_text_input_widget && !widget.label) {
                text_input_value_y = before_y;
            }
        }
        if (widget.secondary) {
            SDL_Color sec_color{static_cast<Uint8>(widget.style.fg_r / 2 + 50),
                                static_cast<Uint8>(widget.style.fg_g / 2 + 50),
                                static_cast<Uint8>(widget.style.fg_b / 2 + 50),
                                255};
            draw_widget_text(engine,
                             renderer,
                             widget.secondary,
                             line_x,
                             line_y,
                             text_width,
                             sec_color,
                             clip_ptr,
                             right_align_text,
                             wrap_widget_text);
        }
        if (is_text_input_widget && widget.label != nullptr && text_input_value_ptr) {
            text_input_value_y = line_y;
            draw_widget_text(engine,
                             renderer,
                             text_input_value_ptr,
                             line_x,
                             line_y,
                             text_width,
                             text_input_color,
                             clip_ptr,
                             right_align_text,
                             false);
        }
        if (widget.tertiary) {
            SDL_Color tert_color{static_cast<Uint8>(widget.style.fg_r / 3 + 60),
                                 static_cast<Uint8>(widget.style.fg_g / 3 + 60),
                                 static_cast<Uint8>(widget.style.fg_b / 3 + 60),
                                 255};
            if (widget.tertiary_overlay) {
                int text_w = msi::measure_text_width(engine, widget.tertiary);
                int overlay_x =
                    std::max(static_cast<int>(rect.x) + 16, static_cast<int>(rect.x + rect.w) - text_w - 12);
                int overlay_y = static_cast<int>(rect.y) + 6;
                msi::draw_text_with_clip(engine, renderer, widget.tertiary, overlay_x, overlay_y, tert_color, nullptr);
            } else {
                msi::draw_text_with_clip(engine, renderer, widget.tertiary, line_x, line_y, tert_color, clip_ptr);
            }
        }
        if (is_text_input_widget && state.text_edit_active && widget.id == state.text_edit_widget) {
            int input_y = text_input_value_y > 0 ? text_input_value_y : (static_cast<int>(rect.y) + 6);
            if (std::fmod(state.caret_time, 1.0f) < 0.5f) {
                int caret_x = line_x + msi::measure_text_width(engine, widget.text_buffer->c_str());
                int caret_top = input_y + 1;
                int caret_bottom = caret_top + 19;
                SDL_SetRenderDrawColor(renderer, widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255);
                SDL_RenderLine(renderer, static_cast<float>(caret_x), static_cast<float>(caret_top),
                               static_cast<float>(caret_x), static_cast<float>(caret_bottom));
            }
        }

        if (has_slider_visual && widget.bind_ptr && widget.show_slider_track) {
            float value = *reinterpret_cast<float*>(widget.bind_ptr);
            float range = widget.max - widget.min;
            float norm = (range > 0.0f) ? (value - widget.min) / range : 0.0f;
            norm = std::clamp(norm, 0.0f, 1.0f);
            float track_width = slider_visual.track_right - slider_visual.track_left;
            SDL_FRect track{slider_visual.track_left, slider_visual.track_y - 2.0f, track_width, 4.0f};
            SDL_SetRenderDrawColor(renderer, 55, 60, 78, 255);
            SDL_RenderFillRect(renderer, &track);
            SDL_FRect fill = track;
            fill.w = track_width * norm;
            SDL_SetRenderDrawColor(renderer, 130, 185, 255, 255);
            SDL_RenderFillRect(renderer, &fill);
            float knob_x = slider_visual.track_left + track_width * norm;
            SDL_FRect knob{knob_x - 6.0f, slider_visual.track_y - 9.0f, 12.0f, 18.0f};
            SDL_SetRenderDrawColor(renderer, 235, 238, 245, 255);
            SDL_RenderFillRect(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 30, 35, 46, 255);
            SDL_RenderRect(renderer, &knob);

            if (slider_has_input) {
                draw_text_input(engine,
                                state,
                                renderer,
                                slider_visual.input_rect,
                                widget,
                                widget.text_buffer,
                                widget.placeholder,
                                state.text_edit_active && widget.id == state.text_edit_widget &&
                                    !state.text_edit_using_aux);
            }
            if (slider_visual.has_buttons) {
                draw_nav_button(renderer, slider_visual.left_btn, true, widget);
                draw_nav_button(renderer, slider_visual.right_btn, false, widget);
            }
        }
        if (widget.type == WidgetType::OptionCycle) {
            msi::OptionLayout opt_layout = msi::compute_option_layout(widget, rect);
            draw_nav_button(renderer, opt_layout.left_btn, true, widget);
            draw_nav_button(renderer, opt_layout.right_btn, false, widget);
            if (widget.badge) {
                SDL_Rect value_clip{
                    static_cast<int>(opt_layout.value_rect.x) + 6,
                    static_cast<int>(opt_layout.value_rect.y) + 1,
                    std::max(0, static_cast<int>(opt_layout.value_rect.w) - 12),
                    std::max(0, static_cast<int>(opt_layout.value_rect.h) - 2)};
                msi::draw_text_with_clip(engine,
                                         renderer,
                                         widget.badge,
                                         static_cast<int>(opt_layout.value_rect.x) + 10,
                                         static_cast<int>(opt_layout.value_rect.y) + 2,
                                         SDL_Color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255},
                                         &value_clip);
                drew_option_value = true;
            }
            if (opt_layout.has_primary_input) {
                draw_text_input(engine,
                                state,
                                renderer,
                                opt_layout.primary_input,
                                widget,
                                widget.text_buffer,
                                widget.placeholder,
                                state.text_edit_active && widget.id == state.text_edit_widget &&
                                    !state.text_edit_using_aux);
            }
            if (opt_layout.has_secondary_input) {
                draw_text_input(engine,
                                state,
                                renderer,
                                opt_layout.secondary_input,
                                widget,
                                widget.aux_text_buffer,
                                widget.aux_placeholder,
                                state.text_edit_active && widget.id == state.text_edit_widget &&
                                    state.text_edit_using_aux);
            }
        }

        if (widget.badge && !drew_option_value) {
            int badge_w = msi::measure_text_width(engine, widget.badge);
            float right_margin = rect.w * 0.03f;
            int badge_x = static_cast<int>(rect.x + rect.w - static_cast<float>(badge_w) - right_margin);
            float left_margin = rect.w * 0.02f;
            if (badge_x < static_cast<int>(rect.x + left_margin))
                badge_x = static_cast<int>(rect.x + left_margin);
            int badge_y = static_cast<int>(rect.y + rect.h * 0.1f);
            SDL_Rect badge_clip{
                static_cast<int>(rect.x + left_margin),
                static_cast<int>(rect.y + rect.h * 0.05f),
                std::max(0, static_cast<int>(rect.w - left_margin * 2.0f)),
                std::max(0, static_cast<int>(rect.h * 0.9f))};
            msi::draw_text_with_clip(engine, renderer, widget.badge, badge_x, badge_y, widget.badge_color, &badge_clip);
        }
    }

    msi::draw_focus_arrows(state, renderer);
}
