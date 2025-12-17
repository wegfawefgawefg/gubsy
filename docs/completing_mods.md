# Mods Completion Checklist

We have the foundational ModHost/registry in place, but a few critical features are still missing before the engine can support the full “Factorio-style” workflow (download anything at any time, enable/disable per session, load assets + APIs on demand). This doc tracks the remaining work.

## 1. Activation/Deactivation API
- Add `activate_mod(const std::string& id)` / `deactivate_mod(const std::string& id)` on ModHost.
- These functions must:
  * Verify dependencies + version compatibility (`game_version`, required mods).
  * Spin up or teardown the Lua context via `ModApiRegistry` (bind/unbind APIs, run scripts, fire lifecycle hooks).
  * Update the active set exposed to the rest of the game so systems/UI know which mods are live.
- Provide a bulk helper (`set_active_mods(const std::vector<std::string>& ids)`) so the lobby can atomically swap the session’s mod set.

## 2. Asset Refresh on Activation
- Today we load sprites/textures/sounds once at startup. After activating or disabling mods mid-session we must re-run the asset pipeline for the changed mods:
  * `scan_mods_for_sprite_defs()` (or a per-mod variant) to rebuild `SpriteDef`s and IDs.
  * `load_all_textures_in_sprite_lookup()` so new/removed textures get reflected in the texture store.
  * `load_mod_sounds()` (or incremental) so audio assets line up with the active set.
- Order: activate mod → rescan assets → bind APIs → finalize gameplay systems. Deactivation should run in reverse (API cleanup, asset unload if we track references).

## 3. Granular Hot Reload
- File watcher should reload only the mods whose files changed:
  * Track active mods + their watched directories.
  * On change, call `deactivate_mod(id)` followed by `activate_mod(id)`; this re-triggers asset reload and API rebind just for that mod, avoiding global churn.

## 4. Install/Download Helpers
- Expose installer entry points separate from the UI:
  * `install_mod_from_catalog(const ModCatalogEntry&)` – downloads missing files, writes to `mods/<folder>`.
  * `ensure_mod_installed(const std::string& id, const std::string& url)` – convenience for lobby so it can pull a mod when a user selects it.
- These should eventually plug into the existing HTTP downloader (the same code the browser uses) but callable directly from gameplay code.

## 5. Session APIs
- Surface helpers that the lobby/session can call:
  * `get_installed_mods()` – manifest + “installed” flag.
  * `get_active_mods()` – current active set.
  * `plan_session_mods(new_set)` – validates dependencies, returns success/errors (missing deps, incompatible versions).
  * `apply_session_mods(new_set)` – convenience wrapper: `ensure_installed → set_active_mods`.

## 6. Save/Load Integration (future)
- Once activation is dynamic, saves must store:
  * Active mod list + versions.
  * Per-mod per-API serialized state (using the existing descriptor hooks).
- On load, the engine should:
  * Install/activate any missing mods (or fail gracefully if not available).
  * Hand each API its saved blob so gameplay state stays consistent.

## 7. Code Organization
- Keep implementation files under ~300–400 LoC; split helpers if needed:
  * `ModHost` may need submodules (activation, asset refresh, persistence) to stay readable.
  * Installer/downloader logic should remain separate from UI (e.g., `engine/mod_install.hpp/.cpp`).

Once these pieces land, we’ll have the core engine-level support needed for:
- Browsing/installing mods from the main menu.
- Selecting any mod (even freshly downloaded) in the session lobby.
- Loading/unloading content immediately before launching a run.

Until then, the mod system is still incomplete—even without the new menu UI, the backend APIs must exist so game code can call them directly.***
