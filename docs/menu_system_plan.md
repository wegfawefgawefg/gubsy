THIS MENU SYSTEM PLAN WAS DESIGNED BEFORE menu_system_plan2.md which sort of replaces this!!!
please go read that file. this one may still be nice for context

# Menu System Plan

## Goals
- Engine provides reusable menu primitives (navigation, focus, widgets, paging) while keeping files small (≤500 LOC).
- Layout is data-driven through the existing `UILayout` system; rendering code queries normalized rects.
- Game code can register new screens or replace engine defaults without duplicating plumbing.
- Dear ImGui remains for debug tooling; production menus use SDL + our renderer.

## Top-Level Architecture
1. **MenuManager (engine/menu/manager.[hpp|cpp])**
   - Owned by `EngineState` (`es->menus`).
   - Tracks active `MenuScreenId`, focus widget id, stacked screens, repeat timers, and text-input mode.
   - Consumes menu actions (`MENU_UP/DOWN/LEFT/RIGHT/SELECT/BACK`, `MENU_SCALE_*`, `MENU_PAGE_*`); distributes to active screen.
   - Provides helper APIs:
     - `push_screen(MenuScreenId id)`
     - `pop_screen()`
     - `set_focus(WidgetId id)`
     - `capture_text(MenuTextCapture& capture)` (struct with buffer, limit, callback)
2. **MenuScreen interface (engine/menu/screen.hpp)**
   ```cpp
   struct MenuScreen {
       MenuScreenId id;
       UILayoutId layout;
       std::vector<MenuWidget> (*build_widgets)(const MenuContext&);
       void (*on_enter)(MenuContext&);
       void (*on_exit)(MenuContext&);
       void (*update)(MenuContext&, float dt);
       void (*render_overlay)(const MenuContext&, SDL_Renderer*);
   };
   ```
   - `MenuContext` exposes references to `EngineState`, `State`, `MenuManager`, and the screen’s local state struct.
3. **Screens registry (engine/menu/screens.cpp)**
   - Holds `constexpr MenuScreen` definitions for built-in pages: `Main`, `SettingsHub`, `VideoSettings`, `AudioSettings`, `ControlsSettings`, `Binds`, `ModsBrowser`, `Lobby`, etc.
   - Provides `register_screen(const MenuScreen&)` so games can add custom ones at startup.
4. **Widget definitions (engine/menu/widgets.[hpp|cpp])**
   - `struct MenuWidget { WidgetId id; WidgetType type; UILayoutObjectId slot; MenuStyle style; WidgetState state; }`
   - Types: `Label`, `Button`, `Toggle`, `Slider1D`, `OptionCycle`, `List`, `TextInput`, `Card`.
   - Each widget carries callbacks:
     - `on_activate(MenuContext&)`
     - `on_adjust(MenuContext&, WidgetAdjust dir)` for sliders/cycles.
   - `WidgetState` stores per-widget runtime info (e.g., slider value pointer, text buffer pointer).
5. **Renderer (engine/menu/render.cpp)**
   - Loops widgets, samples layout rects via `UILayout::get_object_rect(screen.layout, widget.slot, width, height)`.
   - Draws each widget using SDL (simple shapes + fonts); styles pulled from `MenuStyle`.
   - Handles focus/hover visuals via `MenuManager` focus id.

## Files & Responsibilities
| File | Responsibility |
| --- | --- |
| `engine/menu/manager.hpp/cpp` | MenuManager core loop, input dispatch, screen stack, text capture. |
| `engine/menu/screen.hpp/cpp` | Screen definitions, registry, helpers for entering/exiting. |
| `engine/menu/widgets.hpp/cpp` | Widget structs, factory helpers, event dispatch (activate/adjust). |
| `engine/menu/layout_helpers.hpp` | Functions to query `UILayout` objects and fallback defaults. |
| `engine/menu/render.hpp/cpp` | Rendering of widgets, focus indicators, page headers, hints. |
| `engine/menu/screens/*.cpp` | Built-in screens (e.g., `main_screen.cpp`, `mods_screen.cpp`). |
| `game/menu/screens/*.cpp` | Optional game-specific screens registered from game code. |

## Data Flow
1. Game (or engine startup) registers screens and pushes `MenuScreenId::Main`.
2. Each frame:
   - MenuManager collects menu actions (already mapped from device inputs).
   - `MenuManager::update(dt)`:
     - If capturing text, send characters to active text buffer.
     - Otherwise, `dispatch_nav()` to active screen’s widgets (focus changes, slider adjustments).
     - On `SELECT/BACK`, call widget callbacks; screens can push/pop other screens or run game logic.
   - `MenuManager::build_widget_list()` asks the active screen to construct widgets for the current frame (reading current state values).
   - `menu_render.cpp` renders widgets based on layout.

## Layout Integration
- Each screen declares a `UILayoutId`. Designers edit `data/ui_layouts/*.lisp` to position UI objects (buttons, text blocks, cards).
- Widgets reference layout objects by name/ID. Helper:
  ```cpp
  RectPixels menu_widget_rect(const MenuContext&, UILayoutObjectId slot, int width, int height);
  ```
- If a layout object is missing, fallback to predefined normalized rects (maintain robustness).

## Menu State & Widgets
- `MenuManager` stores per-screen state in a `std::unordered_map<MenuScreenId, std::unique_ptr<MenuScreenStateBase>>`.
- Each screen defines its own state struct inheriting from `MenuScreenStateBase`.
  ```cpp
  struct ModsScreenState : MenuScreenStateBase {
      bool catalog_loaded = false;
      int current_page = 0;
      std::string search_query;
      std::vector<int> visible_indices;
      std::string status;
      bool busy = false;
  };
  ```
- `MenuContext` provides `template<typename T> T& state();` to access typed state safely.

## Built-In Screens Outline
1. **Main Screen**
   - Widgets: Buttons for Play, Mods, Settings, Quit.
   - Actions: Play pushes `MenuScreenId::Loading` or sets `es->mode = modes::SETUP`.
2. **Settings Hub**
   - Buttons linking to `Video`, `Audio`, `Controls`, `Binds`, `Players`.
3. **Video/Audio/Controls**
   - Widgets auto-generated from schemas.
   - Use slider/toggle widgets bound to config values (with apply/cancel buttons).
4. **Binds**
   - List widget showing actions, capture dialog on select.
   - Text input to rename/save presets.
5. **Mods Browser**
   - Search text input (uses text capture mode).
   - Prev/Next buttons (or left/right actions) to change pages.
   - For each card:
     - Title, author, version, summary, dependencies text.
     - Status label (Core/Installed/Not installed/Installing/Removing).
     - Action button: Install / Remove / Disabled if required.
     - Action triggers `install_mod_by_index`/`uninstall_mod_by_index`, updates state.
   - Back button pops to previous screen.
6. **Lobby (future)**
   - Similar pattern with custom widgets (player slots, mod toggles, etc.).

## Developer API
From game code (e.g., `game/main.cpp`):
```cpp
#include "engine/menu/manager.hpp"
#include "engine/menu/screens.hpp"

void register_custom_menus() {
    MenuScreen custom{
        MenuScreenId::CustomGameSetup,
        UILayoutID::CUSTOM_SETUP,
        build_custom_widgets,
        enter_custom,
        exit_custom,
        update_custom,
        render_custom_overlay
    };
    register_menu_screen(custom);
}

void enter_title_flow() {
    MenuManager& menus = es->menus;
    menus.push_screen(MenuScreenId::Main);
}
```

**Widget creation helpers** (engine/menu/widgets.hpp):
```cpp
MenuWidget make_button(WidgetId id, UILayoutObjectId slot,
                       const char* label,
                       MenuWidget::OnActivate cb);
MenuWidget make_toggle(WidgetId id, UILayoutObjectId slot,
                       bool* value_ptr, const char* label);
MenuWidget make_slider(WidgetId id, UILayoutObjectId slot,
                       float* value_ptr, float min, float max, const char* label);
MenuWidget make_text_input(WidgetId id, UILayoutObjectId slot,
                           std::string* buffer, int max_len,
                           MenuWidget::OnSubmit submit_cb);
```

**Text Capture Flow**
1. Widget requests capture via `MenuManager::begin_text_capture(TextCaptureDesc desc)`.
2. Desc includes buffer pointer, max length, filter flags, completion callback.
3. During capture, nav actions are suppressed except `MENU_BACK` cancels.
4. On submit/cancel, `end_text_capture()` resumes normal nav.

**Focus & Navigation**
- Widgets may specify neighbor overrides (explicit graph) or rely on auto-layout order.
- `MenuManager` maintains `focus_id`. When `MENU_UP/DOWN/LEFT/RIGHT` fire:
  - If widget has explicit neighbor, move there.
  - Else, auto-choose based on layout positions (e.g., nearest widget in that direction).
- `MENU_SELECT` triggers widget `on_activate`. `MENU_BACK` either runs widget-specific `on_cancel` or pops screen if allowed.

## Migration Strategy
1. Implement MenuManager + widgets + renderer skeleton.
2. Port Main menu to new system.
3. Port Mods screen (largest UI) using existing backend logic.
4. Replace current ImGui-driven title menu by invoking MenuManager screens.
5. Incrementally add other screens (Settings, Binds, etc.) reusing existing code.
6. Keep old `ui/menu/` directory around temporarily for reference; delete once new system covers feature parity.

## Notes
- Screens and widgets stay <500 LOC by splitting each screen into its own file under `engine/menu/screens/`.
- `ss->menu` legacy struct will be replaced by per-screen states + small shared structs (e.g., audio slider preview). During migration, we can wrap legacy state inside new state structs.
- Mods browser continues to rely on existing mod install helpers; we only swap out the UI layer.
