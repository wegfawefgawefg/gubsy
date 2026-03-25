#include "engine/imgui_debug/windows.hpp"

#include "engine/globals.hpp"
#include "engine/graphics.hpp"
#include "engine/render.hpp"
#include "engine/ui_layouts.hpp"

#include <algorithm>
#include <string>
#include <imgui.h>

namespace {

struct ResolutionPreset {
    const char* label;
    int w;
    int h;
    UILayoutFormFactor bias;
};

struct PresetGroup {
    const char* label;
    int start;
    int count;
};

const char* form_factor_short_tag(UILayoutFormFactor factor) {
    switch (factor) {
        case UILayoutFormFactor::Desktop: return "PC";
        case UILayoutFormFactor::Tablet: return "Tab";
        case UILayoutFormFactor::Phone: return "Mob";
    }
    return "PC";
}

constexpr ResolutionPreset kRenderPresets[] = {
    {"1280x720 (720p)", 1280, 720, UILayoutFormFactor::Desktop},
    {"1366x768 (16:9)", 1366, 768, UILayoutFormFactor::Desktop},
    {"1920x1080 (1080p)", 1920, 1080, UILayoutFormFactor::Desktop},
    {"2560x1440 (1440p)", 2560, 1440, UILayoutFormFactor::Desktop},
    {"3840x2160 (4K)", 3840, 2160, UILayoutFormFactor::Desktop},
    {"7680x4320 (8K)", 7680, 4320, UILayoutFormFactor::Desktop},
    {"1280x800 (16:10)", 1280, 800, UILayoutFormFactor::Tablet},
    {"1440x900 (16:10)", 1440, 900, UILayoutFormFactor::Desktop},
    {"1680x1050 (16:10)", 1680, 1050, UILayoutFormFactor::Desktop},
    {"1920x1200 (WUXGA)", 1920, 1200, UILayoutFormFactor::Desktop},
    {"2560x1600 (WQXGA)", 2560, 1600, UILayoutFormFactor::Desktop},
    {"2880x1800 (Retina)", 2880, 1800, UILayoutFormFactor::Desktop},
    {"2560x1080 (21:9)", 2560, 1080, UILayoutFormFactor::Desktop},
    {"3440x1440 (UWQHD)", 3440, 1440, UILayoutFormFactor::Desktop},
    {"5120x2160 (5K2K)", 5120, 2160, UILayoutFormFactor::Desktop},
    {"3840x1080 (32:9)", 3840, 1080, UILayoutFormFactor::Desktop},
    {"5120x1440 (DQHD)", 5120, 1440, UILayoutFormFactor::Desktop},
    {"1280x720 (Switch HD)", 1280, 720, UILayoutFormFactor::Tablet},
    {"640x480 (SD Consoles)", 640, 480, UILayoutFormFactor::Desktop},
    {"480x272 (PSP)", 480, 272, UILayoutFormFactor::Tablet},
    {"480x270 (PSP Crop)", 480, 270, UILayoutFormFactor::Tablet},
    {"320x240 (N64/PS1)", 320, 240, UILayoutFormFactor::Tablet},
    {"256x192 (Nintendo DS)", 256, 192, UILayoutFormFactor::Tablet},
    {"240x160 (GBA)", 240, 160, UILayoutFormFactor::Tablet},
    {"160x144 (Game Boy)", 160, 144, UILayoutFormFactor::Phone},
    {"720x1280 (HD Phone)", 720, 1280, UILayoutFormFactor::Phone},
    {"1080x1920 (FHD Phone)", 1080, 1920, UILayoutFormFactor::Phone},
    {"1080x2340 (19.5:9)", 1080, 2340, UILayoutFormFactor::Phone},
    {"1080x2400 (20:9)", 1080, 2400, UILayoutFormFactor::Phone},
    {"1170x2532 (iPhone 12/13)", 1170, 2532, UILayoutFormFactor::Phone},
    {"1179x2556 (iPhone 14/15)", 1179, 2556, UILayoutFormFactor::Phone},
    {"1440x3200 (QHD+ Phone)", 1440, 3200, UILayoutFormFactor::Phone},
    {"1536x2048 (iPad)", 1536, 2048, UILayoutFormFactor::Tablet},
    {"1668x2388 (iPad Pro 11\")", 1668, 2388, UILayoutFormFactor::Tablet},
    {"2048x2732 (iPad Pro 12.9\")", 2048, 2732, UILayoutFormFactor::Tablet},
    {"1600x2560 (Android Tablet)", 1600, 2560, UILayoutFormFactor::Tablet},
};

constexpr PresetGroup kRenderPresetGroups[] = {
    {"16:9 Desktop / TV", 0, 6},
    {"16:10 Laptops", 6, 6},
    {"Ultrawide 21:9", 12, 3},
    {"Super Ultrawide 32:9", 15, 2},
    {"Console & Retro Landscape", 17, 7},
    {"Phone Portrait", 24, 8},
    {"Tablet Portrait", 32, 4},
};

constexpr auto& kWindowPresets = kRenderPresets;
constexpr auto& kWindowPresetGroups = kRenderPresetGroups;

void auto_adjust_form_factor_for_resolution(int width, int height) {
    if (width <= 0 || height <= 0)
        return;
    UILayoutFormFactor current = current_ui_layout_form_factor();
    if (height > width) {
        if (current != UILayoutFormFactor::Phone)
            set_ui_layout_form_factor(UILayoutFormFactor::Phone);
        return;
    }
    if (current == UILayoutFormFactor::Phone) {
        UILayoutFormFactor fallback = (width <= 1500)
                                          ? UILayoutFormFactor::Tablet
                                          : UILayoutFormFactor::Desktop;
        set_ui_layout_form_factor(fallback);
    }
}

} // namespace

void imgui_debug_render_video_window(bool* open_flag) {
    if (!open_flag || !*open_flag)
        return;
    if (!ImGui::Begin("Debug: Video & Resolution", open_flag)) {
        ImGui::End();
        return;
    }
    if (!gg || !gg->renderer) {
        ImGui::TextUnformatted("Graphics subsystem unavailable.");
        ImGui::End();
        return;
    }
    glm::ivec2 window_dims = get_window_dimensions();
    glm::ivec2 render_dims = get_render_dimensions();
    ImGui::Text("Window: %d x %d", window_dims.x, window_dims.y);
    ImGui::Text("Render: %d x %d", render_dims.x, render_dims.y);

    static int pending_window_w = 1280;
    static int pending_window_h = 720;
    static int pending_render_w = 1280;
    static int pending_render_h = 720;
    static bool initialized = false;
    if (!initialized) {
        pending_window_w = window_dims.x;
        pending_window_h = window_dims.y;
        pending_render_w = render_dims.x;
        pending_render_h = render_dims.y;
        initialized = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Render Resolution");
    ImGui::InputInt("Render Width", &pending_render_w);
    ImGui::InputInt("Render Height", &pending_render_h);
    static int render_preset_idx = -1;
    auto find_render_preset = [&](int w, int h) -> int {
        for (int i = 0; i < IM_ARRAYSIZE(kRenderPresets); ++i) {
            if (kRenderPresets[i].w == w && kRenderPresets[i].h == h)
                return i;
        }
        return -1;
    };
    auto apply_render_resolution = [&](int w, int h, bool auto_adjust) {
        if (set_render_resolution(w, h)) {
            glm::ivec2 updated = get_render_dimensions();
            w = updated.x;
            h = updated.y;
        }
        pending_render_w = w;
        pending_render_h = h;
        if (auto_adjust)
            auto_adjust_form_factor_for_resolution(pending_render_w, pending_render_h);
    };

    if (ImGui::Button("Apply Render Resolution")) {
        apply_render_resolution(pending_render_w, pending_render_h, true);
        render_preset_idx = find_render_preset(pending_render_w, pending_render_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("Match Window Size##render")) {
        pending_render_w = window_dims.x;
        pending_render_h = window_dims.y;
        apply_render_resolution(pending_render_w, pending_render_h, true);
        render_preset_idx = find_render_preset(pending_render_w, pending_render_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Render##render")) {
        pending_render_w = 1280;
        pending_render_h = 720;
        apply_render_resolution(pending_render_w, pending_render_h, true);
        render_preset_idx = find_render_preset(pending_render_w, pending_render_h);
    }

    const char* render_preset_label =
        (render_preset_idx >= 0) ? kRenderPresets[render_preset_idx].label : "Custom";
    if (ImGui::BeginCombo("Render Preset", render_preset_label)) {
        for (const auto& group : kRenderPresetGroups) {
            ImGui::SeparatorText(group.label);
            for (int i = 0; i < group.count; ++i) {
                int idx = group.start + i;
                const auto& preset = kRenderPresets[idx];
                std::string entry = "[" + std::string(form_factor_short_tag(preset.bias)) + "] " + preset.label;
                bool selected = (render_preset_idx == idx);
                if (ImGui::Selectable(entry.c_str(), selected)) {
                    render_preset_idx = idx;
                    apply_render_resolution(preset.w, preset.h, false);
                    set_ui_layout_form_factor(preset.bias);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
        }
        if (ImGui::Selectable("Custom", render_preset_idx == -1))
            render_preset_idx = -1;
        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Window");
    ImGui::InputInt("Window Width", &pending_window_w);
    ImGui::InputInt("Window Height", &pending_window_h);
    static int window_preset_idx = -1;
    auto find_window_preset = [&](int w, int h) -> int {
        for (int i = 0; i < IM_ARRAYSIZE(kWindowPresets); ++i) {
            if (kWindowPresets[i].w == w && kWindowPresets[i].h == h)
                return i;
        }
        return -1;
    };
    auto apply_window_size = [&](int w, int h, bool auto_adjust) {
        set_window_dimensions(w, h);
        glm::ivec2 dims_now = get_window_dimensions();
        pending_window_w = dims_now.x;
        pending_window_h = dims_now.y;
        if (auto_adjust)
            auto_adjust_form_factor_for_resolution(pending_window_w, pending_window_h);
    };

    if (ImGui::Button("Apply Window Size")) {
        apply_window_size(pending_window_w, pending_window_h, true);
        window_preset_idx = find_window_preset(pending_window_w, pending_window_h);
    }
    ImGui::SameLine();
    if (ImGui::Button("Match Render Size##window")) {
        pending_window_w = render_dims.x;
        pending_window_h = render_dims.y;
        apply_window_size(pending_window_w, pending_window_h, true);
        window_preset_idx = find_window_preset(pending_window_w, pending_window_h);
    }

    const char* window_preset_label =
        (window_preset_idx >= 0) ? kWindowPresets[window_preset_idx].label : "Custom";
    if (ImGui::BeginCombo("Window Preset", window_preset_label)) {
        for (const auto& group : kWindowPresetGroups) {
            ImGui::SeparatorText(group.label);
            for (int i = 0; i < group.count; ++i) {
                int idx = group.start + i;
                const auto& preset = kWindowPresets[idx];
                std::string entry = "[" + std::string(form_factor_short_tag(preset.bias)) + "] " + preset.label;
                bool selected = (window_preset_idx == idx);
                if (ImGui::Selectable(entry.c_str(), selected)) {
                    window_preset_idx = idx;
                    apply_window_size(preset.w, preset.h, false);
                    set_ui_layout_form_factor(preset.bias);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
        }
        if (ImGui::Selectable("Custom##window", window_preset_idx == -1))
            window_preset_idx = -1;
        ImGui::EndCombo();
    }

    const char* form_labels[] = {"Desktop / TV", "Tablet / Handheld", "Phone / Portrait"};
    int form_factor = static_cast<int>(current_ui_layout_form_factor());
    if (ImGui::Combo("Layout Form Factor", &form_factor, form_labels, IM_ARRAYSIZE(form_labels))) {
        form_factor = std::clamp(form_factor, 0, 2);
        set_ui_layout_form_factor(static_cast<UILayoutFormFactor>(form_factor));
    }

    const char* window_mode_labels[] = {"Windowed", "Borderless", "Fullscreen"};
    int window_mode = static_cast<int>(gg->window_mode);
    if (ImGui::Combo("Window Mode", &window_mode,
                     window_mode_labels, IM_ARRAYSIZE(window_mode_labels))) {
        set_window_display_mode(static_cast<WindowDisplayMode>(window_mode));
    }

    const char* scale_labels[] = {"Fit (letterbox)", "Stretch"};
    int scale_mode = static_cast<int>(gg->render_scale_mode);
    if (ImGui::Combo("Render Scale", &scale_mode, scale_labels, IM_ARRAYSIZE(scale_labels))) {
        set_render_scale_mode(static_cast<RenderScaleMode>(scale_mode));
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Preview adjustments (debug)");
    float zoom = gg->preview_zoom;
    if (ImGui::SliderFloat("Zoom", &zoom, 0.5f, 3.0f, "%.2fx")) {
        gg->preview_zoom = zoom;
    }
    float pan[2] = {gg->preview_pan.x, gg->preview_pan.y};
    if (ImGui::DragFloat2("Pan (pixels)", pan, 1.0f, -1000.0f, 1000.0f)) {
        gg->preview_pan = glm::vec2(pan[0], pan[1]);
    }
    float safe_vals[4] = {gg->safe_area.x, gg->safe_area.y, gg->safe_area.z, gg->safe_area.w};
    if (ImGui::SliderFloat4("Safe area (pct of window)", safe_vals, 0.0f, 0.25f, "%.3f")) {
        gg->safe_area = glm::vec4(safe_vals[0], safe_vals[1], safe_vals[2], safe_vals[3]);
    }
    if (ImGui::Button("Reset Preview Adjustments")) {
        gg->preview_zoom = 1.0f;
        gg->preview_pan = glm::vec2(0.0f);
        gg->safe_area = glm::vec4(0.0f);
    }

    ImGui::End();
}
