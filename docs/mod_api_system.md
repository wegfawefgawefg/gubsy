# Mod Host & API Registry Plan

Goal: keep the engine responsible for mod discovery, dependency order, runtime management, and save persistence, while letting each game expose its own modding surfaces (items, inventory, maps, etc.). Mods target the APIs they care about instead of poking engine state directly.

## Manifest & Compatibility
Each mod ships a `manifest.json` (or TOML equivalent) with the following keys:

```json
{
  "id": "my_mod",
  "title": "Cool Mod",
  "version": "1.2.0",
  "game_version": "0.5",          // single compatibility gate for the engine/game build
  "dependencies": ["base >= 0.5"],
  "optional_dependencies": ["other_mod >= 1.0"],
  "apis": ["demo_items", "maps"]  // APIs this mod expects to exist
}
```

- We keep a single `game_version` string; when the engine’s mod-facing API surface changes, bump it. Mods written for earlier versions are flagged until they update.
- Dependencies/optional dependencies work like today’s mod loader (topological sort, detect cycles/missing deps).
- `apis` enumerates which mod APIs the mod uses. This is declarative glue between mod scripts and the systems registered by the game.

The existing mod browser/downloader continues to fetch manifests and files exactly as before; it simply surfaces the extra metadata so players/devs see which APIs a mod expects. No need to redesign the catalog for this step.

## Engine Mod Host

New central service that handles the plumbing bits once for the whole game:

```cpp
struct ModContext {
    std::string id;
    std::string path;
    sol::state lua;
    std::vector<std::string> requested_apis;
};

class ModHost {
public:
    bool init(const std::string& mods_root);
    void discover();                    // scan manifests, resolve dependencies
    bool enable_mod(const std::string& id);
    bool disable_mod(const std::string& id);

    bool load_enabled_mods();           // spin up contexts, bind APIs, run scripts
    void unload_all();
    bool reload_mod(const std::string& id); // hot reload one mod when files change

    void save_state(SaveWriter& writer);
    void load_state(SaveReader& reader);

private:
    std::vector<ModInfo> mods;                      // manifest data + dependency ordering
    std::unordered_map<std::string, ModContext> active;
};
```

Responsibilities:
- Discover/install: reuse `mods/*` layout + existing downloader. After discovery, `ModHost` owns the authoritative list of installed mods.
- Enable/disable: track per-save/per-session enabled sets (the menu/lobby can still edit a list; `ModHost` provides the backend state).
- Runtime sandboxing: create a Lua state per mod, open libraries, expose shared helpers (logging, RNG, asset lookup).
- API wiring: for each mod, look at `manifest.apis` and inject/bind the requested APIs before running the mod’s scripts.
- Lifecycle: `load_enabled_mods()` (startup), `reload_mod()` (hot reload), `unload_all()` (shutdown).
- Persistence: when saving, ask each API module to serialize per-mod state; on load, restore before gameplay starts.

## Mod API Registry

Games declare their mod-facing systems through a registry. The engine stays agnostic about what “items” or “maps” mean; it simply provides the glue.

```cpp
struct ModApiDescriptor {
    std::string name;  // "demo_items", "inventory", etc.

    std::function<void(sol::state&, ModContext&)> bind_lua;
    std::function<void(ModContext&)> on_mod_loaded;
    std::function<void(ModContext&)> on_mod_unloaded;
    std::function<void(ModContext&, SaveWriter&)> save_mod_state;
    std::function<void(ModContext&, SaveReader&)> load_mod_state;
};

class ModApiRegistry {
public:
    void register_api(const ModApiDescriptor& desc);
    const ModApiDescriptor* find(std::string_view name) const;
};
```

Typical API module flow (pseudo):
```cpp
void register_demo_items_api(ModApiRegistry& reg) {
    ModApiDescriptor desc;
    desc.name = "demo_items";
    desc.bind_lua = [](sol::state& lua, ModContext& ctx) {
        lua.set_function("register_item", [&ctx](sol::table def) {
            DemoItemDef parsed = parse_item_table(def, ctx.id);
            demo_items::add_def(ctx.id, parsed);
        });
        lua["api"] = demo_items::make_runtime_api();
    };
    desc.on_mod_loaded = [](ModContext& ctx) {
        demo_items::notify_mod_loaded(ctx.id);
    };
    desc.on_mod_unloaded = [](ModContext& ctx) {
        demo_items::remove_mod(ctx.id);
    };
    desc.save_mod_state = [](ModContext& ctx, SaveWriter& w) {
        demo_items::save_state_for_mod(ctx.id, w);
    };
    desc.load_mod_state = [](ModContext& ctx, SaveReader& r) {
        demo_items::load_state_for_mod(ctx.id, r);
    };
    reg.register_api(desc);
}
```

When `ModHost` loads a mod:
1. Create `ModContext` (Lua state, manifest data).
2. For each `manifest.apis` entry, fetch descriptor and run `bind_lua`.
3. Execute the mod’s scripts (e.g., `scripts/init.lua`). Those scripts call into the bound functions to register defs, patch existing content, etc.
4. After script execution, call `on_mod_loaded` so the API module can rebuild caches or spawn instances.

Hot reload/unload run `on_mod_unloaded` then re-run `bind_lua` + scripts + `on_mod_loaded`.

Save/load call `save_mod_state` / `load_mod_state` per API so each module persists data relevant to that mod (e.g., runtime positions, custom timers). The engine save format stores `{mod_id, api_name} -> blob`.

## Multiplayer Considerations
- Mods never see networking primitives; they interact only via APIs. The gameplay systems implementing those APIs remain responsible for replication/determinism (same as today). As long as all peers load the same enabled mod set + versions, the existing systems keep state in sync.
- Lobby/session flow: when starting multiplayer, the host shares its enabled mod list + versions; clients download missing mods (existing downloader), then `ModHost` ensures every client loads the same contexts before gameplay begins.

## Migration Plan
1. **Documentation** (this file) so future engine forks understand the architecture.
2. **Rework demo systems** (items, demo content) to register via `ModApiRegistry` instead of manually iterating mods. This gives us concrete API descriptors to test the host with.
3. **Stub ModHost**: implement discovery + API wiring + script execution using the current loader/hot-reload infrastructure.
4. **Hook existing mods** (mods/base, mods/shuffle_pack, etc.) by adding `manifest.json` entries listing `apis` and updating their Lua scripts to use the injected bindings. Verify hot reload + persistence.
5. **Extend save/load** to capture per-mod API state. For now the demo items can serialize instance positions as proof-of-concept.
6. **Later**: reconnect the mod browser/lobby UI to the new state (enabled mods list, compatibility warnings). Since the host still uses the same mod directories/manifests, existing downloader code just needs to call into the new `ModHost`.

This approach keeps the engine flexible (games define their own APIs) while centralizing the repetitive chores—file management, runtime contexts, lifecycle management, and persistence.***
