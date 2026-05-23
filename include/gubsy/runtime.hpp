#pragma once

#include "gubsy/app.hpp"
#include "gubsy/menu/commands.hpp"
#include "gubsy/menu/ids.hpp"
#include "gubsy/menu/system.hpp"

struct SDL_Renderer;
struct SDL_Window;

struct EngineState;

class GubsyRuntime {
public:
    GubsyRuntime();
    ~GubsyRuntime();

    GubsyRuntime(const GubsyRuntime&) = delete;
    GubsyRuntime& operator=(const GubsyRuntime&) = delete;

    GubsyRuntime(GubsyRuntime&& other) noexcept;
    GubsyRuntime& operator=(GubsyRuntime&& other) noexcept;

private:
    EngineState* engine_{nullptr};

    friend EngineState& gubsy_runtime_engine(GubsyRuntime& runtime);
    friend const EngineState& gubsy_runtime_engine(const GubsyRuntime& runtime);
};

bool init_gubsy_runtime(GubsyRuntime& runtime, const GubsyAppConfig& config = {});
void cleanup_gubsy_runtime(GubsyRuntime& runtime);

const GubsyAppConfig& gubsy_runtime_config(const GubsyRuntime& runtime);
bool gubsy_runtime_has_menu_screen(const GubsyRuntime& runtime, MenuScreenId screen_id);

bool gubsy_attach_sdl_renderer(GubsyRuntime& runtime,
                               SDL_Window* window,
                               SDL_Renderer* renderer,
                               int render_width,
                               int render_height);
MenuCommandId gubsy_register_menu_command(GubsyRuntime& runtime,
                                          GubsyHostMenuCommandFn fn,
                                          void* user_data);
void gubsy_set_main_menu_commands(GubsyRuntime& runtime, GubsyMainMenuCommands commands);
bool gubsy_show_main_menu(GubsyRuntime& runtime);
bool gubsy_push_menu_screen(GubsyRuntime& runtime, MenuScreenId screen_id);
void gubsy_pop_menu_screen(GubsyRuntime& runtime);
void gubsy_clear_menu_stack(GubsyRuntime& runtime);
void gubsy_set_menu_input(GubsyRuntime& runtime, const MenuInputState& input);
void gubsy_update_menu(GubsyRuntime& runtime, float dt, int screen_width, int screen_height);
void gubsy_render_menu(GubsyRuntime& runtime, SDL_Renderer* renderer, int screen_width, int screen_height);
