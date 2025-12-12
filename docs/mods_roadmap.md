# Mod System Roadmap

Goal: support Factorio-style mods (base content shipped as a mod, additional mods layered on top, downloadable from a simple mod server) while keeping the demo lightweight. Proposed steps:

1. **Package Format & Loader** ✅
   - Formalize `manifest.json` for each mod (`id`, `name`, `version`, `dependencies`, optional `download_url`).
   - Loader scans `mods/*/manifest.json`, resolves dependencies (topological sort), executes each mod’s scripts in load-order, and logs conflicts.
   - Provide helpers so mods can query/update existing defs (e.g., `find_item_def(id)`), enabling cross-mod tweaks.
   - Persist an “enabled mods” list in config; disabled mods are skipped but stay installed.

2. **Sample Mods** ✅
   - Move current demo content into a `base` mod + add example mods (“shuffle_pack”, “pad_tweaks”) to showcase additive + override workflows.
   - Ensure hot reload still works per mod (reload scripts when files change).

3. **Mods Menu & Browser (next)**
   - Add a “Mods” button to main menu.
   - Stage 1: use mocked catalog data (title/version/description/deps/image placeholder) with Install/Installed buttons that flip local state.
   - Stage 2: hook to the mod server endpoint (HTTP JSON + zip downloads), persist install state, allow uninstall/enable toggles, and integrate search/filter UI.
   - Remember Factorio flow: the browser simply showcases available mods; when starting a new game, let players enable mods (auto-install missing ones if needed).

4. **Reference Mod Server**
   - Ship a tiny HTTP server (separate binary) that serves:
     * `index.json` listing available mods and metadata.
     * Static `.zip` archives per mod.
   - Keep it simple (no auth) so teams can host their own mod repos or run locally during development.

5. **Cleanup / Engine Focus**
   - Remove leftover game-specific systems we no longer demo.
   - Document the pattern (maybe `docs/modding.md`) so engine forks know how to add new def domains and mod APIs.

Work through the steps sequentially: start with manifests + loader changes, then author the sample mods, then UI/browser, finally the server. Continuous testing with multiple mods loaded ensures compatibility as the system evolves.
