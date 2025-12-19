# Menu System Plan (Variant 2)

Design goals stay the same: data-driven menus, no Dear ImGui in ship builds, 300–500 LOC per file, engine/game boundaries respected even though everything is compiled together. This revision focuses on a fully data-oriented design that avoids per-widget callbacks and lets the engine stay ignorant of game-specific state types.

## 1. Architecture Overview

### 1.1 MenuManager (engine/menu/manager.[hpp|cpp])
- Owned by `EngineState` (`es->menu_manager`).
- Responsibilities:
  - Maintain screen stack (`std::vector<ScreenInstance>`).
  - Track focus widget id, last input source, repeat timers.
  - Handle text capture mode (buffer pointer, limits, callback).
  - Dispatch nav/input events to the active screen’s widget graph.
  - Execute `MenuAction`s emitted by widgets.
- Public API:
  - `void push_screen(MenuScreenId id);`
  - `void pop_screen();`
  - `void handle_actions(const MenuInputState& in);`
  - `void update(float dt);`
  - `void render(SDL_Renderer* renderer);`

### 1.2 MenuScreenDef (engine/menu/screen.hpp)
```cpp
struct ScreenStateOps {
    uint32_t size;
    uint32_t align;
    void (*init)(void*);
    void (*destroy)(void*);
};

struct MenuScreenDef {
    MenuScreenId id;
    UILayoutId layout;
    ScreenStateOps state_ops;
    BuiltScreen (*build)(MenuContext&);
    void (*tick)(MenuContext&, float dt);     // optional
};
```

* `MenuContext` gives screens access to: `EngineState`, `State`, `MenuManager`, screen-local state pointer, and read-only info (time, renderer size).
* `BuiltScreen` is a lightweight struct describing the layout + widget list emitted by `build()`:
```cpp
struct BuiltScreen {
    UILayoutId layout;             // usually same as def.layout but screens can swap
    MenuWidgetSpan widgets;        // Span/view into a temporary vector
    MenuActionSpan frame_actions;  // optional one-off commands (e.g., auto-open dialogs)
};
```

### 1.3 Screen state storage
- Engine stores one state blob per `MenuScreenId`.
- Each blob is just `std::vector<uint8_t> storage`, aligned; `state_ops.init/destroy` manage lifetime.
- Screen code casts `context.state<T>()` to its own struct (engine never sees the type).
- Example:
```cpp
struct ModsScreenState {
    bool catalog_loaded = false;
    int current_page = 0;
    std::string search_query;
    std::vector<int> visible_indices;
    bool in_progress = false;
    std::string status;
};
```

## 2. Widgets & Actions

### 2.1 Widget definition (engine/menu/widgets.hpp)
```cpp
enum class WidgetType : uint8_t { Label, Button, Toggle, Slider1D, OptionCycle, TextInput, Card };

struct MenuWidget {
    WidgetId id;
    WidgetType type;
    UILayoutObjectId slot;   // layout object to query for rect
    MenuStyle style;

    // Bindings / data
    void* bind_ptr = nullptr;        // e.g., bool*, float*, string*
    float min = 0.0f, max = 1.0f;
    int option_count = 0;
    const char* label = nullptr;
    const char* secondary = nullptr; // e.g., value text

    // Actions on interactions
    MenuAction on_select;
    MenuAction on_left;
    MenuAction on_right;
    MenuAction on_back;
};
```

### 2.2 MenuAction (engine/menu/actions.hpp)
```cpp
enum class MenuActionType : uint8_t {
    None,
    PushScreen,
    PopScreen,
    RequestFocus,
    BeginTextCapture,
    EndTextCapture,
    ToggleBool,
    SetFloat,
    DeltaFloat,
    CycleOption,
    RunCommand,        // generic command id + payload
};

struct MenuAction {
    MenuActionType type = MenuActionType::None;
    int32_t a = 0;        // generic payload (ids, indices)
    int32_t b = 0;        // secondary payload
    float f = 0.0f;       // value/delta
    void* ptr = nullptr;  // binding pointer
};
```

* `RunCommand` uses integer command IDs. The engine holds a registry (vector of function pointers) bridging IDs to actual functions (game or engine-level). Example: `CMD_INSTALL_MOD`, `CMD_UNINSTALL_MOD`.
* Widgets can emit multiple actions (e.g., slider left = `DeltaFloat{-0.1f}`; slider select = `RunCommand{CMD_APPLY_SETTING}`).

### 2.3 Input consumption rule
1. Manager receives `MENU_LEFT`:
   - If focused widget has `on_left.type != None`, execute it and **consume**.
   - Else, move focus to neighbor in that direction.
2. Same for `MENU_RIGHT`.
3. `MENU_UP/DOWN` default to navigation unless widget has explicit vertical adjustments.

## 3. Layout Integration
- Screen definitions specify a `UILayoutId`.
- Rendering step resolves each widget’s rect via `UILayout` pool:
```cpp
RectPixels rect = get_layout_rect(screen.layout, widget.slot, width, height);
```
- If the layout lacks the object, fallback to a default normalized rect (with a logged warning).
- Layouts live in `data/ui_layouts/*.sxp` and are created with the same tool as other UI surfaces.

## 4. Rendering
- `engine/menu/render.cpp`:
  - For each widget: fetch rect, draw background, label, focus indicator.
  - Styles are simple: colors, border thickness, fonts (maybe we reuse `gg->ui_font`).
  - Optional overlay hooks: built-in screens can push extra draw commands (e.g., mod card details) after main widget pass.

## 5. Built-in screens & files
All under `engine/menu/screens/`, each ≤300–500 LOC:
1. `main_screen.cpp`: Play / Mods / Settings / Quit.
2. `settings_hub.cpp`: Buttons to Video, Audio, Controls, Binds, Players.
3. `video_screen.cpp`, `audio_screen.cpp`, `controls_screen.cpp`: schema-driven widgets.
4. `binds_screen.cpp`: list of actions, preset management, capture prompts.
5. `mods_screen.cpp`: uses `ModsScreenState`, `install_mod_by_index`, etc.
6. `lobby_screen.cpp`, `lobby_mods_screen.cpp`: future work, follow same pattern.

Game-specific menu screens live in `src/game/menu/screens/...` and call `register_menu_screen()` at startup.

## 6. Developer API (game side)
```cpp
#include "engine/menu/manager.hpp"

struct MyScreenState {
    int selection = 0;
    std::array<float, 4> values{};
};

BuiltScreen build_my_screen(MenuContext& ctx) {
    auto& st = ctx.state<MyScreenState>();
    static std::vector<MenuWidget> widgets;
    widgets.clear();
    widgets.push_back(make_button(100, UILayoutObjectID::MY_PLAY_BTN, "Start",
                                  MenuAction::push(MenuScreenId::SETUP)));
    widgets.push_back(make_slider(110, UILayoutObjectID::MY_SLIDER,
                                  &st.values[0], 0.0f, 1.0f));
    return {UILayoutID::MY_SCREEN, widgets, {}};
}

MenuScreenDef my_screen_def{
    MenuScreenId::MY_SCREEN,
    UILayoutID::MY_SCREEN,
    screen_state_ops<MyScreenState>(),
    build_my_screen,
    nullptr
};

void register_game_screens() {
    register_menu_screen(my_screen_def);
}
```

Helper `screen_state_ops<T>()` generates `ScreenStateOps` for type `T` using placement-new and explicit destructors.

## 7. Command Dispatch
- Engine maintains a simple command table:
```cpp
using MenuCommandFn = void(*)(MenuContext&, int32_t payload);
uint32_t register_menu_command(MenuCommandFn fn);
```
- Game registers commands (e.g., install/uninstall mod) and gets back IDs.
- Widgets reference those IDs in their `MenuAction`.
- Manager executes via `commands[id](context, payload);`.
- This avoids function pointers embedded per widget and keeps actions uniform.

## 8. Input Flow Summary
1. Main loop feeds menu manager a snapshot of menu actions (from `GameAction::MENU_*`).
2. `MenuManager::handle_actions` builds a `MenuInputState`.
3. `MenuManager::update(dt)`:
   - If text capture active → send characters, handle submit/back.
   - Else:
     - Process discrete actions (select/back).
     - Process directional nav with consumption rule.
     - Execute resulting `MenuAction`s (push/pop, command dispatch).
   - Call screen `tick()` if provided (for e.g., mod install progress).
4. `MenuManager::render(renderer)`:
   - Ask active screen for `BuiltScreen`.
   - Generate nav graph from widget rects (cached per frame).
   - Draw widgets.

## 9. Interaction with existing systems
- **Mods screen**:
  - Uses existing helpers (`ensure_mod_catalog_loaded`, `rebuild_mods_filter`, `install_mod_by_index`).
  - Screen state stores catalog, filtered indices, page, search text, busy flag.
  - Search text input uses `BeginTextCapture` action, writing into `std::string` inside state.
  - Install button widget emits `MenuAction::run_command(CMD_INSTALL_MOD, catalog_idx)`; command handler calls backend and updates state.
- **Settings pages**:
  - Widgets bind directly to config fields (bool*, float*). `ToggleBool`/`DeltaFloat` actions mutate the bound pointers; apply buttons emit `RunCommand` for `CMD_APPLY_VIDEO_SETTINGS`.
- **Binds page**:
  - List widget built from binds schema.
  - Selecting a slot uses `BeginCaptureCommand` (custom command) to enter input-capture mode.

## 10. Migration Path
1. Implement MenuManager infra + renderer + action execution.
2. Port the existing ImGui title menu to the new system (just Main screen).
3. Port Mods screen using legacy logic to ensure parity.
4. Port Settings hub + at least one subpage.
5. Drop old `ui/menu/` code once new screens cover required flows.
6. Keep Dear ImGui enabled only for debug overlays (mod viewer, layout tools, etc.).

## 11. Notes / Constraints
- Keep files short: each screen file limited to a few hundred lines; shared helpers in common modules.
- Avoid STL containers in public ABI between engine/game where possible (use spans, references).
- Text capture must respect existing input system (menu actions + normalized keyboard text events).
- Engine code never includes game headers; game registers screens + commands via engine-facing headers.
- All IDs (screen, widget, layout, command) are `uint32_t` typedefs to avoid accidental mixing.

