#include "src/input_system.hpp"

#include <SDL3/SDL_keyboard.h>

#include <algorithm>
#include "src/engine_state.hpp"
#include "src/graphics.hpp"

namespace {

void update_mouse_projection(EngineState& engine) {
    auto& state = engine.device_state;
    state.mouse_norm = glm::vec2(0.0f);
    state.mouse_norm_render = glm::vec2(0.0f);
    state.mouse_render_pos = glm::vec2(0.0f);
    state.has_mouse_render_pos = false;

    const Graphics* graphics = current_graphics(engine);
    if (!graphics || !graphics->renderer)
        return;

    int window_w = std::max(static_cast<int>(graphics->window_dims.x), 2);
    int window_h = std::max(static_cast<int>(graphics->window_dims.y), 2);
    float norm_x = (static_cast<float>(state.mouse_x) / static_cast<float>(window_w - 1)) * 2.0f - 1.0f;
    float norm_y = (static_cast<float>(state.mouse_y) / static_cast<float>(window_h - 1)) * 2.0f - 1.0f;
    state.mouse_norm.x = std::clamp(norm_x, -1.0f, 1.0f);
    state.mouse_norm.y = std::clamp(norm_y, -1.0f, 1.0f);

    float render_w = static_cast<float>(graphics->render_dims.x);
    float render_h = static_cast<float>(graphics->render_dims.y);
    if (render_w <= 1.0f || render_h <= 1.0f)
        return;

    if (graphics->render_scale_mode == RenderScaleMode::Stretch) {
        float local_x = static_cast<float>(state.mouse_x) / static_cast<float>(window_w);
        float local_y = static_cast<float>(state.mouse_y) / static_cast<float>(window_h);
        state.mouse_render_pos.x = std::clamp(local_x, 0.0f, 1.0f) * (render_w - 1.0f);
        state.mouse_render_pos.y = std::clamp(local_y, 0.0f, 1.0f) * (render_h - 1.0f);
        state.has_mouse_render_pos = true;
    } else {
        float src_aspect = render_w / render_h;
        float dst_aspect = static_cast<float>(window_w) / static_cast<float>(window_h);
        float draw_x = 0.0f;
        float draw_y = 0.0f;
        float draw_w = static_cast<float>(window_w);
        float draw_h = static_cast<float>(window_h);
        if (dst_aspect >= src_aspect) {
            draw_h = static_cast<float>(window_h);
            draw_w = draw_h * src_aspect;
            draw_x = (static_cast<float>(window_w) - draw_w) * 0.5f;
        } else {
            draw_w = static_cast<float>(window_w);
            draw_h = draw_w / src_aspect;
            draw_y = (static_cast<float>(window_h) - draw_h) * 0.5f;
        }

        float pad_left = std::clamp(graphics->safe_area.x, 0.0f, 0.45f) * static_cast<float>(window_w);
        float pad_right = std::clamp(graphics->safe_area.y, 0.0f, 0.45f) * static_cast<float>(window_w);
        float pad_top = std::clamp(graphics->safe_area.z, 0.0f, 0.45f) * static_cast<float>(window_h);
        float pad_bottom = std::clamp(graphics->safe_area.w, 0.0f, 0.45f) * static_cast<float>(window_h);

        draw_x += pad_left;
        draw_y += pad_top;
        draw_w = std::max(4.0f, draw_w - (pad_left + pad_right));
        draw_h = std::max(4.0f, draw_h - (pad_top + pad_bottom));

        float zoom = std::max(0.1f, graphics->preview_zoom);
        float center_x = draw_x + draw_w * 0.5f;
        float center_y = draw_y + draw_h * 0.5f;
        draw_w *= zoom;
        draw_h *= zoom;
        draw_x = center_x - draw_w * 0.5f + graphics->preview_pan.x;
        draw_y = center_y - draw_h * 0.5f + graphics->preview_pan.y;

        float mouse_x = static_cast<float>(state.mouse_x);
        float mouse_y = static_cast<float>(state.mouse_y);
        if (mouse_x >= draw_x && mouse_y >= draw_y &&
            mouse_x <= draw_x + draw_w && mouse_y <= draw_y + draw_h) {
            float local_x = (mouse_x - draw_x) / draw_w;
            float local_y = (mouse_y - draw_y) / draw_h;
            state.mouse_render_pos.x = std::clamp(local_x, 0.0f, 1.0f) * (render_w - 1.0f);
            state.mouse_render_pos.y = std::clamp(local_y, 0.0f, 1.0f) * (render_h - 1.0f);
            state.has_mouse_render_pos = true;
        }
    }

    if (state.has_mouse_render_pos) {
        float render_norm_x = (state.mouse_render_pos.x / (render_w - 1.0f)) * 2.0f - 1.0f;
        float render_norm_y = (state.mouse_render_pos.y / (render_h - 1.0f)) * 2.0f - 1.0f;
        state.mouse_norm_render.x = std::clamp(render_norm_x, -1.0f, 1.0f);
        state.mouse_norm_render.y = std::clamp(render_norm_y, -1.0f, 1.0f);
    }
}

} // namespace

void update_device_state_from_sdl(EngineState& engine) {
    auto& state = engine.device_state;
    const bool* sdl_keystate = SDL_GetKeyboardState(nullptr);
    for (int i = 0; i < SDL_SCANCODE_COUNT; ++i)
        state.keyboard[static_cast<std::size_t>(i)] = sdl_keystate[i] ? 1 : 0;

    float x = 0.0f;
    float y = 0.0f;
    state.mouse_buttons = SDL_GetMouseState(&x, &y);
    int mouse_x = static_cast<int>(x);
    int mouse_y = static_cast<int>(y);
    state.mouse_dx = mouse_x - state.mouse_x;
    state.mouse_dy = mouse_y - state.mouse_y;
    state.mouse_x = mouse_x;
    state.mouse_y = mouse_y;
    state.controllers.clear();
    state.controllers.reserve(engine.open_controllers.size());
    for (auto const& [device_id, controller] : engine.open_controllers) {
        DeviceState::ControllerState controller_state{};
        controller_state.device_id = device_id;
        for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
            Sint16 raw_value = SDL_GetGamepadAxis(controller, static_cast<SDL_GamepadAxis>(axis));
            controller_state.axes[static_cast<std::size_t>(axis)] =
                static_cast<float>(raw_value) / 32767.0f;
        }
        for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
            controller_state.buttons[static_cast<std::size_t>(button)] =
                SDL_GetGamepadButton(controller, static_cast<SDL_GamepadButton>(button)) ? 1 : 0;
        }
        state.controllers.push_back(controller_state);
    }

    update_mouse_projection(engine);
}

void accumulate_mouse_wheel_delta(EngineState& engine, int delta) {
    engine.device_state.mouse_wheel += delta;
}
