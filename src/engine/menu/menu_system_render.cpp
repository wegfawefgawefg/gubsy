#include "engine/menu/menu_system.hpp"

#include "engine/menu/menu_system_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace msi = menu_system_internal;

void menu_system_render(SDL_Renderer* renderer, int screen_width, int screen_height) {
    if ((msi::g_cache.width != screen_width || msi::g_cache.height != screen_height) &&
        screen_width > 0 && screen_height > 0) {
        MenuInputState zero_input{};
        msi::g_prev_input = msi::g_current_input;
        msi::g_current_input = zero_input;
        menu_system_update(0.0f, screen_width, screen_height);
    }
    if (!renderer || msi::g_cache.widgets.empty())
        return;

    const UILayout* layout = get_ui_layout_for_resolution(static_cast<int>(msi::g_cache.layout),
                                                          msi::g_cache.width,
                                                          msi::g_cache.height);
    for (std::size_t i = 0; i < msi::g_cache.widgets.size(); ++i) {
        const MenuWidget& widget = msi::g_cache.widgets[i];
        SDL_FRect rect;
        if (i < msi::g_cache.rects.size())
            rect = msi::g_cache.rects[i];
        else {
            const UIObject* obj = layout ? get_ui_object(*layout, static_cast<int>(widget.slot)) : nullptr;
            if (obj)
                rect = msi::rect_from_object(*obj, msi::g_cache.width, msi::g_cache.height);
            else
                rect = SDL_FRect{
                    static_cast<float>(msi::g_cache.width) * 0.3f,
                    static_cast<float>(msi::g_cache.height) * 0.3f,
                    static_cast<float>(msi::g_cache.width) * 0.4f,
                    60.0f};
        }

        bool draw_background = widget.type != WidgetType::Label;
        if (draw_background) {
            SDL_Color bg{widget.style.bg_r, widget.style.bg_g, widget.style.bg_b, widget.style.bg_a};
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRectF(renderer, &rect);

            SDL_Color border{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, widget.style.fg_a};
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
            SDL_RenderDrawRectF(renderer, &rect);

            if (widget.id == msi::g_focus) {
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
                SDL_RenderFillRectF(renderer, &rect);

                SDL_FRect outline = rect;
                outline.x -= 2.0f;
                outline.y -= 2.0f;
                outline.w += 4.0f;
                outline.h += 4.0f;
                SDL_SetRenderDrawColor(renderer, focus.r, focus.g, focus.b, focus.a);
                SDL_RenderDrawRectF(renderer, &outline);

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
                SDL_RenderDrawRectF(renderer, &inner);
            }
        }

        bool has_slider_visual = widget.type == WidgetType::Slider1D;
        msi::SliderLayout slider_visual{};
        if (has_slider_visual)
            slider_visual = msi::compute_slider_layout(widget, rect);
        bool slider_has_input = has_slider_visual && slider_visual.has_input;
        bool drew_option_value = false;

        std::string text_storage;
        const char* text_ptr = widget.label;
        SDL_Color text_color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255};
        if (widget.type == WidgetType::TextInput && widget.text_buffer) {
            if (widget.text_buffer->empty() && widget.placeholder) {
                text_ptr = widget.placeholder;
                text_color = SDL_Color{160, 160, 180, 255};
            } else {
                text_storage = *widget.text_buffer;
                text_ptr = text_storage.c_str();
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
        auto next_line = [&]() {
            line_y += 22;
        };
        if (text_ptr) {
            msi::draw_text_with_clip(renderer, text_ptr, line_x, line_y, text_color, clip_ptr);
            next_line();
        }
        if (widget.secondary) {
            SDL_Color sec_color{static_cast<Uint8>(widget.style.fg_r / 2 + 50),
                                static_cast<Uint8>(widget.style.fg_g / 2 + 50),
                                static_cast<Uint8>(widget.style.fg_b / 2 + 50),
                                255};
            msi::draw_text_with_clip(renderer, widget.secondary, line_x, line_y, sec_color, clip_ptr);
            next_line();
        }
        if (widget.tertiary) {
            SDL_Color tert_color{static_cast<Uint8>(widget.style.fg_r / 3 + 60),
                                 static_cast<Uint8>(widget.style.fg_g / 3 + 60),
                                 static_cast<Uint8>(widget.style.fg_b / 3 + 60),
                                 255};
            msi::draw_text_with_clip(renderer, widget.tertiary, line_x, line_y, tert_color, clip_ptr);
        }
        if (widget.text_buffer &&
            msi::g_text_edit_active && widget.id == msi::g_text_edit_widget &&
            widget.type == WidgetType::TextInput) {
            if (std::fmod(msi::g_caret_time, 1.0f) < 0.5f) {
                int caret_x = line_x + msi::measure_text_width(widget.text_buffer->c_str());
                int caret_top = line_y - 2;
                int caret_bottom = caret_top + 18;
                SDL_SetRenderDrawColor(renderer, widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255);
                SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
            }
        }

        if (has_slider_visual && widget.bind_ptr) {
            float value = *reinterpret_cast<float*>(widget.bind_ptr);
            float range = widget.max - widget.min;
            float norm = (range > 0.0f) ? (value - widget.min) / range : 0.0f;
            norm = std::clamp(norm, 0.0f, 1.0f);
            float track_width = slider_visual.track_right - slider_visual.track_left;
            SDL_FRect track{slider_visual.track_left, slider_visual.track_y - 2.0f, track_width, 4.0f};
            SDL_SetRenderDrawColor(renderer, 55, 60, 78, 255);
            SDL_RenderFillRectF(renderer, &track);
            SDL_FRect fill = track;
            fill.w = track_width * norm;
            SDL_SetRenderDrawColor(renderer, 130, 185, 255, 255);
            SDL_RenderFillRectF(renderer, &fill);
            float knob_x = slider_visual.track_left + track_width * norm;
            SDL_FRect knob{knob_x - 6.0f, slider_visual.track_y - 9.0f, 12.0f, 18.0f};
            SDL_SetRenderDrawColor(renderer, 235, 238, 245, 255);
            SDL_RenderFillRectF(renderer, &knob);
            SDL_SetRenderDrawColor(renderer, 30, 35, 46, 255);
            SDL_RenderDrawRectF(renderer, &knob);

            if (slider_has_input) {
                SDL_Color input_bg{24, 26, 36, 255};
                SDL_Color input_border{80, 90, 110, 255};
                bool editing = msi::g_text_edit_active && widget.id == msi::g_text_edit_widget;
                if (editing)
                    input_border = SDL_Color{widget.style.focus_r, widget.style.focus_g, widget.style.focus_b, 255};
                SDL_SetRenderDrawColor(renderer, input_bg.r, input_bg.g, input_bg.b, input_bg.a);
                SDL_RenderFillRectF(renderer, &slider_visual.input_rect);
                SDL_SetRenderDrawColor(renderer, input_border.r, input_border.g, input_border.b, input_border.a);
                SDL_RenderDrawRectF(renderer, &slider_visual.input_rect);

                std::string display = widget.text_buffer ? *widget.text_buffer : std::string{};
                if (display.empty() && widget.placeholder)
                    display = widget.placeholder;
                SDL_Rect input_clip{
                    static_cast<int>(slider_visual.input_rect.x) + 4,
                    static_cast<int>(slider_visual.input_rect.y) + 3,
                    std::max(0, static_cast<int>(slider_visual.input_rect.w) - 8),
                    std::max(0, static_cast<int>(slider_visual.input_rect.h) - 6)};
                msi::draw_text_with_clip(renderer,
                                         display.c_str(),
                                         static_cast<int>(slider_visual.input_rect.x) + 6,
                                         static_cast<int>(slider_visual.input_rect.y) + 4,
                                         SDL_Color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255},
                                         &input_clip);
                if (editing && std::fmod(msi::g_caret_time, 1.0f) < 0.5f && widget.text_buffer) {
                    int caret_x = static_cast<int>(slider_visual.input_rect.x) + 6 +
                                  msi::measure_text_width(widget.text_buffer->c_str());
                    if (caret_x > static_cast<int>(slider_visual.input_rect.x + slider_visual.input_rect.w) - 6)
                        caret_x = static_cast<int>(slider_visual.input_rect.x + slider_visual.input_rect.w) - 6;
                    int caret_top = static_cast<int>(slider_visual.input_rect.y) + 4;
                    int caret_bottom = caret_top + static_cast<int>(slider_visual.input_rect.h) - 8;
                    SDL_SetRenderDrawColor(renderer, widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255);
                    SDL_RenderDrawLine(renderer, caret_x, caret_top, caret_x, caret_bottom);
                }
            }
            if (widget.quick_value_count > 0) {
                SDL_FRect quick_rects[kMenuMaxQuickValues];
                msi::compute_quick_value_rects(rect, widget.quick_value_count, quick_rects);
                float current_val = widget.bind_ptr ? *reinterpret_cast<float*>(widget.bind_ptr) : 0.0f;
                for (int qi = 0; qi < widget.quick_value_count; ++qi) {
                    const SDL_FRect& qrect = quick_rects[qi];
                    float target_val = widget.quick_values[qi];
                    bool selected = (target_val <= 0.0f && current_val <= 0.0f) ||
                                    (std::fabs(target_val - current_val) < std::max(0.5f, widget.step));
                    SDL_Color bg = selected ? SDL_Color{60, 105, 150, 255}
                                            : SDL_Color{35, 40, 55, 255};
                    SDL_Color border = selected ? SDL_Color{140, 200, 255, 255}
                                                : SDL_Color{90, 100, 125, 255};
                    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
                    SDL_RenderFillRectF(renderer, &qrect);
                    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
                    SDL_RenderDrawRectF(renderer, &qrect);
                    const char* label = widget.quick_labels[i];
                    char buffer[32];
                    if (!label || !*label) {
                        if (target_val <= 0.0f)
                            label = "Unlimited";
                        else {
                            std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(target_val));
                            label = buffer;
                        }
                    }
                    SDL_Rect quick_clip{
                        static_cast<int>(qrect.x) + 4,
                        static_cast<int>(qrect.y) + 2,
                        std::max(0, static_cast<int>(qrect.w) - 8),
                        std::max(0, static_cast<int>(qrect.h) - 4)};
                    msi::draw_text_with_clip(renderer,
                                             label,
                                             static_cast<int>(qrect.x) + 6,
                                             static_cast<int>(qrect.y) + 3,
                                             SDL_Color{225, 230, 240, 255},
                                             &quick_clip);
                }
            }
        }
        if (widget.type == WidgetType::OptionCycle) {
            msi::OptionLayout opt_layout = msi::compute_option_layout(rect);
            auto draw_option_button = [&](const SDL_FRect& btn_rect, bool left) {
                SDL_Color btn_bg{static_cast<Uint8>(widget.style.bg_r + 6),
                                 static_cast<Uint8>(widget.style.bg_g + 8),
                                 static_cast<Uint8>(widget.style.bg_b + 12),
                                 255};
                SDL_Color btn_border{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255};
                SDL_SetRenderDrawColor(renderer, btn_bg.r, btn_bg.g, btn_bg.b, btn_bg.a);
                SDL_RenderFillRectF(renderer, &btn_rect);
                SDL_SetRenderDrawColor(renderer, btn_border.r, btn_border.g, btn_border.b, btn_border.a);
                SDL_RenderDrawRectF(renderer, &btn_rect);
                float cx = btn_rect.x + btn_rect.w * 0.5f;
                float cy = btn_rect.y + btn_rect.h * 0.5f;
                float wing = btn_rect.h * 0.32f;
                float head = btn_rect.w * 0.22f;
                float dir = left ? -1.0f : 1.0f;
                SDL_FPoint tip{cx + dir * head, cy};
                SDL_FPoint wing_top{cx - dir * head * 0.4f, cy - wing};
                SDL_FPoint wing_bottom{cx - dir * head * 0.4f, cy + wing};
                SDL_RenderDrawLineF(renderer, tip.x, tip.y, wing_top.x, wing_top.y);
                SDL_RenderDrawLineF(renderer, tip.x, tip.y, wing_bottom.x, wing_bottom.y);
                SDL_RenderDrawLineF(renderer, wing_top.x, wing_top.y, wing_bottom.x, wing_bottom.y);
            };
            draw_option_button(opt_layout.left_btn, true);
            draw_option_button(opt_layout.right_btn, false);
            if (widget.badge) {
                SDL_Rect value_clip{
                    static_cast<int>(opt_layout.value_rect.x) + 6,
                    static_cast<int>(opt_layout.value_rect.y) + 4,
                    std::max(0, static_cast<int>(opt_layout.value_rect.w) - 12),
                    std::max(0, static_cast<int>(opt_layout.value_rect.h) - 8)};
                msi::draw_text_with_clip(renderer,
                                         widget.badge,
                                         static_cast<int>(opt_layout.value_rect.x) + 10,
                                         static_cast<int>(opt_layout.value_rect.y) + 5,
                                         SDL_Color{widget.style.fg_r, widget.style.fg_g, widget.style.fg_b, 255},
                                         &value_clip);
                drew_option_value = true;
            }
        }

        if (widget.badge && !drew_option_value) {
            int badge_w = msi::measure_text_width(widget.badge);
            int badge_x = static_cast<int>(rect.x + rect.w) - badge_w - 12;
            if (badge_x < static_cast<int>(rect.x) + 8)
                badge_x = static_cast<int>(rect.x) + 8;
            int badge_y = static_cast<int>(rect.y) + 6;
            SDL_Rect badge_clip{
                static_cast<int>(rect.x) + 8,
                static_cast<int>(rect.y) + 4,
                std::max(0, static_cast<int>(rect.w) - 16),
                std::max(0, static_cast<int>(rect.h) - 8)};
            msi::draw_text_with_clip(renderer, widget.badge, badge_x, badge_y, widget.badge_color, &badge_clip);
        }
    }

    msi::draw_focus_arrows(renderer);
}
