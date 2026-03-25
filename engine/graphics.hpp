#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/sprites.hpp" // for SpriteDef metadata

struct EngineState;

inline constexpr float TILE_SIZE = 16.0f;

struct Camera2D {
    glm::vec2 target{0.0f, 0.0f};
    glm::vec2 offset{0.0f, 0.0f};
    float rotation{0.0f};
    float zoom{2.0f};
};

struct PlayCam {
    glm::vec2 pos{0.0f, 0.0f};
    float zoom{2.0f};
};

enum class RenderScaleMode {
    Fit,
    Stretch,
};

enum class WindowDisplayMode {
    Windowed,
    Borderless,
    Fullscreen,
};

struct Graphics {
    // Windowing
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    TTF_Font* ui_font{nullptr};

    glm::uvec2 window_dims{1280, 720};
    glm::uvec2 render_dims{1280, 720};
    bool fullscreen{false};
    RenderScaleMode render_scale_mode{RenderScaleMode::Fit};
    WindowDisplayMode window_mode{WindowDisplayMode::Windowed};
    SDL_Texture* render_target{nullptr};
    float preview_zoom{1.0f};
    glm::vec2 preview_pan{0.0f, 0.0f};
    glm::vec4 safe_area{0.0f, 0.0f, 0.0f, 0.0f}; // left, right, top, bottom (normalized)

    Camera2D camera{};
    PlayCam play_cam{};

    // Sprite registry and definitions
    std::unordered_map<std::string, int> sprite_name_to_id;
    std::vector<std::string> sprite_id_to_name; // index == id
    std::vector<SpriteDef> sprite_defs_by_id;   // index == id

    // Textures keyed by sprite id
    std::unordered_map<int, SDL_Texture*> textures_by_id;
};

Graphics* current_graphics(EngineState& engine);
const Graphics* current_graphics(const EngineState& engine);

// Initialize window/renderer into Graphics.
// Returns true on success false if windowed init fails.
bool init_graphics(EngineState& engine);

// Destroy renderer/window if present and reset pointers.
void cleanup_graphics(EngineState& engine);

// Initialize UI font into gfx.ui_font by scanning the engine font directory.
bool init_font(const std::filesystem::path& fonts_dir = {}, int pt_size = 20);

// Try initializing SDL video with a specific driver; logs on failure.
bool try_init_video_with_driver(const char* driver);

// Runtime controls
bool set_window_dimensions(EngineState& engine, int width, int height);
bool set_window_display_mode(EngineState& engine, WindowDisplayMode mode);
bool set_render_resolution(EngineState& engine, int width, int height);
void set_render_scale_mode(EngineState& engine, RenderScaleMode mode);
glm::ivec2 get_render_dimensions(const EngineState& engine);
glm::ivec2 get_window_dimensions(const EngineState& engine);

// Sync graphics settings from top-level settings
void sync_graphics_from_settings(EngineState& engine);

// ---- Asset Management ----

// Registry operations
void build_sprite_name_id_mapping(EngineState& engine, const std::vector<std::string>& names);
int add_or_get_sprite_id(EngineState& engine, const std::string& name);
int try_get_sprite_id(const EngineState& engine, const std::string& name);

// Sprite definitions operations
void rebuild_sprite_mapping(EngineState& engine, const std::vector<SpriteDef>& defs);
const SpriteDef* get_sprite_def_by_id(const EngineState& engine, int id);
const SpriteDef* try_get_sprite_def(const EngineState& engine, const std::string& name);

// Textures operations
void clear_textures(EngineState& engine);
bool load_all_textures_in_sprite_lookup(EngineState& engine);
SDL_Texture* get_texture(EngineState& engine, int sprite_id);
