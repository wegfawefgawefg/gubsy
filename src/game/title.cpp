#include "game/title.hpp"

#include "engine/globals.hpp"
#include "game/modes.hpp"

#include <SDL2/SDL_render.h>
#include <imgui.h>

namespace {

void draw_menu_content(int width, int height) {
    if (!ImGui::GetCurrentContext())
        return;

    const float window_width = 360.0f;
    const float window_height = 220.0f;
    const float wx = static_cast<float>(width);
    const float wy = static_cast<float>(height);
    const ImVec2 pos{(wx - window_width) * 0.5f, (wy - window_height) * 0.5f};
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("gubsy_main_menu", nullptr, flags)) {
        ImGui::Text("gubsy demo");
        ImGui::Separator();
        if (ImGui::Button("Play", ImVec2(-1.0f, 0.0f))) {
            if (es)
                es->mode = modes::SETUP;
        }

        ImGui::BeginDisabled(true);
        ImGui::Button("Settings", ImVec2(-1.0f, 0.0f));
        ImGui::Button("Quit", ImVec2(-1.0f, 0.0f));
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.4f, 1.0f),
                           "Settings and Quit will arrive after the flow is finalized.");
    }
    ImGui::End();
}

} // namespace

void title_step() {}

void title_process_inputs() {}

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
