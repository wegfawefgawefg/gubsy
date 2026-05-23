# Ripout Tasks

This is a parking lot for small reusable `g-*` libraries that can be pulled
out of game repos. The goal is not to make framework code. The goal is to
extract boring, usable libraries with clear ownership and low abstraction.

## General Rules

- Keep libraries small enough to vendor directly with CMake `FetchContent` or a
  git submodule.
- Prefer C++20, plain data types, and explicit control flow.
- Avoid requiring SDL, ImGui, Lua, OpenGL, or a game `EngineState` in core
  libraries.
- Optional UI/debug adapters are fine, but they should sit at the edge.
- Use `gsexp` for sexpr parsing instead of copying parser code into every
  library.
- Do not rip out game rules just because code is reusable-looking.
- Prefer names that describe the job plainly.

## Asset And Content Stack

The content/asset/animation/particle libraries should compose without turning
`gubsy` into a hard dependency for games like `splonks-cpp`.

Preferred dependency shape:

```text
gmods
  package/mod manifests, enabled order, roots

gassets
  asset catalog, ids, types, override resolution, dependency graph

ganim
  clips, frames, timing, source rects, animator state

gparticles
  particle specs, emitters, simulation, draw commands

game or gubsy
  renderer, gameplay metadata, validation, concrete asset application
```

`gmods` and `gassets` are different jobs. `gmods` decides what packages or
mods exist, whether they are compatible, and what ordered content roots should
be active. `gassets` consumes active roots and resolves concrete asset records:
ids, names, types, source paths, dependencies, and overrides. A non-modded game
can still use `gassets` by passing a single base content root.

`gassets` should be asset-agnostic. It should know that an asset exists, what
type it claims to be, where it came from, which package owns it, and which
record wins after overrides. It should not parse every animation, particle,
audio, or gameplay format. Typed libraries such as `ganim`, `gparticles`, and
`gaudio` should either load directly from files or use small optional
adapters that read matching records from `gassets`.

`ganim` should not own game-specific frame semantics. It can own clips, frames,
timing, image/source-rect references, draw offsets, ids, and animator state.
Splonks-specific fields such as `pbox`, `cbox`, tile flags, emit points when
they drive gameplay, and entity/tile content-name mappings should be parsed
once into game-owned typed sidecar data.

For hot runtime reads, prefer parallel typed arrays over opaque metadata lookups:

```cpp
struct SplonksClipMeta {
    ganim::AnimId anim_id;
    std::vector<SplonksFrameMeta> frames;
};

struct SplonksFrameMeta {
    Rect pbox;
    Rect cbox;
    Vec2 emit_point;
    bool tile = false;
};
```

The load step validates that `SplonksClipMeta::frames` matches the `ganim` clip
frame count and order. Runtime code should cache direct clip/meta pointers or
indices and read:

```cpp
const std::uint32_t frame_index = animator.frame_index();
const ganim::Frame& frame = clip.frames[frame_index];
const SplonksFrameMeta& meta = clip_meta.frames[frame_index];
```

Span/editor interchange can still author arbitrary metadata, but runtime should
compile that metadata into typed game-owned structs. Do not parse strings or
walk generic metadata maps per frame.

`gubsy` may depend on `ganim` or `gparticles` for its own menus and effects.
That does not mean those systems should live inside `gubsy`. Keep dependency
direction one-way: `gubsy` composes the libraries; the libraries do not depend
on `gubsy`.

## Already Extracted

### `gsexp`

Small sexpr parser and query layer.

Current role:

- Shared parser for config, layouts, manifests, and simple data files.
- Intended replacement for ad hoc sexpr parser copies.

Follow-up:

- Replace `gubsy/engine/parser.*` users with vendored `gsexp`.
- Avoid creating new per-library parsers.

### `glayout`

Resolution-matched rectangle layout tool with optional ImGui editor support.

Current role:

- Layout selection by target resolution.
- Layout object rectangles.
- Optional editor/debug UI.
- Demo app shows SDL3 and ImGui integration.

Follow-up:

- Migrate `gubsy/engine/ui_layouts.*` and `gubsy/engine/layout_editor/*` to
  vendored `glayout`.

## Gubsy Candidates

### 1. `gsettings` or `gconfig`

Priority: high.

Source areas:

- `engine/settings_types.hpp`
- `engine/settings_schema.*`
- `engine/settings_catalog.*`
- `engine/settings_defaults.*`
- `engine/game_settings.*`
- `engine/top_level_game_settings.*`
- `engine/user_profiles.*`
- `engine/input_settings_profiles.*`
- `engine/audio_settings.*`
- `engine/device_settings.hpp`
- `engine/runtime_settings.hpp`

Why it is a candidate:

- Most games need typed settings, defaults, validation, profiles, and file IO.
- This is likely useful across `gubsy`, `splonks-cpp`, and future projects.
- It should naturally use `gsexp`.

Extraction boundary:

- Core library owns setting definitions, values, schema validation, profile
  records, and serialization.
- Caller owns file paths, save timing, UI screens, and actual application of
  settings to SDL/audio/video/input systems.

Before ripout:

- Remove direct `EngineState` dependence.
- Replace `engine/parser.*` use with `gsexp`.
- Separate schema/value logic from built-in Gubsy setting lists.
- Decide whether profiles are part of the core library or a small companion
  layer.

### 2. `gmenu`

Priority: high, but after `glayout` integration is cleaner.

Source areas:

- `engine/menu/menu_manager.*`
- `engine/menu/menu_screen.hpp`
- `engine/menu/menu_types.hpp`
- `engine/menu/menu_commands.*`
- `engine/menu/menu_runtime_state.hpp`
- `engine/menu/menu_system.*`
- `engine/menu/menu_system_state.*`
- `engine/menu/menu_system_update.cpp`
- `engine/menu/menu_system_render.cpp`
- `engine/menu/screens/*`

Why it is a candidate:

- Menu stack, focus movement, commands, navigation, and screen data are
  reusable.
- It pairs naturally with `glayout` but should not require it internally.

Extraction boundary:

- Core library owns menu models, focus state, actions, navigation, and command
  results.
- Caller owns rendering, sound effects, input event translation, concrete
  screens, and applying commands.

Before ripout:

- Separate the menu runtime from Gubsy's built-in screens.
- Remove direct SDL, `EngineState`, mod, profile, and settings dependencies from
  the core menu code.
- Keep rendering as an adapter or example, not the core.

### 3. `gmods`

Priority: medium-high.

Source areas:

- `engine/mods.*`
- `engine/mods_helpers.cpp`
- `engine/mod_install.*`
- `engine/mod_server_config.*`
- `engine/mod_host.*`
- `engine/mod_api_registry.*`
- `engine/menu/screens/mods_screen.*`

Why it is a candidate:

- Mod manifest parsing, content versioning, dependencies, install state, and
  catalog data can be useful outside Gubsy.
- The current code probably mixes manifest logic with game-specific host and UI
  policy.

Extraction boundary:

- Core library owns manifest structs, dependency checks, content version
  records, install records, and catalog parsing.
- Caller owns scripting runtime, sandboxing, mod server policy, UI, filesystem
  roots, and game-specific content loading.

Before ripout:

- Identify the smallest manifest/catalog layer first.
- Do not extract Lua/sol bindings in the first pass.
- Do not bake in one mod server protocol unless that is the actual library
  goal.

### 4. `gassets`

Priority: medium.

Source areas:

- `engine/sprites.*`
- `engine/data.*`
- `engine/audio.*`
- `engine/mods.*` content scan pieces

Why it is a candidate:

- Asset ids, manifests, content names, and metadata are reusable.
- Actual texture/audio loading should stay at the edge.

Extraction boundary:

- Core library owns asset records, ids, names, manifest parsing, and lookup.
- Caller owns SDL textures, audio device buffers, OpenGL handles, hot reload,
  and game-specific asset lifetimes.

Before ripout:

- Decide whether this is one asset manifest library or separate sprite/audio
  libraries.
- Keep renderer and audio backend code out.
- Compare with the sprite extraction plan from `splonks-cpp`.

### 5. `gnet` or `gsession`

Priority: medium.

Source areas:

- `engine/session_contract.*`
- `engine/session_link.*`
- `engine/matchmaking.hpp`
- `engine/room_matchmaking.*`
- `engine/net_transport.hpp`
- `engine/sync_payload_codec.*`
- `engine/sync_transport_packet_codec.*`
- `engine/sync_transport_udp.*`

Why it is a candidate:

- Session contracts and packet codecs are more isolated than most engine code.
- Could become a small sync/session protocol library.

Extraction boundary:

- Core library owns packet formats, session descriptors, and encode/decode.
- Caller owns sockets, retry policy, room servers, matchmaking UX, and game
  state semantics.

Before ripout:

- Decide whether this is generic enough to justify extraction.
- Remove room-server assumptions from the core, or keep the name specific.
- Avoid turning this into a large networking framework.

### 6. `ginput`

Priority: medium-high.

Source areas:

- `engine/input_binding_utils.*`
- `engine/input_settings_profiles.*`
- `engine/input_sources.*`
- `engine/input_system.hpp`
- `engine/binds_profiles.*`
- `engine/binds_ui_helpers.*`

Why it is a candidate:

- Binding serialization, profiles, and action maps are reusable.
- The profile/schema layer is useful and fairly clear.
- Current sampling/backend code is tangled with SDL, layout editor hooks,
  settings, and Gubsy UI.

Extraction boundary:

- Core library owns integer action/axis ids, action names, bindings, profiles,
  serialization, reconciliation, and conflict checks.
- Optional adapters can translate SDL3, raylib, or other backend input into
  `ginput` device/control ids.
- Caller owns raw device event loops, UI, user profile references, save files,
  and gameplay input consumption.

Before ripout:

- Use the new `ginput` repo plan in `../ginput/docs/spec.md`.
- Keep SDL/raylib mapping code in adapters, not the core.
- Keep user-profile preferred input profile ids in `gconfig`, not `ginput`.

### 7. `gvid`, `gpool`, or `gcore`

Priority: low.

Source areas:

- `engine/vid.hpp`
- `engine/vid_pool.hpp`

Why it is a candidate:

- Stable handles and pools are useful.
- This is probably too small as a standalone repo.

Extraction boundary:

- Fold into a future tiny `gcore` only if several libraries need it.
- Do not create a repo for two headers unless it removes real duplication.

## Splonks-cpp Candidates

### 1. `gparticles`

Priority: high.

Source areas:

- `src/particles/system.*`
- `src/particles/particle_specs.*`
- `src/particles/sprite_particle.*`
- `src/particles/scripted_particle.*`
- `src/particles/ribbon_particle.*`
- `src/particles/segmented_sprite_particle.*`
- `src/particles/lighting_mode.hpp`
- `src/render/particles.*`

Why it is a candidate:

- Particle specs, emit/update logic, sprite particle variants, ribbons, and
  segmented particles are reusable game tech.
- The user specifically wants particle effects considered for ripout.

Extraction boundary:

- Core library owns particle specs, particle state, spawning, stepping, and
  generated draw commands.
- Caller owns renderer, textures, sprite frame database, camera, lighting
  backend, and world/game state.

Before ripout:

- Split simulation/spec code from render submission.
- Replace direct dependencies on Splonks `State`, renderer, and asset database
  with small caller-provided lookups.
- Decide whether scripted particles remain data-driven only or expose callback
  hooks.

### 2. `gsprites` or `ganim`

Priority: high.

Source areas:

- `src/sprite.*`
- `src/aframe.*`
- `src/raw_aframe.*`
- `src/aframe_animator.*`
- `src/aframe_id.hpp`
- `src/content_names.*`

Why it is a candidate:

- Sprite metadata, animation frames, animation ids, and animator state are
  reusable.
- The user specifically wants sprite and sprite-loading code considered for
  ripout.
- This likely supports `gparticles`.

Extraction boundary:

- Core library owns sprite definitions, animation frame definitions, animation
  lookup, animator state, and metadata parsing.
- Caller owns image decoding, GPU textures, draw batching, filesystem layout,
  and game-specific content names if needed.

Before ripout:

- Separate raw asset loading from runtime animation playback.
- Decide whether content-name mapping belongs here or in `gassets`.
- Keep renderer-specific frame drawing outside the library.

### 3. `gaudio`

Priority: medium.

Source areas:

- `src/audio_asset.*`
- `src/raw_audio_asset.*`
- `src/audio_asset_id.hpp`
- `src/audio_emitters.*`
- `src/audio_filter.*`

Why it is a candidate:

- Audio asset ids, metadata, emitters, and filters may be reusable.
- Audio device code and world acoustics are less portable.

Extraction boundary:

- Core library owns audio asset metadata, ids, and lightweight emitter data.
- Caller owns SDL/audio backend, decoding, mixing, listener placement, and
  world raycasts.

Before ripout:

- Keep `src/audio_acoustics.*` out initially because it depends on game world
  queries.
- Decide whether filters are data-only or backend-specific.

### 4. `gsettings`

Priority: medium.

Source areas:

- `src/settings.*`
- `src/menu/settings.*`
- `data/settings.cfg`

Why it is a candidate:

- Splonks has real settings/config needs that can inform the Gubsy settings
  extraction.
- This should probably not become a separate Splonks-specific settings repo.

Extraction boundary:

- Use Gubsy's future `gsettings`/`gconfig` as the canonical library.
- Pull requirements from Splonks before freezing the settings API.

### 5. `gstagegen`

Priority: low-medium.

Source areas:

- `src/stage_gen/*`
- `src/stage_gen/classic/*`
- `src/stage_gen/room_template_loader.*`

Why it is a candidate:

- Room template parsing and tile palette machinery may be reusable.
- The stage generation rules are very Splonks-specific.

Extraction boundary:

- Only consider extracting room template loading or generic weighted selection
  pieces first.
- Keep Spelunky-like generation rules in the game.

### 6. `gnetlock` or `grollback`

Priority: low-medium.

Source areas:

- `src/network/input_lockstep.*`
- `src/network/net_protocol.*`
- `src/network/net_session.*`
- `src/network/net_transport.*`
- `src/network/net_lobby*`
- `src/network/net_ids.hpp`
- `src/network/net_limits.hpp`

Why it is a candidate:

- Lockstep networking, packet protocol code, and lobby/session code can become
  reusable later.
- Current code is likely tied to Splonks player ids, input frames, stage ids,
  snapshots, and lobby policy.

Extraction boundary:

- Core library could own lockstep buffer rules, transport-neutral packet
  encoding, and session state machines.
- Caller owns game snapshots, input payload shape, lobby service, and transport
  implementation.

Before ripout:

- Wait until the net experiment stabilizes.
- Avoid extracting a moving target.

### 7. `gdebugplayback`

Priority: low.

Source areas:

- `src/debug/playback*.{cpp,hpp}`
- `src/debug/control_server.*`
- `src/debug/input_bot.*`

Why it is a candidate:

- Recording, replay, bot input, and control-server tooling are valuable.
- Current code is probably too game-specific for a first library.

Extraction boundary:

- Core library would own recording files, playback cursor, and event streams.
- Caller owns game snapshot format, UI, control commands, and simulation hooks.

### 8. `glighting`

Priority: low.

Source areas:

- `src/stage_lighting.*`
- `src/render/tile_lighting.*`
- `src/render/debug_lighting.*`
- `src/menu/lighting.*`
- `src/render/postfx.*`

Why it is a candidate:

- Lighting and postfx code can be reusable, but it is often renderer- and
  game-world-specific.

Extraction boundary:

- Only extract after renderer boundaries are clearer.
- Keep tile/world queries out of a core lighting library.

### 9. `gmath` or `gcore`

Priority: low.

Source areas:

- `src/math_types.hpp`
- `src/direction.*`
- `src/vid.hpp`
- `src/sid.*`
- small id and side/direction helpers

Why it is a candidate:

- Several future libraries may need shared ids, vectors, and directions.
- This can become junk-drawer code if extracted too early.

Extraction boundary:

- Create only when at least two extracted libraries need the same small core
  types.
- Keep it tiny and boring.

## First Likely Sequence

1. Replace Gubsy's parser users with `gsexp`.
2. Migrate Gubsy layout code to `glayout`.
3. Extract Gubsy settings/config as `gsettings` or `gconfig`.
4. Extract Splonks sprite/animation metadata as `gsprites` or `ganim`.
5. Extract Splonks particle simulation/specs as `gparticles`.
6. Revisit menu, mods/content, and asset catalog libraries once the first
   shared config/assets boundaries are real.
