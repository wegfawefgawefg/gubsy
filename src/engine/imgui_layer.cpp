#include "engine/imgui_layer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

namespace {
bool g_imgui_init = false;
SDL_Renderer* g_imgui_renderer = nullptr;
} // namespace

bool init_imgui_layer(SDL_Window* window, SDL_Renderer* renderer) {
    if (g_imgui_init)
        return true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    if (!ImGui_ImplSDL2_InitForSDLRenderer(window, renderer))
        return false;
    if (!ImGui_ImplSDLRenderer2_Init(renderer))
        return false;
    g_imgui_init = true;
    g_imgui_renderer = renderer;
    return true;
}

void shutdown_imgui_layer() {
    if (!g_imgui_init)
        return;
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    g_imgui_init = false;
    g_imgui_renderer = nullptr;
}

void imgui_new_frame() {
    if (!g_imgui_init)
        return;
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void imgui_render_layer() {
    if (!g_imgui_init || !g_imgui_renderer)
        return;
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), g_imgui_renderer);
}

void imgui_process_event(const SDL_Event& event) {
    if (!g_imgui_init)
        return;
    ImGui_ImplSDL2_ProcessEvent(&event);
}

bool imgui_is_initialized() {
    return g_imgui_init;
}
