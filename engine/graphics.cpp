#include "engine/graphics.hpp"
#include "engine/engine_state.hpp"
#include "engine/project_paths.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

Graphics* current_graphics(EngineState& engine) {
    return engine.graphics;
}

const Graphics* current_graphics(const EngineState& engine) {
    return engine.graphics;
}

bool init_font(const std::filesystem::path& fonts_dir, int pt_size) {
    (void)fonts_dir;
    (void)pt_size;
    return false;
}

namespace {

bool init_font_for_graphics(Graphics& graphics,
                            const std::filesystem::path& fonts_dir,
                            int pt_size) {
    if (graphics.ui_font)
        return true;
    if (!TTF_WasInit()) {
        if (!TTF_Init()) {
            std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
            return false;
        }
    }
    std::string font_path;
    std::error_code ec;
    std::filesystem::path fdir = fonts_dir.empty() ? engine_assets_path("fonts") : fonts_dir;
    if (std::filesystem::exists(fdir, ec) && std::filesystem::is_directory(fdir, ec)) {
        std::filesystem::path fallback_path;
        for (auto const& de : std::filesystem::directory_iterator(fdir, ec)) {
            if (ec) { ec.clear(); continue; }
            if (!de.is_regular_file()) continue;
            auto p = de.path();
            auto ext = p.extension().string();
            for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
            if (ext == ".ttf") {
                font_path = p.string();
                break;
            }
            if (ext == ".otf" && fallback_path.empty())
                fallback_path = p;
        }
        if (font_path.empty() && !fallback_path.empty())
            font_path = fallback_path.string();
    }
    if (!font_path.empty()) {
        graphics.ui_font = TTF_OpenFont(font_path.c_str(), pt_size);
        if (!graphics.ui_font) {
            std::fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError());
            return false;
        }
        return true;
    } else {
        std::fprintf(stderr, "No .ttf/.otf found in %s. Numeric countdown will be hidden.\n",
                     fdir.string().c_str());
        return false;
    }
}

int clamp_dimension(int value) {
    if (value < 16)
        return 16;
    return value;
}

bool recreate_render_target(Graphics& graphics, int width, int height) {
    if (!graphics.renderer)
        return false;
    width = clamp_dimension(width);
    height = clamp_dimension(height);
    SDL_Texture* tex = SDL_CreateTexture(graphics.renderer, SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_TARGET, width, height);
    if (!tex) {
        std::fprintf(stderr, "Failed to create render target %dx%d: %s\n",
                     width, height, SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    if (graphics.render_target)
        SDL_DestroyTexture(graphics.render_target);
    graphics.render_target = tex;
    graphics.render_dims = {static_cast<unsigned int>(width), static_cast<unsigned int>(height)};
    return true;
}

} // namespace

bool try_init_video_with_driver(const char* driver) {
    Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS;
    if (!driver || !*driver) {
        if (SDL_InitSubSystem(init_flags))
            return true;
        std::fprintf(stderr, "SDL_InitSubSystem(auto) failed: %s\n", SDL_GetError());
        return false;
    }

    const char* previous = SDL_getenv("SDL_VIDEODRIVER");
    char previous_buf[64]{};
    if (previous && *previous)
        std::snprintf(previous_buf, sizeof(previous_buf), "%s", previous);

    SDL_setenv("SDL_VIDEODRIVER", driver, 1);
    if (SDL_InitSubSystem(init_flags))
        return true;

    std::fprintf(stderr, "SDL_InitSubSystem(%s) failed: %s\n", driver, SDL_GetError());
    if (previous_buf[0] != '\0')
        SDL_setenv("SDL_VIDEODRIVER", previous_buf, 1);
    else
        SDL_setenv("SDL_VIDEODRIVER", "", 1);
    return false;
}

bool init_graphics(EngineState& engine) {
    if (engine.graphics)
        return true;
    engine.graphics = new Graphics{};
    Graphics& graphics = *engine.graphics;

    const char* title = "artificial";
    glm::ivec2 window_dims = {1280, 720};

    graphics.window = nullptr;
    graphics.renderer = nullptr;
    graphics.window_dims = {static_cast<unsigned int>(window_dims.x), static_cast<unsigned int>(window_dims.y)};

    // Initialize SDL video with driver selection
    const char* env_display = std::getenv("DISPLAY");
    const char* env_wayland = std::getenv("WAYLAND_DISPLAY");
    const char* env_sdl_driver = std::getenv("SDL_VIDEODRIVER");

    bool initialized = false;
    if (env_sdl_driver && *env_sdl_driver)
        initialized = try_init_video_with_driver(env_sdl_driver);
    else
        initialized = try_init_video_with_driver(nullptr); // auto-pick

    if (!initialized && env_display && *env_display)
        initialized = try_init_video_with_driver("x11");
    if (!initialized && env_wayland && *env_wayland)
        initialized = try_init_video_with_driver("wayland");
    if (!initialized)
        return false;

    Uint32 win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_UTILITY;
    graphics.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  window_dims.x, window_dims.y, win_flags);
    if (!graphics.window) {
        const char* err = SDL_GetError();
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", (err && *err) ? err : "(no error text)");
        return false;
    }

    SDL_SetWindowAlwaysOnTop(graphics.window, SDL_TRUE);

    graphics.renderer = SDL_CreateRenderer(graphics.window, -1, SDL_RENDERER_ACCELERATED);
    if (!graphics.renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        graphics.renderer = SDL_CreateRenderer(graphics.window, -1, 0); // software fallback
    }
    if (!graphics.renderer)
        return false;

    const char* active_driver = SDL_GetCurrentVideoDriver();
    std::printf("SDL video driver: %s\n", active_driver ? active_driver : "(none)");

    // Initialize default UI font (optional)
    (void)init_font_for_graphics(graphics, {}, 20);
    recreate_render_target(graphics, window_dims.x, window_dims.y);
    return true;
}

void cleanup_graphics(EngineState& engine) {
    Graphics* graphics = current_graphics(engine);
    if (!graphics)
        return;
    if (graphics->ui_font) { TTF_CloseFont(graphics->ui_font); graphics->ui_font = nullptr; }
    // Destroy textures before renderer
    clear_textures(engine);
    if (graphics->render_target) {
        SDL_DestroyTexture(graphics->render_target);
        graphics->render_target = nullptr;
    }
    if (graphics->renderer) {
        SDL_DestroyRenderer(graphics->renderer);
        graphics->renderer = nullptr;
    }
    if (graphics->window) {
        SDL_DestroyWindow(graphics->window);
        graphics->window = nullptr;
    }
    if (TTF_WasInit()) TTF_Quit();
    delete graphics;
    engine.graphics = nullptr;
}

bool set_window_dimensions(EngineState& engine, int width, int height) {
    if (!current_graphics(engine) || !current_graphics(engine)->window)
        return false;
    width = clamp_dimension(width);
    height = clamp_dimension(height);
    SDL_SetWindowSize(current_graphics(engine)->window, width, height);
    int actual_w = width;
    int actual_h = height;
    SDL_GetWindowSize(current_graphics(engine)->window, &actual_w, &actual_h);
    current_graphics(engine)->window_dims = {static_cast<unsigned int>(actual_w),
                       static_cast<unsigned int>(actual_h)};
    return true;
}

bool set_window_display_mode(EngineState& engine, WindowDisplayMode mode) {
    if (!current_graphics(engine) || !current_graphics(engine)->window)
        return false;
    Uint32 flag = 0;
    switch (mode) {
        case WindowDisplayMode::Windowed:
            flag = 0;
            break;
        case WindowDisplayMode::Borderless:
            flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
            break;
        case WindowDisplayMode::Fullscreen:
            flag = SDL_WINDOW_FULLSCREEN;
            break;
    }
    if (!SDL_SetWindowFullscreen(current_graphics(engine)->window, flag)) {
        std::fprintf(stderr, "Failed to change window mode: %s\n", SDL_GetError());
        return false;
    }
    current_graphics(engine)->window_mode = mode;
    int actual_w = 0;
    int actual_h = 0;
    SDL_GetWindowSize(current_graphics(engine)->window, &actual_w, &actual_h);
    current_graphics(engine)->window_dims = {static_cast<unsigned int>(actual_w),
                       static_cast<unsigned int>(actual_h)};
    return true;
}

bool set_render_resolution(EngineState& engine, int width, int height) {
    Graphics* graphics = current_graphics(engine);
    return graphics ? recreate_render_target(*graphics, width, height) : false;
}

void set_render_scale_mode(EngineState& engine, RenderScaleMode mode) {
    if (!current_graphics(engine))
        return;
    current_graphics(engine)->render_scale_mode = mode;
}

glm::ivec2 get_render_dimensions(const EngineState& engine) {
    if (!current_graphics(engine))
        return glm::ivec2(0, 0);
    return glm::ivec2(static_cast<int>(current_graphics(engine)->render_dims.x),
                      static_cast<int>(current_graphics(engine)->render_dims.y));
}

glm::ivec2 get_window_dimensions(const EngineState& engine) {
    if (!current_graphics(engine))
        return glm::ivec2(0, 0);
    return glm::ivec2(static_cast<int>(current_graphics(engine)->window_dims.x),
                      static_cast<int>(current_graphics(engine)->window_dims.y));
}


// ---- Registry ----

void build_sprite_name_id_mapping(EngineState& engine, const std::vector<std::string>& names) {
    current_graphics(engine)->sprite_name_to_id.clear();
    current_graphics(engine)->sprite_id_to_name.clear();
    current_graphics(engine)->sprite_id_to_name.reserve(names.size());
    for (const auto& n : names) {
        int id = static_cast<int>(current_graphics(engine)->sprite_id_to_name.size());
        current_graphics(engine)->sprite_name_to_id.emplace(n, id);
        current_graphics(engine)->sprite_id_to_name.push_back(n);
    }
}

int add_or_get_sprite_id(EngineState& engine, const std::string& name) {
    auto it = current_graphics(engine)->sprite_name_to_id.find(name);
    if (it != current_graphics(engine)->sprite_name_to_id.end()) return it->second;
    int id = static_cast<int>(current_graphics(engine)->sprite_id_to_name.size());
    current_graphics(engine)->sprite_name_to_id.emplace(name, id);
    current_graphics(engine)->sprite_id_to_name.push_back(name);
    return id;
}

int try_get_sprite_id(const EngineState& engine, const std::string& name) {
    auto it = current_graphics(engine)->sprite_name_to_id.find(name);
    return (it == current_graphics(engine)->sprite_name_to_id.end()) ? -1 : it->second;
}

// ---- Sprite definitions ----

void rebuild_sprite_mapping(EngineState& engine, const std::vector<SpriteDef>& new_defs) {

    auto& name_to_id = current_graphics(engine)->sprite_name_to_id;
    auto& id_to_name = current_graphics(engine)->sprite_id_to_name;
    auto& defs_by_id = current_graphics(engine)->sprite_defs_by_id;

    bool only_additions = true;
    if (!name_to_id.empty()) {
        std::unordered_map<std::string, int> new_names;
        new_names.reserve(new_defs.size());
        for (const auto& d : new_defs) new_names.emplace(d.name, 1);
        for (const auto& kv : name_to_id) {
            if (new_names.find(kv.first) == new_names.end()) { only_additions = false; break; }
        }
    }

    std::unordered_map<std::string, int> new_name_to_id;
    std::vector<std::string> new_id_to_name;
    std::vector<SpriteDef> new_defs_by_id;
    new_name_to_id.reserve(new_defs.size());
    new_id_to_name.reserve(new_defs.size());

    if (!name_to_id.empty() && only_additions) {
        new_defs_by_id = std::vector<SpriteDef>(defs_by_id.size());
        new_id_to_name = id_to_name;
        for (const auto& d : new_defs) {
            auto it = name_to_id.find(d.name);
            if (it != name_to_id.end()) {
                int id = it->second;
                if (id >= 0 && static_cast<size_t>(id) < new_defs_by_id.size()) {
                    new_defs_by_id[static_cast<size_t>(id)] = d;
                    new_name_to_id.emplace(d.name, id);
                }
            }
        }
        for (const auto& d : new_defs) {
            if (new_name_to_id.find(d.name) != new_name_to_id.end()) continue;
            int id = static_cast<int>(new_defs_by_id.size());
            new_defs_by_id.push_back(d);
            new_id_to_name.push_back(d.name);
            new_name_to_id.emplace(d.name, id);
        }
    } else {
        new_defs_by_id.reserve(new_defs.size());
        for (const auto& d : new_defs) {
            int id = static_cast<int>(new_defs_by_id.size());
            new_defs_by_id.push_back(d);
            new_name_to_id.emplace(d.name, id);
            new_id_to_name.push_back(d.name);
        }
    }

    name_to_id.swap(new_name_to_id);
    id_to_name.swap(new_id_to_name);
    defs_by_id.swap(new_defs_by_id);
}

const SpriteDef* get_sprite_def_by_id(const EngineState& engine, int id) {
    if (id < 0) return nullptr;
    size_t idx = static_cast<size_t>(id);
    if (idx >= current_graphics(engine)->sprite_defs_by_id.size()) return nullptr;
    return &current_graphics(engine)->sprite_defs_by_id[idx];
}

const SpriteDef* try_get_sprite_def(const EngineState& engine, const std::string& name) {
    int id = try_get_sprite_id(engine, name);
    return get_sprite_def_by_id(engine, id);
}

// ---- Textures ----

void clear_textures(EngineState& engine) {
    for (auto& kv : current_graphics(engine)->textures_by_id) {
        if (kv.second) SDL_DestroyTexture(kv.second);
    }
    current_graphics(engine)->textures_by_id.clear();
}

/// Runs through the sprite defs from the last mod scan and loads the textures.
/// Heavy. Dont run often.
bool load_all_textures_in_sprite_lookup(EngineState& engine) {
    if (!current_graphics(engine)->renderer) return false;
    for (int id = 0; id < static_cast<int>(current_graphics(engine)->sprite_defs_by_id.size()); ++id) {
        const auto* def = get_sprite_def_by_id(engine, id);
        if (!def) continue;
        if (def->image_path.empty()) continue;
        SDL_Texture* tex = IMG_LoadTexture(current_graphics(engine)->renderer, def->image_path.c_str());
        if (!tex) {
            std::fprintf(stderr, "IMG_LoadTexture failed for %s: %s\n", def->image_path.c_str(),
                         IMG_GetError());
            continue;
        }
        current_graphics(engine)->textures_by_id[id] = tex;
    }
    return true;
}

SDL_Texture* get_texture(EngineState& engine, int sprite_id) {
    auto it = current_graphics(engine)->textures_by_id.find(sprite_id);
    return (it == current_graphics(engine)->textures_by_id.end()) ? nullptr : it->second;
}

void sync_graphics_from_settings(EngineState& engine) {
    if (!current_graphics(engine))
        return;

    const auto& settings = engine.top_level_game_settings.settings;

    // Sync preview zoom
    if (auto it = settings.find("gubsy.video.preview_zoom"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->preview_zoom = *fv;
    }

    // Sync preview pan X
    if (auto it = settings.find("gubsy.video.preview_pan_x"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->preview_pan.x = *fv;
    }

    // Sync preview pan Y
    if (auto it = settings.find("gubsy.video.preview_pan_y"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->preview_pan.y = *fv;
    }

    // Sync safe area (left, right, top, bottom -> x, y, z, w)
    if (auto it = settings.find("gubsy.video.safe_area_left"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->safe_area.x = *fv;
    }
    if (auto it = settings.find("gubsy.video.safe_area_right"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->safe_area.y = *fv;
    }
    if (auto it = settings.find("gubsy.video.safe_area_top"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->safe_area.z = *fv;
    }
    if (auto it = settings.find("gubsy.video.safe_area_bottom"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            current_graphics(engine)->safe_area.w = *fv;
    }

    // Sync render scale mode
    if (auto it = settings.find("gubsy.video.render_scale_mode"); it != settings.end()) {
        if (const std::string* sv = std::get_if<std::string>(&it->second)) {
            if (*sv == "fit")
                current_graphics(engine)->render_scale_mode = RenderScaleMode::Fit;
            else if (*sv == "stretch")
                current_graphics(engine)->render_scale_mode = RenderScaleMode::Stretch;
        }
    }

    // Sync audio volumes
    if (auto it = settings.find("gubsy.audio.master_volume"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            engine.audio_settings.vol_master = *fv;
    }
    if (auto it = settings.find("gubsy.audio.music_volume"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            engine.audio_settings.vol_music = *fv;
    }
    if (auto it = settings.find("gubsy.audio.sfx_volume"); it != settings.end()) {
        if (const float* fv = std::get_if<float>(&it->second))
            engine.audio_settings.vol_sfx = *fv;
    }
}

#include <limits>
