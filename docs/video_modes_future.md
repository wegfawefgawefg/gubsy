# Video Modes and Fullscreen - Future Implementation Notes

## Current Implementation

The engine currently uses SDL2's **borderless fullscreen** (`SDL_WINDOW_FULLSCREEN_DESKTOP`) which:
- Takes over the desktop at its native resolution/refresh rate
- Doesn't change display modes
- Provides seamless Alt-Tab
- Works with any render resolution (scales internally)

Settings available:
- **Display Mode**: Windowed, Borderless, Fullscreen (all use borderless currently)
- **Render Resolution**: 35+ presets from 160x144 to 7680x4320
- **Render Scale**: Fit (letterbox) or Stretch
- **V-Sync**: On/Off (syncs to monitor refresh rate)
- **Frame Rate Cap**: Slider from unlimited to capped

## Future Enhancement: Exclusive Fullscreen

### Why Add It?
- **Lower input latency** (direct display access)
- **Better performance** on some systems (no desktop compositor)
- **Preferred by competitive gamers**

### Implementation Plan

#### 1. Add New Setting: "Fullscreen Mode"
Location: Video settings, shown only when Display Mode = "Fullscreen"

Options:
- **Borderless (recommended)** - Current behavior, seamless
- **Exclusive** - True fullscreen with video mode selection

#### 2. When Exclusive Mode is Selected

Add new setting: **Video Mode** (dropdown)
- Only shown when Fullscreen Mode = Exclusive
- Enumerate available modes using `SDL_GetNumDisplayModes()` and `SDL_GetDisplayMode()`
- Display as: "1920x1080 @ 144Hz", "2560x1440 @ 60Hz", etc.
- Filter to show only modes >= 720p (hide ancient modes)

#### 3. Synchronization Considerations

When in Exclusive Fullscreen:
- **Frame Cap** should be automatically set to match refresh rate (or lower)
  - Example: If 144Hz mode selected, suggest capping at 144fps
  - Show warning if cap > refresh rate ("Exceeds display refresh rate")
- **V-Sync** becomes more important (prevents tearing in exclusive mode)
- **Render Resolution** stays independent (internal rendering resolution)

#### 4. Code Changes Required

```cpp
// In graphics.cpp or similar
struct VideoModeInfo {
    int width;
    int height;
    int refresh_rate;
    std::string label;  // "1920x1080 @ 144Hz"
};

std::vector<VideoModeInfo> enumerate_video_modes(int display_index) {
    std::vector<VideoModeInfo> modes;
    int num_modes = SDL_GetNumDisplayModes(display_index);

    for (int i = 0; i < num_modes; ++i) {
        SDL_DisplayMode mode;
        if (SDL_GetDisplayMode(display_index, i, &mode) == 0) {
            // Filter: only >= 720p
            if (mode.w >= 1280 && mode.h >= 720) {
                VideoModeInfo info;
                info.width = mode.w;
                info.height = mode.h;
                info.refresh_rate = mode.refresh_rate;
                info.label = std::to_string(mode.w) + "x" + std::to_string(mode.h)
                           + " @ " + std::to_string(mode.refresh_rate) + "Hz";
                modes.push_back(info);
            }
        }
    }

    // Sort by resolution, then refresh rate
    std::sort(modes.begin(), modes.end(), /*...*/);

    return modes;
}

void set_exclusive_fullscreen(int width, int height, int refresh_rate) {
    SDL_DisplayMode target_mode;
    target_mode.w = width;
    target_mode.h = height;
    target_mode.refresh_rate = refresh_rate;
    target_mode.format = 0; // Let SDL choose
    target_mode.driverdata = nullptr;

    SDL_SetWindowDisplayMode(gg->window, &target_mode);
    SDL_SetWindowFullscreen(gg->window, SDL_WINDOW_FULLSCREEN);
}
```

#### 5. Settings Schema

```cpp
// New setting (conditional on Display Mode = Fullscreen)
schema.add_setting(make_option_setting(
    SettingScope::Install,
    "gubsy.video.fullscreen_mode",
    "Fullscreen Mode",
    "How fullscreen is implemented.",
    {"Video"},
    {SettingOption{"borderless", "Borderless (recommended)"},
     SettingOption{"exclusive", "Exclusive"}},
    "borderless"));

// New setting (conditional on Fullscreen Mode = Exclusive)
// This would need to be dynamically populated from enumerate_video_modes()
schema.add_setting(make_option_setting(
    SettingScope::Install,
    "gubsy.video.exclusive_mode",
    "Video Mode",
    "Display resolution and refresh rate.",
    {"Video"},
    /* dynamically generated options */,
    "1920x1080@60"));
```

#### 6. UI Considerations

- Show "Fullscreen Mode" setting only when Display Mode = "Fullscreen"
- Show "Video Mode" dropdown only when Fullscreen Mode = "Exclusive"
- Add help text: "Exclusive mode may provide lower latency but can cause Alt-Tab issues"
- Show warning if user selects unsupported combinations

## Testing Strategy

1. **Cross-platform testing** - Windows, Linux, macOS behave differently
2. **Multi-monitor testing** - Which display? Primary vs secondary?
3. **Fallback handling** - What if requested mode isn't supported?
4. **Alt-Tab behavior** - Does window restore correctly?
5. **Mode switching performance** - Is transition smooth or jarring?

## Priority

**Low** - Current borderless implementation works well for 95% of users. Only implement this if:
- Users specifically request it
- Building a competitive/esports game
- Need to squeeze out every bit of performance

## Notes

- Modern games increasingly default to borderless windowed
- Many users don't even notice the difference anymore
- Exclusive fullscreen can cause issues with multi-monitor setups
- Windows 10+ with Game Mode reduces borderless latency significantly
