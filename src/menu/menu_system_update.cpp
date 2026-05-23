#include "src/menu/menu_system.hpp"

#include "src/engine_state.hpp"
#include "src/imgui_layer.hpp"
#include "src/input_binding_utils.hpp"
#include "src/layout_editor/layout_editor.hpp"
#include "src/layout_editor/layout_editor_hooks.hpp"
#include "src/menu/menu_system_state.hpp"
#include "src/render.hpp"

#include <algorithm>

namespace msi = menu_system_internal;

void menu_system_set_input(EngineState& engine, const MenuInputState& input) {
    msi::runtime_state(engine).current_input = input;
}

void menu_system_update(EngineState& engine, float dt, int screen_width, int screen_height) {
    msi::MenuRuntimeState& state = msi::runtime_state(engine);
    state.active = false;
    MenuManager& manager = engine.menu_manager;
    if (manager.stack().empty())
        return;

    state.active = true;
    if (state.text_edit_active)
        state.caret_time += dt;
    else
        state.caret_time = 0.0f;

    bool stack_changed = false;
    bool needs_rebuild = true;
    WidgetId prev_focus_frame = state.focus;

    state.has_focus_rect = false;
    state.has_focus_color = false;

    while (needs_rebuild && !manager.stack().empty()) {
        needs_rebuild = false;
        auto& inst = const_cast<MenuManager::ScreenInstance&>(manager.stack().back());
        state.current_screen = inst.def ? inst.def->id : kMenuIdInvalid;
        if (!inst.def || !inst.def->build)
            break;
        MenuContext ctx{engine,
                        manager,
                        screen_width,
                        screen_height,
                        inst.player_index,
                        inst.def ? inst.def->id : kMenuIdInvalid,
                        inst.state_ptr};
        auto handle_text_commit = [&](WidgetId widget_id, bool modified) {
            if (!modified || widget_id == kMenuIdInvalid)
                return false;
            MenuWidget* edited = msi::find_widget(state, widget_id);
            if (edited && edited->on_select.type != MenuActionType::None) {
                msi::execute_action(state, edited->on_select, ctx, stack_changed);
                return true;
            }
            return false;
        };
        msi::rebuild_cache(state, inst, ctx);
        MenuWidget* focus = msi::find_widget(state, state.focus);
        MenuInputState prev = state.prev_input;
        state.prev_input = state.current_input;
        bool select_handled = false;

        const bool allow_mouse_input =
            !imgui_want_capture_mouse() && !layout_editor_is_active(engine);
        int mouse_x = engine.device_state.mouse_x;
        int mouse_y = engine.device_state.mouse_y;
        state.last_mouse_x = mouse_x;
        state.last_mouse_y = mouse_y;
        Uint32 mouse_buttons = allow_mouse_input ? engine.device_state.mouse_buttons : 0u;
        float render_mouse_x = static_cast<float>(mouse_x);
        float render_mouse_y = static_cast<float>(mouse_y);
        bool has_render_mouse = false;
        if (allow_mouse_input && state.cache.width > 0 && state.cache.height > 0) {
            if (mouse_render_position(engine,
                                      static_cast<float>(state.cache.width),
                                      static_cast<float>(state.cache.height),
                                      render_mouse_x,
                                      render_mouse_y)) {
                has_render_mouse = true;
            }
        }
        bool mouse_down =
            allow_mouse_input && (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        bool mouse_clicked = allow_mouse_input && mouse_down && !state.prev_mouse_down;
        if (allow_mouse_input) {
            state.prev_mouse_down = mouse_down;
            msi::ensure_mouse_lock(state, mouse_x, mouse_y);
            msi::unlock_mouse_focus_if_moved(state, mouse_x, mouse_y);
            if (mouse_clicked)
                msi::unlock_mouse_focus_now(state);
        } else {
            state.prev_mouse_down = false;
            has_render_mouse = false;
        }

        const UILayout* layout_rects =
            get_ui_layout_for_resolution(engine,
                                         static_cast<int>(state.cache.layout),
                                         state.cache.width,
                                         state.cache.height);
        for (std::size_t idx = 0; idx < state.cache.widgets.size(); ++idx) {
            const MenuWidget& widget = state.cache.widgets[idx];
            const UIObject* obj =
                layout_rects ? get_ui_object(*layout_rects, static_cast<int>(widget.slot)) : nullptr;
            SDL_FRect rect;
            if (obj) {
                rect = msi::rect_from_object(*obj, state.cache.width, state.cache.height);
            } else {
                rect = SDL_FRect{static_cast<float>(state.cache.width) * 0.3f,
                                 static_cast<float>(state.cache.height) * 0.3f,
                                 static_cast<float>(state.cache.width) * 0.4f,
                                 60.0f};
            }
            if (idx >= state.cache.rects.size())
                state.cache.rects.push_back(rect);
            else
                state.cache.rects[idx] = rect;

            if (widget.id == state.focus) {
                state.focus_rect = rect;
                state.has_focus_rect = true;
                state.focus_outline_color = SDL_Color{widget.style.focus_r,
                                                      widget.style.focus_g,
                                                      widget.style.focus_b,
                                                      widget.style.focus_a};
                state.has_focus_color = true;
            }
        }

        bool up_pressed = state.current_input.up && !prev.up;
        bool down_pressed = state.current_input.down && !prev.down;
        bool left_pressed = state.current_input.left && !prev.left;
        bool right_pressed = state.current_input.right && !prev.right;
        bool select_pressed = state.current_input.select && !prev.select;
        bool back_pressed = state.current_input.back && !prev.back;
        bool page_prev_pressed = state.current_input.page_prev && !prev.page_prev;
        bool page_next_pressed = state.current_input.page_next && !prev.page_next;

        msi::update_repeat(state.current_input.left, state.repeat_left, left_pressed, dt);
        msi::update_repeat(state.current_input.right, state.repeat_right, right_pressed, dt);

        bool editing_focus =
            state.text_edit_active && focus && focus->id == state.text_edit_widget;

        if (focus && editing_focus) {
            up_pressed = down_pressed = left_pressed = right_pressed = false;
            page_prev_pressed = page_next_pressed = false;
            select_pressed = false;
            select_handled = true;
            if (back_pressed) {
                WidgetId editing_id = focus->id;
                bool modified = msi::end_text_edit(state);
                if (handle_text_commit(editing_id, modified) || modified) {
                    needs_rebuild = true;
                    continue;
                }
                back_pressed = false;
                editing_focus = false;
            }
        }

        if (focus && !editing_focus) {
            if (up_pressed) {
                msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                if (focus->nav_up != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(state, focus->nav_up);
                    if (target != kMenuIdInvalid)
                        state.focus = target;
                }
            } else if (down_pressed) {
                msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                if (focus->nav_down != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(state, focus->nav_down);
                    if (target != kMenuIdInvalid)
                        state.focus = target;
                }
            }

            if (!needs_rebuild && left_pressed) {
                msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                bool handled = false;
                if (focus->on_left.type != MenuActionType::None) {
                    msi::play_left_sound(engine);
                    msi::execute_action(state, focus->on_left, ctx, stack_changed);
                    needs_rebuild = true;
                    handled = true;
                    continue;
                } else if (focus->nav_left != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(state, focus->nav_left);
                    if (target != kMenuIdInvalid) {
                        state.focus = target;
                        handled = true;
                    }
                }
                if (!handled)
                    msi::play_cant_sound(engine);
            }

            if (!needs_rebuild && right_pressed) {
                msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                bool handled = false;
                if (focus->on_right.type != MenuActionType::None) {
                    msi::play_right_sound(engine);
                    msi::execute_action(state, focus->on_right, ctx, stack_changed);
                    needs_rebuild = true;
                    handled = true;
                    continue;
                } else if (focus->nav_right != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(state, focus->nav_right);
                    if (target != kMenuIdInvalid) {
                        state.focus = target;
                        handled = true;
                    }
                }
                if (!handled)
                    msi::play_cant_sound(engine);
            }

            if (!needs_rebuild) {
                if (select_pressed && focus->text_buffer && focus->select_enters_text) {
                    msi::begin_text_edit(state, *focus);
                    select_handled = true;
                    select_pressed = false;
                    continue;
                }

                if (select_pressed && focus->on_select.type != MenuActionType::None) {
                    msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                    if (focus->play_select_sound)
                        msi::play_confirm_sound(engine);
                    select_handled = true;
                    msi::execute_action(state, focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                } else if (select_pressed && !select_handled) {
                    msi::play_cant_sound(engine);
                    select_pressed = false;
                } else if (back_pressed) {
                    if (focus->on_back.type != MenuActionType::None) {
                        msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                        msi::execute_action(state, focus->on_back, ctx, stack_changed);
                        needs_rebuild = true;
                        continue;
                    } else if (manager.stack().size() > 1) {
                        msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                        MenuAction pop = MenuAction::pop();
                        msi::execute_action(state, pop, ctx, stack_changed);
                        needs_rebuild = true;
                        continue;
                    } else {
                        back_pressed = false;
                    }
                }
            }

            if (!needs_rebuild && page_prev_pressed) {
                msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                MenuAction action = MenuAction::none();
                auto find_page_widget = [&](MenuWidgetRole role) -> MenuWidget* {
                    for (auto& widget : state.cache.widgets) {
                        if (widget.role == role)
                            return &widget;
                    }
                    return nullptr;
                };
                if (MenuWidget* prev_widget = find_page_widget(MenuWidgetRole::PagePrev))
                    action = prev_widget->on_select;
                if (action.type != MenuActionType::None) {
                    msi::play_left_sound(engine);
                    msi::execute_action(state, action, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                }
                msi::play_cant_sound(engine);
            }
            if (!needs_rebuild && page_next_pressed) {
                msi::lock_mouse_focus_at(state, mouse_x, mouse_y);
                MenuAction action = MenuAction::none();
                auto find_page_widget = [&](MenuWidgetRole role) -> MenuWidget* {
                    for (auto& widget : state.cache.widgets) {
                        if (widget.role == role)
                            return &widget;
                    }
                    return nullptr;
                };
                if (MenuWidget* next_widget = find_page_widget(MenuWidgetRole::PageNext))
                    action = next_widget->on_select;
                if (action.type != MenuActionType::None) {
                    msi::play_right_sound(engine);
                    msi::execute_action(state, action, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                }
                msi::play_cant_sound(engine);
            }
        }

        focus = msi::find_widget(state, state.focus);
        if (!focus) {
            state.focus = msi::first_selectable_widget(state);
            focus = msi::find_widget(state, state.focus);
        }
        editing_focus =
            state.text_edit_active && focus && focus->id == state.text_edit_widget;
        if (state.text_edit_active && !editing_focus) {
            WidgetId editing_id = msi::current_text_widget(state);
            bool modified = msi::end_text_edit(state);
            if (handle_text_commit(editing_id, modified) || modified) {
                needs_rebuild = true;
                continue;
            }
        }

        if (!focus) {
            state.focus = (!state.cache.widgets.empty()) ? state.cache.widgets.front().id
                                                         : kMenuIdInvalid;
            focus = msi::find_widget(state, state.focus);
        }

        auto apply_slider_position =
            [&](MenuWidget& slider, float fx, bool begin_drag, bool persist_now) -> bool {
            SDL_FRect* rect_ptr = msi::find_widget_rect(state, slider.id);
            if (!rect_ptr)
                return false;
            msi::SliderLayout slider_layout = msi::compute_slider_layout(slider, *rect_ptr);
            float track_width = slider_layout.track_right - slider_layout.track_left;
            if (track_width <= 1.0f)
                return false;
            float norm = (fx - slider_layout.track_left) / track_width;
            norm = std::clamp(norm, 0.0f, 1.0f);
            float target_value = slider.min + (slider.max - slider.min) * norm;
            if (persist_now) {
                if (slider.on_select.type == MenuActionType::None)
                    return false;
                state.slider_drag_value = target_value;
                state.slider_drag_value_valid = true;
                state.slider_commit_pending = true;
                msi::execute_action(state, slider.on_select, ctx, stack_changed);
                state.slider_commit_pending = false;
                state.slider_drag_value_valid = false;
            } else if (slider.bind_ptr) {
                *reinterpret_cast<float*>(slider.bind_ptr) = target_value;
                if (slider.text_buffer && !state.text_edit_active) {
                    float shown = target_value * slider.display_scale + slider.display_offset;
                    int precision = std::max(0, slider.display_precision);
                    char buffer[64];
                    if (precision == 0) {
                        std::snprintf(buffer, sizeof(buffer), "%.0f",
                                      static_cast<double>(shown));
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%.*f", precision,
                                      static_cast<double>(shown));
                    }
                    *slider.text_buffer = buffer;
                }
            }
            if (begin_drag)
                state.slider_drag_id = slider.id;
            if (persist_now)
                needs_rebuild = true;
            return true;
        };

        if (allow_mouse_input) {
            WidgetId hovered = kMenuIdInvalid;
            for (std::size_t i = 0; i < state.cache.widgets.size() && i < state.cache.rects.size();
                 ++i) {
                const MenuWidget& widget = state.cache.widgets[i];
                if (widget.type == WidgetType::Label)
                    continue;
                const SDL_FRect& rect = state.cache.rects[i];
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                bool inside =
                    fx >= rect.x && fx <= rect.x + rect.w && fy >= rect.y && fy <= rect.y + rect.h;
                if (inside) {
                    hovered = widget.id;
                    break;
                }
            }
            bool hover_changed = hovered != state.focus;
            if (state.allow_mouse_focus && hover_changed && hovered != kMenuIdInvalid &&
                !mouse_down) {
                state.focus = hovered;
                focus = msi::find_widget(state, state.focus);
            }
            if (hovered != kMenuIdInvalid && mouse_clicked) {
                state.focus = hovered;
                focus = msi::find_widget(state, state.focus);
                bool click_handled = false;
                if (focus && focus->type == WidgetType::OptionCycle) {
                    SDL_FRect* rect_ptr = msi::find_widget_rect(state, focus->id);
                    if (rect_ptr) {
                        msi::OptionLayout opt_layout = msi::compute_option_layout(*focus, *rect_ptr);
                        float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                        float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                        auto trigger_action = [&](const MenuAction& action, auto sound_fn) -> bool {
                            if (action.type == MenuActionType::None)
                                return false;
                            sound_fn(engine);
                            msi::execute_action(state, action, ctx, stack_changed);
                            needs_rebuild = true;
                            return true;
                        };
                        if (msi::point_in_rect(fx, fy, opt_layout.left_btn)) {
                            if (trigger_action(focus->on_left, [](EngineState& sound_engine) {
                                    msi::play_left_sound(sound_engine);
                                })) {
                                continue;
                            }
                        } else if (msi::point_in_rect(fx, fy, opt_layout.right_btn)) {
                            if (trigger_action(focus->on_right, [](EngineState& sound_engine) {
                                    msi::play_right_sound(sound_engine);
                                })) {
                                continue;
                            }
                        } else if (msi::point_in_rect(fx, fy, opt_layout.value_rect)) {
                            if (trigger_action(focus->on_select, [](EngineState& sound_engine) {
                                    msi::play_confirm_sound(sound_engine);
                                })) {
                                continue;
                            }
                        } else if (opt_layout.has_primary_input &&
                                   msi::point_in_rect(fx, fy, opt_layout.primary_input)) {
                            msi::begin_text_edit(state, *focus, false);
                            continue;
                        } else if (opt_layout.has_secondary_input &&
                                   msi::point_in_rect(fx, fy, opt_layout.secondary_input)) {
                            msi::begin_text_edit(state, *focus, true);
                            continue;
                        }
                    }
                }
                if (focus && focus->type == WidgetType::Slider1D) {
                    SDL_FRect* rect_ptr = msi::find_widget_rect(state, focus->id);
                    if (rect_ptr) {
                        auto slider_layout = msi::compute_slider_layout(*focus, *rect_ptr);
                        float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                        float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                        auto trigger_action = [&](const MenuAction& action, auto sound_fn) -> bool {
                            if (action.type == MenuActionType::None)
                                return false;
                            sound_fn(engine);
                            msi::execute_action(state, action, ctx, stack_changed);
                            needs_rebuild = true;
                            return true;
                        };
                        if (focus->has_discrete_options && slider_layout.has_buttons) {
                            if (msi::point_in_rect(fx, fy, slider_layout.left_btn)) {
                                if (trigger_action(focus->on_left, [](EngineState& sound_engine) {
                                        msi::play_left_sound(sound_engine);
                                    })) {
                                    continue;
                                }
                            } else if (msi::point_in_rect(fx, fy, slider_layout.right_btn)) {
                                if (trigger_action(focus->on_right, [](EngineState& sound_engine) {
                                        msi::play_right_sound(sound_engine);
                                    })) {
                                    continue;
                                }
                            }
                        }
                        if (slider_layout.has_input &&
                            msi::point_in_rect(fx, fy, slider_layout.input_rect)) {
                            msi::begin_text_edit(state, *focus);
                            click_handled = true;
                            continue;
                        }
                        SDL_FRect track_rect{slider_layout.track_left,
                                             slider_layout.track_y - 10.0f,
                                             slider_layout.track_right - slider_layout.track_left,
                                             20.0f};
                        if (track_rect.w > 4.0f && msi::point_in_rect(fx, fy, track_rect)) {
                            apply_slider_position(*focus, fx, true, false);
                            click_handled = true;
                        }
                    }
                }
                if (focus && focus->text_buffer && focus->select_enters_text && !click_handled) {
                    msi::begin_text_edit(state, *focus);
                    click_handled = true;
                }
                if (!click_handled) {
                    WidgetId editing_id = msi::current_text_widget(state);
                    bool modified = msi::end_text_edit(state);
                    if (handle_text_commit(editing_id, modified) || modified) {
                        needs_rebuild = true;
                        continue;
                    }
                }
                if (focus && focus->on_select.type != MenuActionType::None) {
                    msi::play_confirm_sound(engine);
                    click_handled = true;
                    msi::execute_action(state, focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                } else if (!click_handled) {
                    msi::play_cant_sound(engine);
                }
            }
        }

        if (!mouse_down && state.slider_drag_id != kMenuIdInvalid) {
            MenuWidget* dragging = msi::find_widget(state, state.slider_drag_id);
            if (dragging && dragging->type == WidgetType::Slider1D) {
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                if (apply_slider_position(*dragging, fx, false, true)) {
                    state.slider_drag_id = kMenuIdInvalid;
                    continue;
                }
            }
            state.slider_drag_id = kMenuIdInvalid;
        } else if (mouse_down && state.slider_drag_id != kMenuIdInvalid) {
            MenuWidget* dragging = msi::find_widget(state, state.slider_drag_id);
            if (dragging && dragging->type == WidgetType::Slider1D) {
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                apply_slider_position(*dragging, fx, false, false);
            }
        }

        if (state.current_screen != kMenuIdInvalid && focus)
            state.last_focus[state.current_screen] = focus->id;
    }

    msi::update_arrows(state, dt);

    if (state.focus != prev_focus_frame && state.focus != kMenuIdInvalid)
        msi::play_focus_sound(engine);

    if (manager.stack().empty()) {
        state.current_screen = kMenuIdInvalid;
        msi::end_text_edit(state);
        state.last_focus.clear();
        state.cache.widgets.clear();
        state.cache.layout = kMenuIdInvalid;
        state.focus = kMenuIdInvalid;
        state.cache.rects.clear();
        state.active_text_buffer = nullptr;
        state.arrows.initialized = false;
        state.has_focus_rect = false;
        state.has_focus_color = false;
        state.allow_mouse_focus = true;
        state.mouse_focus_locked = false;
    }
}

void menu_system_handle_text_input(EngineState& engine, const char* text) {
    msi::MenuRuntimeState& state = msi::runtime_state(engine);
    if (!state.text_edit_active || !state.active_text_buffer || !text)
        return;
    while (*text) {
        if (state.active_text_max <= 0 ||
            static_cast<int>(state.active_text_buffer->size()) < state.active_text_max) {
            state.active_text_buffer->push_back(*text);
        }
        ++text;
    }
}

void menu_system_handle_backspace(EngineState& engine, bool clear_all) {
    msi::MenuRuntimeState& state = msi::runtime_state(engine);
    if (!state.active_text_buffer) {
        if (state.focus != kMenuIdInvalid) {
            MenuWidget* focus_widget = msi::find_widget(state, state.focus);
            if (focus_widget && focus_widget->type == WidgetType::TextInput)
                msi::begin_text_edit(state, *focus_widget);
        }
    }
    if (!state.text_edit_active || !state.active_text_buffer)
        return;

    if (clear_all) {
        state.active_text_buffer->clear();
    } else if (!state.active_text_buffer->empty()) {
        state.active_text_buffer->pop_back();
    }
}

void menu_system_reset(EngineState& engine) {
    msi::MenuRuntimeState& state = msi::runtime_state(engine);
    msi::end_text_edit(state);
    state.last_focus.clear();
    state.cache.widgets.clear();
    state.cache.layout = kMenuIdInvalid;
    state.cache.rects.clear();
    state.focus = kMenuIdInvalid;
    state.prev_input = {};
    state.current_input = {};
    state.active_text_buffer = nullptr;
    if (state.text_input_enabled) {
        SDL_StopTextInput();
        state.text_input_enabled = false;
    }
    state.allow_mouse_focus = true;
    state.mouse_focus_locked = false;
    state.mouse_focus_lock_x = 0;
    state.mouse_focus_lock_y = 0;
}

bool menu_system_active(const EngineState& engine) {
    return msi::runtime_state(engine).active;
}
