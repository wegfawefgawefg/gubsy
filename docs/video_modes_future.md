# Video Modes and Fullscreen

## Current Model

Gubsy keeps display/output mode separate from internal rendering:

1. `Display Mode` controls how SDL presents the window: windowed, borderless, or fullscreen.
2. `Window Size` controls the normal window size when `Display Mode` is windowed.
3. `Fullscreen Display Mode` controls the SDL fullscreen mode when `Display Mode` is fullscreen.
4. `Render Resolution` controls the internal render target, unless `Match Render To Window` is on.
5. `Render Scale` controls how the internal render target maps into the window/output.
6. `V-Sync` controls presentation synchronization where the backend supports it.
7. `Render Rate Limit` is a software cap on render/present frequency. It does not change fixed-step simulation.

## Fullscreen Modes

`Borderless` uses SDL desktop fullscreen behavior. It keeps the desktop display mode and scales the internal render target into that output.

`Fullscreen` uses SDL fullscreen mode selection:

1. `Desktop Default` passes a null fullscreen mode to SDL, preserving desktop fullscreen behavior.
2. Concrete options such as `1920x1080 @ 60 Hz` are enumerated from `SDL_GetFullscreenDisplayModes`.
3. Applying fullscreen calls `SDL_SetWindowFullscreenMode` and then enters fullscreen.
4. If a saved concrete mode is unavailable, Gubsy prints a warning and falls back to `Desktop Default`.

The fullscreen mode setting is visible in Video settings but disabled unless `Display Mode` is fullscreen. This avoids menu reshuffling while still making the relationship explicit.

## Render Rate Limit

`Render Rate Limit` is off by default. Normal users should be able to rely on V-Sync and display mode selection without touching it.

The cap is useful for laptops, debugging, broken V-Sync setups, thermal limits, or intentionally low present rates. It only throttles rendering/presenting. Games using fixed simulation steps continue to simulate at their fixed timestep.

## Notes

Avoid automatic mutation between V-Sync, fullscreen mode, and render rate limit. They control different layers. Prefer hints and warnings over hidden setting changes.
