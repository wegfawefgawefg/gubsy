#include "engine/imgui_layer.hpp"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

namespace {
bool g_imgui_init = false;
SDL_Renderer* g_imgui_renderer = nullptr;
float g_imgui_scale = 1.0f;
ImGuiStyle g_base_style{};
bool g_style_cached = false;
constexpr float kImguiScaleMin = 1.0f;
constexpr float kImguiScaleMax = 4.0f;

void apply_scale() {
    if (!g_imgui_init)
        return;
    ImGuiStyle style = g_base_style;
    style.ScaleAllSizes(g_imgui_scale);
    ImGui::GetStyle() = style;
    ImGui::GetIO().FontGlobalScale = g_imgui_scale;
}
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
    g_base_style = ImGui::GetStyle();
    g_style_cached = true;
    apply_scale();
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
    switch (event.type) {
        case SDL_CONTROLLERAXISMOTION:
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
        case SDL_CONTROLLERDEVICEREMAPPED:
        case SDL_CONTROLLERTOUCHPADDOWN:
        case SDL_CONTROLLERTOUCHPADMOTION:
        case SDL_CONTROLLERTOUCHPADUP:
        case SDL_CONTROLLERSENSORUPDATE:
            return; // Controller input is routed through our binds, not ImGui.
        default:
            break;
    }
    ImGui_ImplSDL2_ProcessEvent(&event);
}

bool imgui_is_initialized() {
    return g_imgui_init;
}

void imgui_set_scale(float scale) {
    float clamped = std::clamp(scale, kImguiScaleMin, kImguiScaleMax);
    float snapped = std::round(clamped);
    g_imgui_scale = std::clamp(snapped, kImguiScaleMin, kImguiScaleMax);
    if (g_style_cached)
        apply_scale();
}

void imgui_adjust_scale(float delta) {
    imgui_set_scale(g_imgui_scale + delta);
}

float imgui_get_scale() {
    return g_imgui_scale;
}
