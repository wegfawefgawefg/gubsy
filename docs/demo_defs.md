# Demo Item Defs (Pattern for Mod APIs)

This repo ships a tiny “demo items” system to showcase how to expose C++ defs + Lua hooks to mods. The important pieces:

1. **Def structs live in C++** (`DemoItemDef` in `src/demo_items.hpp`). Mods never touch engine state directly—they register new defs.
2. **Mods register defs via Lua tables** (`register_item{ id=..., position=..., sprite=..., on_use=function(item) ... end }`). The loader copies table fields into the def struct and stores the callback.
3. **Callbacks use a minimal API surface** (the `api:*` methods). That API manipulates *game* state (player position, bonk toggles, alerts, etc.) so mods can change behaviour without touching the engine code.
4. **Gameplay pulls from the defs table** (`demo_item_defs()` returns the vector). Rendering, interaction logic, etc. iterate over those defs, spawn instances, and call `trigger_demo_item_use(def)` when needed.
5. **Instances can expose mutable state**. The demo now forwards live instance positions (`info.x/info.y`) to Lua and ships two helpers: `api:get_item_position(id)` and `api:set_item_position(id, x, y)`. Lua mods can move pads around at runtime, showing how to mutate per-instance data while still referencing the shared def.
6. **Mods can patch defs after the fact** via `patch_item{ id=\"base:teleport_pad\", color={...}, position={...} }`. This lets downstream mods tweak base templates without re-registering them—ideal for compatibility packs.
7. **Hot reload**: `load_demo_item_defs()` runs on startup and when scripts change, so edits take effect immediately.

Use this pattern when adding your own mod-facing systems:

- Define a `FooDef` struct in C++ with the properties you want to expose.
- Create a `register_foo{}` Lua binding via sol2 that fills `std::vector<FooDef>`.
- Provide an `api` namespace with helper methods to mutate game state safely.
- Keep a bounded pool of instances backed by `VID`s (see `DemoItemInstance`): when an instance is spawned, reuse a slot, bump its version, and keep `vid.id == slot`. This gives you deterministic memory use and safe handles.
- Drive your gameplay using those defs + instance pools (spawn entities, run hooks, etc.) instead of hardcoding data.

Sample mods shipped with the repo demonstrate layering:
- `mods/base` defines the core pads.
- `mods/shuffle_pack` adds additional pads that reposition the teleport pad using the per-instance API.
- `mods/pad_tweaks` patches base defs at load and exposes a “Reset Pads” item that applies more patches at runtime.

When forking the engine, move these “game” modules outside `src/engine/` so you can customize them freely while keeping the engine services (render/input/audio) untouched. This demo is intentionally small; it just demonstrates the canonical pattern.***
