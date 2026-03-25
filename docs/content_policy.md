# Content Policy

`gubsy` is a code-first engine. That means not all game-owned content should be
forced through the mod system.

What belongs in `engine/assets/`
--------------------------------

Only generic engine-owned runtime assets:

- fallback fonts
- generic UI sounds
- engine icons
- other resources that are not specific to one game's rules or fiction

What belongs in `game/assets/`
------------------------------

Game-owned shipped content that does not need mod semantics:

- menu art
- logos
- cutscene media
- directly loaded data files with no dependency graph
- content that is always part of the game and is not meant to be enabled,
  disabled, patched, or distributed independently

What belongs in `game/mods/`
----------------------------

Game-owned shipped content packs that intentionally use the mod pipeline:

- content with manifests, dependencies, and mod APIs
- content meant to be hot-reloadable or patchable
- content the game wants to treat the same way as downloaded mods
- built-in content packs that serve as a base layer for other mods

What belongs in `data/mods/`
----------------------------

Writable runtime mod state:

- installed mods
- downloaded mods
- local mutable copies of built-in shipped mods
- any mod content the running game is allowed to alter or replace

What belongs in `tools/mod_repo/`
---------------------------------

Tool-side catalog content:

- local test repository data
- smoke-test install targets
- mod server source data

Current repo decision
---------------------

- `engine/assets/` stays engine-only.
- `game/mods/base` stays a built-in shipped mod on purpose.
  It is intentionally mod-shaped because it exercises manifests, mod APIs,
  hot reload, dependency handling, and content patching.
- optional demo packs stay in `tools/mod_repo/` unless we explicitly decide to
  ship them as part of the game.
- `game/assets/` exists conceptually for future direct-loaded game assets, but
  the current demo leans heavily on the mod pipeline, so most shipped content is
  still under `game/mods/`.

Practical rule
--------------

Use a mod only when the content benefits from mod behavior. If the content is
just normal built-in game data with no need for manifests, dependencies, patch
chains, or independent distribution, keep it in `game/assets/` instead.
