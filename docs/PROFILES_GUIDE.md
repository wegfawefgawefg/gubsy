# Gubsy Profile Systems Guide

This guide explains the complete profile and settings architecture in the Gubsy engine. The system is designed to separate different concerns (user identity, input configuration, game preferences) while allowing flexible combinations.

## Architecture Overview

The Gubsy engine uses a multi-layered profile system:

1. **User Profiles** - Identity and profile references
2. **Players** - Active game participants that reference user profiles
3. **Binds Profiles** - Shareable button/analog mappings
4. **Input Settings Profiles** - Personal sensitivity and preference settings
5. **Game Settings** - Per-user, non-input game preferences
6. **Top-Level Game Settings** - Global, installation-wide settings

## 1. User Profiles

User profiles represent player identities and store references to their preferred settings.

### Structure

```cpp
struct UserProfile {
    int id;                           // Unique 8-digit ID
    std::string name;                 // Display name
    bool guest;                       // True if temporary/non-persistent
    int last_binds_profile;          // ID of last used binds profile (-1 if none)
    int last_input_settings_profile; // ID of last used input settings (-1 if none)
    int last_game_settings;          // ID of last used game settings (-1 if none)
};
```

### Key Functions

```cpp
// Load all user profiles from disk
std::vector<UserProfile> load_all_user_profile_metadatas();

// Load specific profile by ID
UserProfile load_user_profile(int profile_id);

// Save profile to disk (prevents duplicate names/IDs)
bool save_user_profile(const UserProfile& profile);

// Load all profiles into es->user_profiles_pool
bool load_user_profiles_pool();

// Generate random 8-digit ID
int generate_user_profile_id();

// Create and save default profile
UserProfile create_default_user_profile();
```

### Storage Format

User profiles are stored in `data/player_profiles/user_profiles.lisp` as s-expressions:

```lisp
(user_profiles
  (profile
    (id 12345678)
    (name "Player1")
    (last_binds_profile 87654321)
    (last_input_settings_profile 11223344)
    (last_game_settings 55667788)
  )
)
```

### Usage Example

```cpp
// Load all profiles on startup
load_user_profiles_pool();
if (es->user_profiles_pool.empty()) {
    UserProfile default_profile = create_default_user_profile();
    es->user_profiles_pool.push_back(default_profile);
}

// Create a new user profile
UserProfile new_user;
new_user.id = generate_user_profile_id();
new_user.name = "NewPlayer";
new_user.last_binds_profile = -1;
new_user.last_input_settings_profile = -1;
new_user.last_game_settings = -1;
save_user_profile(new_user);

// Update profile's last used settings
UserProfile& profile = es->user_profiles_pool[0];
profile.last_binds_profile = some_binds_profile.id;
save_user_profile(profile);
```

## 2. Players

Players are active entities in the game that reference user profiles. On startup, the engine creates one player and assigns the first user profile to it.

### Structure

```cpp
struct Player {
    bool has_active_profile;
    UserProfile profile;
};
```

### Key Functions

```cpp
// Add a new player (returns player index)
int add_player();

// Remove player by index
void remove_player(int player_index);
```

### Usage Example

```cpp
// On startup, first player gets first profile
add_player();
load_user_profiles_pool();
if (!es->players.empty() && !es->user_profiles_pool.empty()) {
    es->players[0].profile = es->user_profiles_pool[0];
    es->players[0].has_active_profile = true;
}

// Add second player for local multiplayer
int p2_index = add_player();
es->players[p2_index].profile = es->user_profiles_pool[1];
es->players[p2_index].has_active_profile = true;
```

## 3. Binds Profiles

Binds profiles define mappings from physical device inputs to game actions. They are shareable between users.

**Why separate from input settings?** This allows multiple users to share the same button layout while maintaining personal sensitivity/invert preferences.

### Structure

```cpp
struct BindsProfile {
    int id;
    std::string name;
    std::unordered_map<int, int> button_binds;    // device_button → gubsy_action
    std::unordered_map<int, int> analog_1d_binds; // device_axis → gubsy_1d_analog
    std::unordered_map<int, int> analog_2d_binds; // device_stick → gubsy_2d_analog
};
```

### Key Functions

```cpp
// Load all binds profiles
std::vector<BindsProfile> load_all_binds_profiles();

// Load specific profile by ID
BindsProfile load_binds_profile(int profile_id);

// Save profile (prevents duplicate names/IDs)
bool save_binds_profile(const BindsProfile& profile);

// Load all into es->binds_profiles
bool load_binds_profiles_pool();

// Generate random ID
int generate_binds_profile_id();

// Create default (empty) profile
BindsProfile create_default_binds_profile();

// Helper functions
void bind_button(BindsProfile& profile, int device_button, int gubsy_action);
void bind_1d_analog(BindsProfile& profile, int device_axis, int gubsy_1d_analog);
void bind_2d_analog(BindsProfile& profile, int device_stick, int gubsy_2d_analog);
```

### Storage Format

Stored in `data/binds_profiles/*.lisp`:

```lisp
(binds_profiles
  (profile
    (id 87654321)
    (name "DefaultBinds")
    (button_binds
      (bind (device_button 0) (gubsy_action 1))
      (bind (device_button 1) (gubsy_action 2))
    )
    (analog_1d_binds
      (bind (device_axis 0) (gubsy_analog 0))
    )
    (analog_2d_binds
      (bind (device_stick 0) (gubsy_stick 0))
    )
  )
)
```

### Usage Example

```cpp
// Create custom binds profile
BindsProfile custom_binds;
custom_binds.id = generate_binds_profile_id();
custom_binds.name = "CustomLayout";

// Map physical buttons to game actions
// Example: Map SDL button 0 (A on Xbox) to Jump action (enum value 5)
bind_button(custom_binds, 0, 5);

// Map left stick (device_stick 0) to movement (gubsy_stick 0)
bind_2d_analog(custom_binds, 0, 0);

save_binds_profile(custom_binds);

// Assign to user
user_profile.last_binds_profile = custom_binds.id;
save_user_profile(user_profile);
```

## 4. Input Settings Profiles

Input settings profiles store personal sensitivity, deadzone, and invert preferences. These are per-user and allow customization without changing the underlying button mappings.

### Structure

```cpp
struct InputSettingsProfile {
    int id;
    std::string name;

    // Mouse settings
    float mouse_sensitivity;
    bool mouse_invert_x;
    bool mouse_invert_y;

    // Controller settings
    float controller_sensitivity;
    float stick_deadzone;
    float trigger_threshold;
    bool controller_invert_x;
    bool controller_invert_y;
    bool vibration_enabled;
    float vibration_strength;
};
```

### Key Functions

```cpp
// Load all input settings profiles
std::vector<InputSettingsProfile> load_all_input_settings_profiles();

// Load specific profile by ID
InputSettingsProfile load_input_settings_profile(int profile_id);

// Save profile (prevents duplicate names/IDs)
bool save_input_settings_profile(const InputSettingsProfile& profile);

// Load all into es->input_settings_profiles
bool load_input_settings_profiles_pool();

// Generate random ID
int generate_input_settings_profile_id();

// Create default with sensible defaults
InputSettingsProfile create_default_input_settings_profile();
```

### Storage Format

Stored in `data/settings_profiles/input_settings_profiles.lisp`:

```lisp
(input_settings_profiles
  (profile
    (id 11223344)
    (name "DefaultInputSettings")
    (mouse_sensitivity 1.0)
    (mouse_invert_x 0)
    (mouse_invert_y 0)
    (controller_sensitivity 1.0)
    (stick_deadzone 0.15)
    (trigger_threshold 0.1)
    (controller_invert_x 0)
    (controller_invert_y 0)
    (vibration_enabled 1)
    (vibration_strength 1.0)
  )
)
```

### Default Values

- `mouse_sensitivity`: 1.0
- `mouse_invert_x/y`: false
- `controller_sensitivity`: 1.0
- `stick_deadzone`: 0.15
- `trigger_threshold`: 0.1
- `controller_invert_x/y`: false
- `vibration_enabled`: true
- `vibration_strength`: 1.0

### Usage Example

```cpp
// Create high-sensitivity profile
InputSettingsProfile high_sens;
high_sens.id = generate_input_settings_profile_id();
high_sens.name = "HighSensitivity";
high_sens.mouse_sensitivity = 2.5f;
high_sens.controller_sensitivity = 1.8f;
high_sens.stick_deadzone = 0.10f;
high_sens.mouse_invert_y = true; // Inverted Y-axis
high_sens.vibration_strength = 0.7f;

save_input_settings_profile(high_sens);

// Assign to user
user_profile.last_input_settings_profile = high_sens.id;
save_user_profile(user_profile);
```

## 5. Game Settings (Per-User)

Game settings store per-user, non-input-related preferences. Developers define custom key-value pairs using int, float, string, or vec2 types.

### Structure

```cpp
struct SettingsVec2 {
    float x;
    float y;
};

using SettingsValue = std::variant<int, float, std::string, SettingsVec2>;

struct GameSettings {
    int id;
    std::string name;
    std::unordered_map<std::string, SettingsValue> settings;
};
```

### Key Functions

```cpp
// Load all game settings
std::vector<GameSettings> load_all_game_settings();

// Load specific by ID
GameSettings load_game_settings(int settings_id);

// Save (prevents duplicate names/IDs)
bool save_game_settings(const GameSettings& settings);

// Load all into es->game_settings_pool
bool load_game_settings_pool();

// Generate random ID
int generate_game_settings_id();

// Create default (empty)
GameSettings create_default_game_settings();

// Setters
void set_game_setting_int(GameSettings& settings, const std::string& key, int value);
void set_game_setting_float(GameSettings& settings, const std::string& key, float value);
void set_game_setting_string(GameSettings& settings, const std::string& key, const std::string& value);
void set_game_setting_vec2(GameSettings& settings, const std::string& key, float x, float y);

// Getters (with defaults)
int get_game_setting_int(const GameSettings& settings, const std::string& key, int default_value = 0);
float get_game_setting_float(const GameSettings& settings, const std::string& key, float default_value = 0.0f);
std::string get_game_setting_string(const GameSettings& settings, const std::string& key, const std::string& default_value = "");
SettingsVec2 get_game_setting_vec2(const GameSettings& settings, const std::string& key, float default_x = 0.0f, float default_y = 0.0f);
```

### Storage Format

Stored in `data/settings_profiles/game_settings.lisp`:

```lisp
(game_settings_list
  (settings
    (id 55667788)
    (name "DefaultGameSettings")
    (values
      (key "hud_scale" 1.0)
      (key "subtitle_size" 18)
      (key "favorite_color" "blue")
      (key "camera_offset" (vec2 0.0 -2.5))
    )
  )
)
```

### Usage Example

```cpp
// Create custom game settings for a user
GameSettings user_settings;
user_settings.id = generate_game_settings_id();
user_settings.name = "Player1Settings";

// Set various preferences
set_game_setting_float(user_settings, "hud_scale", 1.2f);
set_game_setting_int(user_settings, "subtitle_size", 24);
set_game_setting_string(user_settings, "favorite_character", "Sonic");
set_game_setting_vec2(user_settings, "camera_offset", 0.0f, -3.0f);

save_game_settings(user_settings);

// Assign to user profile
user_profile.last_game_settings = user_settings.id;
save_user_profile(user_profile);

// Read settings in-game
GameSettings active_settings = load_game_settings(user_profile.last_game_settings);
float hud_scale = get_game_setting_float(active_settings, "hud_scale", 1.0f);
int subtitle_size = get_game_setting_int(active_settings, "subtitle_size", 16);
std::string fav_char = get_game_setting_string(active_settings, "favorite_character", "Default");
SettingsVec2 cam_offset = get_game_setting_vec2(active_settings, "camera_offset", 0.0f, 0.0f);
```

### Design Flexibility

Developers define their own settings keys. Common examples:
- HUD preferences (scale, opacity, layout)
- Subtitle settings (size, speed, language)
- Difficulty modifiers
- UI color schemes
- Character preferences
- Camera settings

## 6. Top-Level Game Settings (Global)

Top-level settings are installation-wide and not tied to any user profile. There is only **one** instance for the entire game.

### Structure

```cpp
struct TopLevelGameSettings {
    std::unordered_map<std::string, SettingsValue> settings;
};
```

**Note:** No `id` or `name` field - this is a singleton.

### Key Functions

```cpp
// Load from disk (returns empty if file doesn't exist)
TopLevelGameSettings load_top_level_game_settings();

// Save to disk
bool save_top_level_game_settings(const TopLevelGameSettings& settings);

// Load into es->top_level_game_settings
bool load_top_level_game_settings_into_state();

// Setters
void set_top_level_setting_int(TopLevelGameSettings& settings, const std::string& key, int value);
void set_top_level_setting_float(TopLevelGameSettings& settings, const std::string& key, float value);
void set_top_level_setting_string(TopLevelGameSettings& settings, const std::string& key, const std::string& value);

// Getters (with defaults)
int get_top_level_setting_int(const TopLevelGameSettings& settings, const std::string& key, int default_value = 0);
float get_top_level_setting_float(const TopLevelGameSettings& settings, const std::string& key, float default_value = 0.0f);
std::string get_top_level_setting_string(const TopLevelGameSettings& settings, const std::string& key, const std::string& default_value = "");
```

### Storage Format

Stored in `data/settings_profiles/top_level_game_settings.lisp`:

```lisp
(top_level_game_settings
  (values
    (key "language" "en")
    (key "graphics_quality" 2)
    (key "audio_device" "Default")
    (key "gubsy.audio.master_volume" 0.8)
    (key "telemetry_enabled" 0)
  )
)
```

### Usage Example

```cpp
// Load on startup
load_top_level_game_settings_into_state();

// Access global settings
std::string lang = get_top_level_setting_string(es->top_level_game_settings, "language", "en");
int gfx_quality = get_top_level_setting_int(es->top_level_game_settings, "graphics_quality", 1);
float master_vol = get_top_level_setting_float(es->top_level_game_settings, "gubsy.audio.master_volume", 1.0f);

// Update settings
set_top_level_setting_string(es->top_level_game_settings, "language", "fr");
set_top_level_setting_int(es->top_level_game_settings, "graphics_quality", 3);
save_top_level_game_settings(es->top_level_game_settings);
```

### Common Use Cases

- Language/localization
- Graphics quality presets
- Audio output device selection
- Mod installation paths
- Update/telemetry preferences
- Accessibility features that should persist across all users
- Window mode (fullscreen, windowed, borderless)

## 7. Input Sources

Input sources represent detected hardware devices (keyboard, mouse, gamepads). The engine automatically detects and tracks these.

### Structure

```cpp
enum class InputSourceType {
    Keyboard,
    Mouse,
    Gamepad
};

struct DeviceIdentifier {
    int id;
};

struct InputSource {
    InputSourceType type;
    DeviceIdentifier device;
};
```

### Key Functions

```cpp
// Detect all currently connected devices
bool detect_input_sources();

// Re-scan for changes (called every frame)
void refresh_input_sources();

// Hot-plug handlers (called from SDL events)
void on_device_added(int device_index);
void on_device_removed(int instance_id);
```

### SDL2 Limitations

- **Keyboard and Mouse**: SDL2 aggregates all keyboards and all mice. You cannot differentiate between multiple keyboards or multiple mice. Both use `id = 0`.
- **Gamepads**: SDL2 can enumerate multiple gamepads individually. Each gamepad gets a unique SDL joystick index as its ID.

### Usage

Input sources are automatically populated in `es->input_sources`. The engine handles hot-plug detection via SDL events.

```cpp
// Startup
detect_input_sources();

// Every frame (in step.cpp)
refresh_input_sources();

// Check available devices
for (const auto& source : es->input_sources) {
    if (source.type == InputSourceType::Gamepad) {
        std::printf("Gamepad detected: ID %d\n", source.device.id);
    }
}
```

## Complete Initialization Flow

Here's how the engine initializes all profile systems on startup:

```cpp
// 1. Initialize engine state
init_engine_state();

// 2. Setup players
add_player(); // Create first player

// 3. Load user profiles
load_user_profiles_pool();
if (es->user_profiles_pool.empty()) {
    UserProfile default_profile = create_default_user_profile();
    es->user_profiles_pool.push_back(default_profile);
}

// 4. Assign first profile to first player
if (!es->players.empty() && !es->user_profiles_pool.empty()) {
    es->players[0].profile = es->user_profiles_pool[0];
    es->players[0].has_active_profile = true;
}

// 5. Detect input sources
detect_input_sources();
if (es->input_sources.empty()) {
    std::fprintf(stderr, "[input] Warning: No input sources detected\n");
}

// 6. Load all profile pools
load_binds_profiles_pool();
load_input_settings_profiles_pool();
load_game_settings_pool();
load_top_level_game_settings_into_state();
```

## Best Practices

### 1. Separation of Concerns

- Use **Binds Profiles** for button layouts (shareable between users)
- Use **Input Settings Profiles** for personal sensitivity/invert preferences
- Use **Game Settings** for non-input user preferences
- Use **Top-Level Settings** for installation-wide configuration

### 2. Profile Naming

Use descriptive names that help users identify profiles:
- Binds: "DefaultBinds", "FPSLayout", "Fighting"
- Input Settings: "HighSensitivity", "InvertedY", "LowDeadzone"
- Game Settings: "CasualMode", "HardcorePreferences"

### 3. Default Values

Always provide sensible defaults in getter functions:

```cpp
float sens = get_game_setting_float(settings, "mouse_sensitivity", 1.0f);
```

This ensures the game works even if a setting is missing.

### 4. Validation

The save functions prevent duplicate names and IDs:

```cpp
if (!save_user_profile(profile)) {
    // Handle error: duplicate name or ID
}
```

### 5. Guest Profiles

For temporary players (local multiplayer guests), set `guest = true` on the user profile. These can be excluded from profile selection lists or auto-deleted after the session.

## Example: Complete User Setup

```cpp
// Create new user
UserProfile new_user;
new_user.id = generate_user_profile_id();
new_user.name = "Alice";
new_user.guest = false;

// Create custom binds
BindsProfile alice_binds;
alice_binds.id = generate_binds_profile_id();
alice_binds.name = "AliceLayout";
bind_button(alice_binds, 0, 5); // A button -> Jump
bind_button(alice_binds, 1, 6); // B button -> Attack
save_binds_profile(alice_binds);

// Create input settings
InputSettingsProfile alice_input;
alice_input.id = generate_input_settings_profile_id();
alice_input.name = "AliceSettings";
alice_input.mouse_sensitivity = 1.5f;
alice_input.mouse_invert_y = true;
save_input_settings_profile(alice_input);

// Create game settings
GameSettings alice_game;
alice_game.id = generate_game_settings_id();
alice_game.name = "AlicePreferences";
set_game_setting_float(alice_game, "hud_scale", 1.1f);
set_game_setting_string(alice_game, "favorite_character", "Tails");
save_game_settings(alice_game);

// Link everything to user profile
new_user.last_binds_profile = alice_binds.id;
new_user.last_input_settings_profile = alice_input.id;
new_user.last_game_settings = alice_game.id;
save_user_profile(new_user);

// Add to pool and assign to player
es->user_profiles_pool.push_back(new_user);
es->players[0].profile = new_user;
es->players[0].has_active_profile = true;
```

## File Locations

All configuration files are stored in the `data/` directory:

- `data/player_profiles/user_profiles.lisp` - User profiles
- `data/binds_profiles/*.lisp` - Binds profiles
- `data/settings_profiles/input_settings_profiles.lisp` - Input settings profiles
- `data/settings_profiles/game_settings.lisp` - Game settings
- `data/settings_profiles/top_level_game_settings.lisp` - Top-level settings

The engine automatically creates these directories if they don't exist.

## Troubleshooting

### Profile not saving?

Check return values:
```cpp
if (!save_user_profile(profile)) {
    // Possible causes:
    // - Invalid ID (id <= 0)
    // - Empty name
    // - Duplicate name (different ID with same name)
    // - Duplicate ID
}
```

### Settings not persisting?

Ensure you're saving after modification:
```cpp
set_game_setting_int(settings, "difficulty", 3);
save_game_settings(settings); // Don't forget this!
```

### Input device not detected?

Check `es->input_sources` after calling `detect_input_sources()`. Remember that keyboard and mouse are always present (even if SDL reports no physical devices).

---

For more information on s-expression parsing, see [src/engine/parser.hpp](../src/engine/parser.hpp).
