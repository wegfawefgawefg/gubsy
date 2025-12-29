#include "engine/menu/menu_system.hpp"

#include "engine/globals.hpp"
#include "engine/imgui_layer.hpp"
#include "engine/input_binding_utils.hpp"
#include "engine/layout_editor/layout_editor.hpp"
#include "engine/layout_editor/layout_editor_hooks.hpp"
#include "engine/menu/menu_system_state.hpp"
#include "engine/render.hpp"

#include <algorithm>

namespace msi = menu_system_internal;

void menu_system_set_input(const MenuInputState& input) {
    msi::g_current_input = input;
}

void menu_system_update(float dt, int screen_width, int screen_height) {
    msi::g_active = false;
    if (!es || !ss)
        return;

    MenuManager& manager = es->menu_manager;
    if (manager.stack().empty())
        return;

    msi::g_active = true;
    if (msi::g_text_edit_active)
        msi::g_caret_time += dt;
    else
        msi::g_caret_time = 0.0f;

    bool stack_changed = false;
    bool needs_rebuild = true;
    WidgetId prev_focus_frame = msi::g_focus;

    msi::g_has_focus_rect = false;
    msi::g_has_focus_color = false;

    while (needs_rebuild && !manager.stack().empty()) {
        needs_rebuild = false;
        auto& inst = const_cast<MenuManager::ScreenInstance&>(manager.stack().back());
        msi::g_current_screen = inst.def ? inst.def->id : kMenuIdInvalid;
        if (!inst.def || !inst.def->build)
            break;
        MenuContext ctx{*es,
                        *ss,
                        manager,
                        screen_width,
                        screen_height,
                        inst.player_index,
                        inst.def ? inst.def->id : kMenuIdInvalid,
                        inst.state_ptr};
        auto handle_text_commit = [&](WidgetId widget_id, bool modified) {
            if (!modified || widget_id == kMenuIdInvalid)
                return false;
            MenuWidget* edited = msi::find_widget(widget_id);
            if (edited && edited->on_select.type != MenuActionType::None) {
                msi::execute_action(edited->on_select, ctx, stack_changed);
                return true;
            }
            return false;
        };
        msi::rebuild_cache(inst, ctx);
        MenuWidget* focus = msi::find_widget(msi::g_focus);
        MenuInputState prev = msi::g_prev_input;
        msi::g_prev_input = msi::g_current_input;
        bool select_handled = false;

        const bool allow_mouse_input = !imgui_want_capture_mouse() && !layout_editor_is_active();
        int mouse_x = es->device_state.mouse_x;
        int mouse_y = es->device_state.mouse_y;
        Uint32 mouse_buttons = allow_mouse_input ? es->device_state.mouse_buttons : 0u;
        float render_mouse_x = static_cast<float>(mouse_x);
        float render_mouse_y = static_cast<float>(mouse_y);
        bool has_render_mouse = false;
        if (allow_mouse_input && msi::g_cache.width > 0 && msi::g_cache.height > 0) {
            if (mouse_render_position(es->device_state,
                                      static_cast<float>(msi::g_cache.width),
                                      static_cast<float>(msi::g_cache.height),
                                      render_mouse_x,
                                      render_mouse_y)) {
                has_render_mouse = true;
            }
        }
        bool mouse_down = allow_mouse_input &&
                          (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        bool mouse_clicked = allow_mouse_input && mouse_down && !msi::g_prev_mouse_down;
        if (allow_mouse_input) {
            msi::g_prev_mouse_down = mouse_down;
            msi::ensure_mouse_lock(mouse_x, mouse_y);
            msi::unlock_mouse_focus_if_moved(mouse_x, mouse_y);
            if (mouse_clicked)
                msi::unlock_mouse_focus_now();
        } else {
            msi::g_prev_mouse_down = false;
            has_render_mouse = false;
        }

        const UILayout* layout_rects =
            get_ui_layout_for_resolution(static_cast<int>(msi::g_cache.layout),
                                         msi::g_cache.width,
                                         msi::g_cache.height);
        for (std::size_t idx = 0; idx < msi::g_cache.widgets.size(); ++idx) {
            const MenuWidget& widget = msi::g_cache.widgets[idx];
            const UIObject* obj =
                layout_rects ? get_ui_object(*layout_rects, static_cast<int>(widget.slot)) : nullptr;
            SDL_FRect rect;
            if (obj)
                rect = msi::rect_from_object(*obj, msi::g_cache.width, msi::g_cache.height);
            else
                rect = SDL_FRect{
                    static_cast<float>(msi::g_cache.width) * 0.3f,
                    static_cast<float>(msi::g_cache.height) * 0.3f,
                    static_cast<float>(msi::g_cache.width) * 0.4f,
                    60.0f};
            if (idx >= msi::g_cache.rects.size())
                msi::g_cache.rects.push_back(rect);
            else
                msi::g_cache.rects[idx] = rect;

            if (widget.id == msi::g_focus) {
                msi::g_focus_rect = rect;
                msi::g_has_focus_rect = true;
                msi::g_focus_outline_color = SDL_Color{widget.style.focus_r,
                                                       widget.style.focus_g,
                                                       widget.style.focus_b,
                                                       widget.style.focus_a};
                msi::g_has_focus_color = true;
            }
        }

        bool up_pressed = msi::g_current_input.up && !prev.up;
        bool down_pressed = msi::g_current_input.down && !prev.down;
        bool left_pressed = msi::g_current_input.left && !prev.left;
        bool right_pressed = msi::g_current_input.right && !prev.right;
        bool select_pressed = msi::g_current_input.select && !prev.select;
        bool back_pressed = msi::g_current_input.back && !prev.back;
        bool page_prev_pressed = msi::g_current_input.page_prev && !prev.page_prev;
        bool page_next_pressed = msi::g_current_input.page_next && !prev.page_next;

        // Update repeat for left/right
        msi::update_repeat(msi::g_current_input.left, msi::g_repeat_left, left_pressed, dt);
        msi::update_repeat(msi::g_current_input.right, msi::g_repeat_right, right_pressed, dt);

        bool editing_focus = msi::g_text_edit_active && focus && focus->id == msi::g_text_edit_widget;

        if (focus && editing_focus) {
            // When editing text, disable all directional/page inputs and select
            up_pressed = down_pressed = left_pressed = right_pressed = false;
            page_prev_pressed = page_next_pressed = false;
            select_pressed = false;
            select_handled = true;
            if (back_pressed) {
                WidgetId editing_id = focus->id;
                bool modified = msi::end_text_edit();
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
                msi::lock_mouse_focus_at(mouse_x, mouse_y);
                if (focus->nav_up != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(focus->nav_up);
                    if (target != kMenuIdInvalid)
                        msi::g_focus = target;
                }
            } else if (down_pressed) {
                msi::lock_mouse_focus_at(mouse_x, mouse_y);
                if (focus->nav_down != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(focus->nav_down);
                    if (target != kMenuIdInvalid)
                        msi::g_focus = target;
                }
            }

            if (!needs_rebuild && left_pressed) {
                msi::lock_mouse_focus_at(mouse_x, mouse_y);
                bool handled = false;
                if (focus->on_left.type != MenuActionType::None) {
                    msi::play_left_sound();
                    msi::execute_action(focus->on_left, ctx, stack_changed);
                    needs_rebuild = true;
                    handled = true;
                    continue;
                } else if (focus->nav_left != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(focus->nav_left);
                    if (target != kMenuIdInvalid) {
                        msi::g_focus = target;
                        handled = true;
                    }
                }
                if (!handled)
                    msi::play_cant_sound();
            }

            if (!needs_rebuild && right_pressed) {
                msi::lock_mouse_focus_at(mouse_x, mouse_y);
                bool handled = false;
                if (focus->on_right.type != MenuActionType::None) {
                    msi::play_right_sound();
                    msi::execute_action(focus->on_right, ctx, stack_changed);
                    needs_rebuild = true;
                    handled = true;
                    continue;
                } else if (focus->nav_right != kMenuIdInvalid) {
                    WidgetId target = msi::resolve_focus(focus->nav_right);
                    if (target != kMenuIdInvalid) {
                        msi::g_focus = target;
                        handled = true;
                    }
                }
                if (!handled)
                    msi::play_cant_sound();
            }

            if (!needs_rebuild) {
                // Handle TextInput widgets that enter edit mode on select
                if (select_pressed && focus->text_buffer && focus->select_enters_text) {
                    msi::begin_text_edit(*focus);
                    select_handled = true;
                    select_pressed = false;
                    continue;
                }

                if (select_pressed && focus->on_select.type != MenuActionType::None) {
                    msi::lock_mouse_focus_at(mouse_x, mouse_y);
                    if (focus->play_select_sound)
                        msi::play_confirm_sound();
                    select_handled = true;
                    msi::execute_action(focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                } else if (select_pressed && !select_handled) {
                    msi::play_cant_sound();
                    select_pressed = false;
                } else if (back_pressed) {
                    if (focus->on_back.type != MenuActionType::None) {
                        msi::lock_mouse_focus_at(mouse_x, mouse_y);
                        msi::execute_action(focus->on_back, ctx, stack_changed);
                        needs_rebuild = true;
                        continue;
                    } else if (manager.stack().size() > 1) {
                        msi::lock_mouse_focus_at(mouse_x, mouse_y);
                        MenuAction pop = MenuAction::pop();
                        msi::execute_action(pop, ctx, stack_changed);
                        needs_rebuild = true;
                        continue;
                    } else {
                        back_pressed = false;
                    }
                }
            }

            if (!needs_rebuild && page_prev_pressed) {
                msi::lock_mouse_focus_at(mouse_x, mouse_y);
                MenuAction action = MenuAction::none();
                auto find_page_widget = [&](std::initializer_list<UILayoutObjectId> slots) -> MenuWidget* {
                    for (UILayoutObjectId slot : slots) {
                        if (MenuWidget* widget = msi::find_widget_by_slot(slot))
                            return widget;
                    }
                    return nullptr;
                };
                if (MenuWidget* prev_widget = find_page_widget({SettingsObjectID::PREV,
                                                               ModsObjectID::PREV,
                                                               LobbyModsObjectID::PREV,
                                                               LocalPlayersObjectID::PREV}))
                    action = prev_widget->on_select;
                if (action.type != MenuActionType::None) {
                    msi::play_left_sound();
                    msi::execute_action(action, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                }
                msi::play_cant_sound();
            }
            if (!needs_rebuild && page_next_pressed) {
                msi::lock_mouse_focus_at(mouse_x, mouse_y);
                MenuAction action = MenuAction::none();
                auto find_page_widget = [&](std::initializer_list<UILayoutObjectId> slots) -> MenuWidget* {
                    for (UILayoutObjectId slot : slots) {
                        if (MenuWidget* widget = msi::find_widget_by_slot(slot))
                            return widget;
                    }
                    return nullptr;
                };
                if (MenuWidget* next_widget = find_page_widget({SettingsObjectID::NEXT,
                                                               ModsObjectID::NEXT,
                                                               LobbyModsObjectID::NEXT,
                                                               LocalPlayersObjectID::NEXT}))
                    action = next_widget->on_select;
                if (action.type != MenuActionType::None) {
                    msi::play_right_sound();
                    msi::execute_action(action, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                }
                msi::play_cant_sound();
            }
        }

        focus = msi::find_widget(msi::g_focus);
        if (!focus) {
            msi::g_focus = msi::first_selectable_widget();
            focus = msi::find_widget(msi::g_focus);
        }
        editing_focus = msi::g_text_edit_active && focus && focus->id == msi::g_text_edit_widget;
        if (msi::g_text_edit_active && !editing_focus) {
            WidgetId editing_id = msi::current_text_widget();
            bool modified = msi::end_text_edit();
            if (handle_text_commit(editing_id, modified) || modified) {
                needs_rebuild = true;
                continue;
            }
        }

        if (!focus) {
            msi::g_focus =
                (!msi::g_cache.widgets.empty()) ? msi::g_cache.widgets.front().id : kMenuIdInvalid;
            focus = msi::find_widget(msi::g_focus);
        }

        auto apply_slider_position = [&](MenuWidget& slider,
                                        float fx,
                                        bool begin_drag,
                                        bool persist_now) -> bool {
            SDL_FRect* rect_ptr = msi::find_widget_rect(slider.id);
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
                menu_system_internal::g_slider_drag_value = target_value;
                menu_system_internal::g_slider_drag_value_valid = true;
                menu_system_internal::g_slider_commit_pending = true;
                msi::execute_action(slider.on_select, ctx, stack_changed);
                menu_system_internal::g_slider_commit_pending = false;
                menu_system_internal::g_slider_drag_value_valid = false;
            } else if (slider.bind_ptr) {
                *reinterpret_cast<float*>(slider.bind_ptr) = target_value;
                if (slider.text_buffer && !msi::g_text_edit_active) {
                    float shown = target_value * slider.display_scale + slider.display_offset;
                    int precision = std::max(0, slider.display_precision);
                    char buffer[64];
                    if (precision == 0)
                        std::snprintf(buffer, sizeof(buffer), "%.0f", static_cast<double>(shown));
                    else
                        std::snprintf(buffer, sizeof(buffer), "%.*f", precision, static_cast<double>(shown));
                    *slider.text_buffer = buffer;
                }
            }
            if (begin_drag) {
                menu_system_internal::g_slider_drag_id = slider.id;
            }
            if (persist_now)
                needs_rebuild = true;
            return true;
        };

        if (allow_mouse_input) {
            WidgetId hovered = kMenuIdInvalid;
            for (std::size_t i = 0; i < msi::g_cache.widgets.size() && i < msi::g_cache.rects.size(); ++i) {
                const MenuWidget& widget = msi::g_cache.widgets[i];
                if (widget.type == WidgetType::Label)
                    continue;
                const SDL_FRect& rect = msi::g_cache.rects[i];
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                bool inside = fx >= rect.x && fx <= rect.x + rect.w &&
                              fy >= rect.y && fy <= rect.y + rect.h;
                if (inside) {
                    hovered = widget.id;
                    break;
                }
            }
            bool hover_changed = hovered != msi::g_focus;
            if (msi::g_allow_mouse_focus && hover_changed &&
                hovered != kMenuIdInvalid && !mouse_down) {
                msi::g_focus = hovered;
                focus = msi::find_widget(msi::g_focus);
            }
            if (hovered != kMenuIdInvalid && mouse_clicked) {
                msi::g_focus = hovered;
                focus = msi::find_widget(msi::g_focus);
                bool click_handled = false;
                if (focus && focus->type == WidgetType::OptionCycle) {
                    SDL_FRect* rect_ptr = msi::find_widget_rect(focus->id);
                    if (rect_ptr) {
                        msi::OptionLayout opt_layout = msi::compute_option_layout(*focus, *rect_ptr);
                        float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                        float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                        auto trigger_action = [&](const MenuAction& action, auto sound_fn) -> bool {
                            if (action.type == MenuActionType::None)
                                return false;
                            sound_fn();
                            msi::execute_action(action, ctx, stack_changed);
                            needs_rebuild = true;
                            return true;
                        };
                        if (msi::point_in_rect(fx, fy, opt_layout.left_btn)) {
                            if (trigger_action(focus->on_left, []() { msi::play_left_sound(); }))
                                continue;
                        } else if (msi::point_in_rect(fx, fy, opt_layout.right_btn)) {
                            if (trigger_action(focus->on_right, []() { msi::play_right_sound(); }))
                                continue;
                        } else if (msi::point_in_rect(fx, fy, opt_layout.value_rect)) {
                            if (trigger_action(focus->on_select, []() { msi::play_confirm_sound(); }))
                                continue;
                        } else if (opt_layout.has_primary_input &&
                                   msi::point_in_rect(fx, fy, opt_layout.primary_input)) {
                            msi::begin_text_edit(*focus, false);
                            continue;
                        } else if (opt_layout.has_secondary_input &&
                                   msi::point_in_rect(fx, fy, opt_layout.secondary_input)) {
                            msi::begin_text_edit(*focus, true);
                            continue;
                        }
                    }
                }
                if (focus && focus->type == WidgetType::Slider1D) {
                    SDL_FRect* rect_ptr = msi::find_widget_rect(focus->id);
                    if (rect_ptr) {
                        auto slider_layout = msi::compute_slider_layout(*focus, *rect_ptr);
                        float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                        float fy = has_render_mouse ? render_mouse_y : static_cast<float>(mouse_y);
                        auto trigger_action = [&](const MenuAction& action, auto sound_fn) -> bool {
                            if (action.type == MenuActionType::None)
                                return false;
                            sound_fn();
                            msi::execute_action(action, ctx, stack_changed);
                            needs_rebuild = true;
                            return true;
                        };
                        if (focus->has_discrete_options && slider_layout.has_buttons) {
                            if (msi::point_in_rect(fx, fy, slider_layout.left_btn)) {
                                if (trigger_action(focus->on_left, []() { msi::play_left_sound(); }))
                                    continue;
                            } else if (msi::point_in_rect(fx, fy, slider_layout.right_btn)) {
                                if (trigger_action(focus->on_right, []() { msi::play_right_sound(); }))
                                    continue;
                            }
                        }
                        if (slider_layout.has_input && msi::point_in_rect(fx, fy, slider_layout.input_rect)) {
                            msi::begin_text_edit(*focus);
                            click_handled = true;
                            continue;
                        }
                        SDL_FRect track_rect{slider_layout.track_left,
                                             slider_layout.track_y - 10.0f,
                                             slider_layout.track_right - slider_layout.track_left,
                                             20.0f};
                        if (track_rect.w > 4.0f &&
                            msi::point_in_rect(fx, fy, track_rect)) {
                            apply_slider_position(*focus, fx, true, false);
                            click_handled = true;
                        }
                    }
                }
                if (focus && focus->text_buffer && focus->select_enters_text && !click_handled) {
                    msi::begin_text_edit(*focus);
                    click_handled = true;
                }
                if (!click_handled) {
                    WidgetId editing_id = msi::current_text_widget();
                    bool modified = msi::end_text_edit();
                    if (handle_text_commit(editing_id, modified) || modified) {
                        needs_rebuild = true;
                        continue;
                    }
                }
                if (focus && focus->on_select.type != MenuActionType::None) {
                    msi::play_confirm_sound();
                    click_handled = true;
                    msi::execute_action(focus->on_select, ctx, stack_changed);
                    needs_rebuild = true;
                    continue;
                } else if (!click_handled) {
                    msi::play_cant_sound();
                }
            }
        }

        if (!mouse_down && menu_system_internal::g_slider_drag_id != kMenuIdInvalid) {
            MenuWidget* dragging = msi::find_widget(menu_system_internal::g_slider_drag_id);
            if (dragging && dragging->type == WidgetType::Slider1D) {
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                if (apply_slider_position(*dragging, fx, false, true)) {
                    menu_system_internal::g_slider_drag_id = kMenuIdInvalid;
                    continue;
                }
            }
            menu_system_internal::g_slider_drag_id = kMenuIdInvalid;
        } else if (mouse_down && menu_system_internal::g_slider_drag_id != kMenuIdInvalid) {
            MenuWidget* dragging = msi::find_widget(menu_system_internal::g_slider_drag_id);
            if (dragging && dragging->type == WidgetType::Slider1D) {
                float fx = has_render_mouse ? render_mouse_x : static_cast<float>(mouse_x);
                apply_slider_position(*dragging, fx, false, false);
            }
        }

        if (msi::g_current_screen != kMenuIdInvalid && focus)
            msi::g_last_focus[msi::g_current_screen] = focus->id;

    }

    msi::update_arrows(dt);

    if (msi::g_focus != prev_focus_frame && msi::g_focus != kMenuIdInvalid)
        msi::play_focus_sound();

    if (manager.stack().empty()) {
        msi::g_current_screen = kMenuIdInvalid;
        msi::end_text_edit();
        msi::g_last_focus.clear();
        msi::g_cache.widgets.clear();
        msi::g_cache.layout = kMenuIdInvalid;
        msi::g_focus = kMenuIdInvalid;
        msi::g_cache.rects.clear();
        msi::g_active_text_buffer = nullptr;
        msi::g_arrows.initialized = false;
        msi::g_has_focus_rect = false;
        msi::g_has_focus_color = false;
        msi::g_allow_mouse_focus = true;
        msi::g_mouse_focus_locked = false;
    }
}

void menu_system_handle_text_input(const char* text) {
    if (!msi::g_text_edit_active || !msi::g_active_text_buffer || !text)
        return;
    while (*text) {
        if (msi::g_active_text_max <= 0 ||
            static_cast<int>(msi::g_active_text_buffer->size()) < msi::g_active_text_max)
            msi::g_active_text_buffer->push_back(*text);
        ++text;
    }
}

void menu_system_handle_backspace(bool clear_all) {
    if (!msi::g_active_text_buffer) {
        if (msi::g_focus != kMenuIdInvalid) {
            MenuWidget* focus_widget = msi::find_widget(msi::g_focus);
            if (focus_widget && focus_widget->type == WidgetType::TextInput)
                msi::begin_text_edit(*focus_widget);
        }
    }
    if (!msi::g_text_edit_active || !msi::g_active_text_buffer)
        return;

    if (clear_all) {
        msi::g_active_text_buffer->clear();
    } else if (!msi::g_active_text_buffer->empty()) {
        msi::g_active_text_buffer->pop_back();
    }
}

void menu_system_reset() {
    msi::end_text_edit();
    msi::g_last_focus.clear();
    msi::g_cache.widgets.clear();
    msi::g_cache.layout = kMenuIdInvalid;
    msi::g_cache.rects.clear();
    msi::g_focus = kMenuIdInvalid;
    msi::g_prev_input = {};
    msi::g_current_input = {};
    msi::g_active_text_buffer = nullptr;
    if (msi::g_text_input_enabled) {
        SDL_StopTextInput();
        msi::g_text_input_enabled = false;
    }
    msi::g_allow_mouse_focus = true;
    msi::g_mouse_focus_locked = false;
    msi::g_mouse_focus_lock_x = 0;
    msi::g_mouse_focus_lock_y = 0;
}

bool menu_system_active() {
    return msi::g_active;
}
