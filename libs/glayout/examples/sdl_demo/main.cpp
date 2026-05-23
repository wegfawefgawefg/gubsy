#include "glayout/editor.hpp"
#include "glayout/layout.hpp"

#include <SDL3/SDL.h>

#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
#include "glayout/imgui.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kTitlePage = 100;
constexpr int kSettingsPage = 200;
constexpr int kCreditsPage = 300;

struct PreviewPreset {
    const char* label;
    int width;
    int height;
    glayout::FormFactor form_factor;
};

const std::array<PreviewPreset, 3> kPreviewPresets{{
    {"desktop 1920x1080", 1920, 1080, glayout::FormFactor::Desktop},
    {"tablet 1536x2048", 1536, 2048, glayout::FormFactor::Tablet},
    {"phone 1080x1920", 1080, 1920, glayout::FormFactor::Phone},
}};

const char* page_name(int page_id) {
    switch (page_id) {
    case kTitlePage:
        return "Title";
    case kSettingsPage:
        return "Settings";
    case kCreditsPage:
        return "Credits";
    default:
        return "Unknown";
    }
}

int next_page(int page_id) {
    switch (page_id) {
    case kTitlePage:
        return kSettingsPage;
    case kSettingsPage:
        return kCreditsPage;
    default:
        return kTitlePage;
    }
}

std::filesystem::path demo_layout_path() {
    return std::filesystem::path(GLAYOUT_SDL_DEMO_DATA_DIR) / "layouts.lisp";
}

int best_layout_index(const std::vector<glayout::Layout>& layouts, int page_id, int width,
                      int height, glayout::FormFactor form_factor) {
    const glayout::Layout* best =
        glayout::find_best_layout(layouts, page_id, width, height, form_factor);
    if (!best)
        return -1;

    for (std::size_t i = 0; i < layouts.size(); ++i) {
        if (&layouts[i] == best)
            return static_cast<int>(i);
    }
    return -1;
}

SDL_FRect to_sdl_rect(glayout::Rect rect) {
    return SDL_FRect{rect.x, rect.y, rect.w, rect.h};
}

void set_color(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void draw_text(SDL_Renderer* renderer, float x, float y, const char* text) {
    SDL_RenderDebugText(renderer, x, y, text);
}

void draw_textf(SDL_Renderer* renderer, float x, float y, const char* fmt, const char* a) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), fmt, a);
    draw_text(renderer, x, y, buffer);
}

void draw_grid(SDL_Renderer* renderer, glayout::Viewport viewport, float step) {
    if (step <= 0.0f)
        return;

    int index = 1;
    for (float x = step; x < 1.0f; x += step, ++index) {
        Uint8 shade = index % 5 == 0 ? 78 : 42;
        set_color(renderer, shade, shade + 8, shade + 14, 255);
        float screen_x = viewport.x + x * viewport.w;
        SDL_RenderLine(renderer, screen_x, viewport.y, screen_x, viewport.y + viewport.h);
    }

    index = 1;
    for (float y = step; y < 1.0f; y += step, ++index) {
        Uint8 shade = index % 5 == 0 ? 78 : 42;
        set_color(renderer, shade, shade + 8, shade + 14, 255);
        float screen_y = viewport.y + y * viewport.h;
        SDL_RenderLine(renderer, viewport.x, screen_y, viewport.x + viewport.w, screen_y);
    }
}

void draw_layout(SDL_Renderer* renderer, const glayout::Layout& layout,
                 const glayout::EditorState& editor, int focused_object, bool edit_mode,
                 int window_w, int window_h) {
    glayout::Viewport viewport{0.0f, 0.0f, static_cast<float>(window_w),
                               static_cast<float>(window_h)};
    if (edit_mode && editor.snap_enabled)
        draw_grid(renderer, viewport, editor.grid_step);

    std::vector<glayout::OverlayObject> overlays =
        glayout::editor_collect_overlay_objects(editor, layout, viewport);

    for (const glayout::OverlayObject& overlay : overlays) {
        const glayout::Object& object =
            layout.objects[static_cast<std::size_t>(overlay.object_index)];
        SDL_FRect rect = to_sdl_rect(overlay.rect);

        if (overlay.object_index == focused_object) {
            set_color(renderer, 210, 164, 55, 180);
        } else if (object.label.find("panel") != std::string::npos) {
            set_color(renderer, 58, 80, 94, 180);
        } else {
            set_color(renderer, 50, 92, 140, 180);
        }
        SDL_RenderFillRect(renderer, &rect);

        if (edit_mode && overlay.selected) {
            set_color(renderer, 235, 80, 70, 255);
        } else {
            set_color(renderer, 210, 220, 225, 255);
        }
        SDL_RenderRect(renderer, &rect);

        set_color(renderer, 245, 245, 245, 255);
        draw_text(renderer, rect.x + 8.0f, rect.y + 8.0f, object.label.c_str());
    }

    if (edit_mode) {
        std::vector<glayout::OverlayHandle> handles =
            glayout::editor_collect_overlay_handles(editor, layout, viewport);
        set_color(renderer, 255, 95, 82, 255);
        for (const glayout::OverlayHandle& handle : handles) {
            SDL_FRect rect = to_sdl_rect(handle.rect);
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

void activate_focused(const glayout::Layout& layout, int focused_object, int& page_id) {
    if (focused_object < 0 || focused_object >= static_cast<int>(layout.objects.size()))
        return;

    const std::string& label = layout.objects[static_cast<std::size_t>(focused_object)].label;
    if (label == "settings") {
        page_id = kSettingsPage;
    } else if (label == "credits") {
        page_id = kCreditsPage;
    } else if (label == "back") {
        page_id = kTitlePage;
    }
}

} // namespace

int main(int, char**) {
    std::filesystem::path layout_path = demo_layout_path();
    glayout::ParseResult parsed = glayout::load_layout_file(layout_path);
    if (!parsed.ok) {
        for (const glayout::Diagnostic& diagnostic : parsed.diagnostics) {
            std::cerr << diagnostic.line << ":" << diagnostic.column << ": " << diagnostic.message
                      << "\n";
        }
        return 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window =
        SDL_CreateWindow("glayout SDL demo", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_UTILITY);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<glayout::Layout> layouts = std::move(parsed.layouts);
    glayout::EditorState editor;
    int page_id = kTitlePage;
    int focused_object = 0;
    int preview_index = 0;
    bool edit_mode = false;
    bool running = true;

#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
    int imgui_selected_layout = 0;
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
#endif

    while (running) {
        glayout::EditorInput editor_input;
#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
        const ImGuiIO& imgui_io = ImGui::GetIO();
        bool imgui_wants_mouse = imgui_io.WantCaptureMouse;
        bool imgui_wants_keyboard = imgui_io.WantCaptureKeyboard;
#else
        bool imgui_wants_mouse = false;
        bool imgui_wants_keyboard = false;
#endif
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
            ImGui_ImplSDL3_ProcessEvent(&event);
#endif
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (imgui_wants_keyboard && event.key.key != SDLK_ESCAPE)
                    continue;

                switch (event.key.key) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_TAB:
                    page_id = next_page(page_id);
                    focused_object = 0;
                    glayout::editor_clear_selection(editor);
                    break;
                case SDLK_1:
                    page_id = kTitlePage;
                    focused_object = 0;
                    glayout::editor_clear_selection(editor);
                    break;
                case SDLK_2:
                    page_id = kSettingsPage;
                    focused_object = 0;
                    glayout::editor_clear_selection(editor);
                    break;
                case SDLK_3:
                    page_id = kCreditsPage;
                    focused_object = 0;
                    glayout::editor_clear_selection(editor);
                    break;
                case SDLK_E:
                    edit_mode = !edit_mode;
                    break;
                case SDLK_P:
                    preview_index = (preview_index + 1) % static_cast<int>(kPreviewPresets.size());
                    break;
                case SDLK_S:
                    editor_input.key_save = true;
                    break;
                case SDLK_Z:
                    editor_input.key_undo = true;
                    break;
                case SDLK_Y:
                    editor_input.key_redo = true;
                    break;
                case SDLK_DELETE:
                case SDLK_BACKSPACE:
                    editor_input.key_delete = true;
                    break;
                case SDLK_C:
                    editor_input.key_copy = edit_mode;
                    break;
                case SDLK_V:
                    editor_input.key_paste = edit_mode;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: {
                    int layout_index =
                        best_layout_index(layouts, page_id, kPreviewPresets[preview_index].width,
                                          kPreviewPresets[preview_index].height,
                                          kPreviewPresets[preview_index].form_factor);
                    if (layout_index >= 0)
                        activate_focused(layouts[static_cast<std::size_t>(layout_index)],
                                         focused_object, page_id);
                    break;
                }
                case SDLK_UP:
                case SDLK_LEFT:
                    focused_object = std::max(0, focused_object - 1);
                    break;
                case SDLK_DOWN:
                case SDLK_RIGHT:
                    ++focused_object;
                    break;
                default:
                    break;
                }
            }
        }

        float mouse_x = 0.0f;
        float mouse_y = 0.0f;
        SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
        editor_input.mouse_x = mouse_x;
        editor_input.mouse_y = mouse_y;
        editor_input.left_down = !imgui_wants_mouse && (buttons & SDL_BUTTON_LMASK) != 0;

        int window_w = 0;
        int window_h = 0;
        SDL_GetWindowSize(window, &window_w, &window_h);
        const PreviewPreset& preview = kPreviewPresets[static_cast<std::size_t>(preview_index)];
        int layout_index =
            best_layout_index(layouts, page_id, preview.width, preview.height, preview.form_factor);
        if (layout_index < 0) {
#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
#endif
            set_color(renderer, 18, 22, 26, 255);
            SDL_RenderClear(renderer);
            set_color(renderer, 245, 245, 245, 255);
            draw_text(renderer, 20.0f, 20.0f, "No matching layout.");
#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
            ImGui::Render();
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
#endif
            SDL_RenderPresent(renderer);
            SDL_Delay(16);
            continue;
        }

        glayout::Layout& layout = layouts[static_cast<std::size_t>(layout_index)];
        focused_object = std::clamp(focused_object, 0, static_cast<int>(layout.objects.size()) - 1);

#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
#endif

        if (edit_mode) {
            glayout::editor_begin_frame(editor, layout, editor_input,
                                        glayout::Viewport{
                                            0.0f,
                                            0.0f,
                                            static_cast<float>(window_w),
                                            static_cast<float>(window_h),
                                        });
        }

#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
        if (edit_mode) {
            imgui_selected_layout = layout_index;
            if (glayout::imgui::render_integrated_editor(editor, layouts, imgui_selected_layout)) {
                if (imgui_selected_layout >= 0 &&
                    imgui_selected_layout < static_cast<int>(layouts.size())) {
                    const glayout::Layout& selected =
                        layouts[static_cast<std::size_t>(imgui_selected_layout)];
                    page_id = selected.id;
                    focused_object = 0;
                    for (int i = 0; i < static_cast<int>(kPreviewPresets.size()); ++i) {
                        if (kPreviewPresets[static_cast<std::size_t>(i)].form_factor ==
                            selected.form_factor) {
                            preview_index = i;
                            break;
                        }
                    }
                }
            }
        }
#endif

        if (editor.save_requested) {
            if (glayout::save_layout_file(layout_path, layouts))
                glayout::editor_mark_saved(editor);
        }

        set_color(renderer, 18, 22, 26, 255);
        SDL_RenderClear(renderer);
        draw_layout(renderer, layout, editor, focused_object, edit_mode, window_w, window_h);

        set_color(renderer, 245, 245, 245, 255);
        draw_textf(renderer, 14.0f, 14.0f, "page: %s", page_name(page_id));
        draw_textf(renderer, 14.0f, 30.0f, "preview: %s", preview.label);
        draw_text(renderer, 14.0f, 46.0f,
                  edit_mode ? "edit: ON  drag rects, S save, Z/Y undo/redo, C/V copy/paste"
                            : "edit: OFF  E toggles editor");
        draw_text(renderer, 14.0f, 62.0f,
                  "Tab/1/2/3 page, P preview, arrows focus, Enter activate");
        if (editor.dirty) {
            set_color(renderer, 255, 190, 80, 255);
            draw_text(renderer, 14.0f, 78.0f, "dirty");
        }

#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
#endif

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

#if defined(GLAYOUT_SDL_DEMO_WITH_IMGUI)
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
#endif

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
