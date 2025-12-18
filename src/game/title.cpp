#include "game/title.hpp"

#include "engine/globals.hpp"
#include "engine/imgui_layer.hpp"
#include "engine/audio.hpp"
#include "game/modes.hpp"
#include "game/actions.hpp"
#include "engine/player.hpp"
#include "engine/binds_profiles.hpp"
#include "engine/input_binding_utils.hpp"

#include <SDL2/SDL_render.h>
#include <imgui.h>
#include <array>

namespace {

constexpr std::array<int, 6> kNavActions = {
    GameAction::MENU_UP,
    GameAction::MENU_DOWN,
    GameAction::MENU_LEFT,
    GameAction::MENU_RIGHT,
    GameAction::MENU_SELECT,
    GameAction::MENU_BACK,
};

constexpr std::array<int, 2> kScaleActions = {
    GameAction::MENU_SCALE_UP,
    GameAction::MENU_SCALE_DOWN,
};

std::array<bool, GameAction::COUNT> g_menu_action_down{};
std::array<bool, GameAction::COUNT> g_prev_action_down{};
bool g_quit_requested = false;

ImGuiKey imgui_key_for_action(int action) {
    switch (action) {
        case GameAction::MENU_UP: return ImGuiKey_UpArrow;
        case GameAction::MENU_DOWN: return ImGuiKey_DownArrow;
        case GameAction::MENU_LEFT: return ImGuiKey_LeftArrow;
        case GameAction::MENU_RIGHT: return ImGuiKey_RightArrow;
        case GameAction::MENU_SELECT: return ImGuiKey_Enter;
        case GameAction::MENU_BACK: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    }
}

void sync_menu_actions_to_imgui() {
    if (!es || !ImGui::GetCurrentContext())
        return;
    BindsProfile* profile = get_player_binds_profile(0);
    if (!profile)
        return;
    std::array<bool, GameAction::COUNT> current{};
    for (const auto& [device_button, action] : profile->button_binds) {
        if (device_button_is_down(es->device_state, device_button))
            current[static_cast<std::size_t>(action)] = true;
    }
    ImGuiIO& io = ImGui::GetIO();
    for (int action : kNavActions) {
        std::size_t idx = static_cast<std::size_t>(action);
        bool down = current[idx];
        if (down == g_menu_action_down[idx])
            continue;
        g_menu_action_down[idx] = down;
        ImGuiKey key = imgui_key_for_action(action);
        if (key != ImGuiKey_None)
            io.AddKeyEvent(key, down);

        if (down && !g_prev_action_down[idx]) {
            if (action == GameAction::MENU_SELECT)
                play_sound("base:ui_confirm");
            else if (action == GameAction::MENU_BACK)
                play_sound("base:ui_cant");
            else
                play_sound("base:ui_cursor_move");
        }
    }
    for (int action : kScaleActions) {
        std::size_t idx = static_cast<std::size_t>(action);
        bool down = current[idx];
        if (down && !g_menu_action_down[idx]) {
            if (action == GameAction::MENU_SCALE_UP)
                imgui_adjust_scale(1.0f);
            else if (action == GameAction::MENU_SCALE_DOWN)
                imgui_adjust_scale(-1.0f);
        }
        g_menu_action_down[idx] = down;
    }
    g_prev_action_down = g_menu_action_down;
}

void draw_menu_content(int width, int height) {
    if (!ImGui::GetCurrentContext())
        return;

    const float scale = imgui_get_scale();
    const float window_width = 360.0f * scale;
    const float window_height = 180.0f * scale;
    const float wx = static_cast<float>(width);
    const float wy = static_cast<float>(height);
    const ImVec2 pos{(wx - window_width) * 0.5f, (wy - window_height) * 0.5f};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("Main Menu##gubsy_main_menu", nullptr, flags)) {
        ImGui::Spacing();
        if (ImGui::Button("Play", ImVec2(-1.0f, 0.0f))) {
            if (es)
                es->mode = modes::SETUP;
        }
        ImGui::BeginDisabled(true);
        ImGui::Button("Settings", ImVec2(-1.0f, 0.0f));
        ImGui::EndDisabled();
        if (ImGui::Button("Quit", ImVec2(-1.0f, 0.0f)))
            g_quit_requested = true;
    }
    ImGui::End();
}

} // namespace

void title_step() {}

void title_process_inputs() {
    sync_menu_actions_to_imgui();
    if (g_quit_requested && es)
        es->running = false;
}

void title_draw() {
    if (!gg || !gg->renderer)
        return;
    SDL_Renderer* renderer = gg->renderer;
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_SetRenderDrawColor(renderer, 14, 12, 26, 255);
    SDL_RenderClear(renderer);
    draw_menu_content(width, height);
}
