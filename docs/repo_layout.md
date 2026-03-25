# Repo Layout

`gubsy` is structured as one engine plus one game project using it. The layout is
meant to keep engine infrastructure separate from game semantics without forcing
a plugin ABI or a reflection-heavy framework.

Top-level directories
---------------------

- `engine/`
  - Reusable runtime and infrastructure code.
  - Engine-owned built-in assets only when they are genuinely generic.
  - Examples: window/input/audio plumbing, menu framework, save/load helpers,
    matchmaking/transport services, generic debug tooling.
- `game/`
  - The actual game project built on top of the engine.
  - Game-owned code, assets, metadata meaning, gameplay protocol, and built-in
    shipped mods/content packs.
- `tools/`
  - Standalone helper programs such as servers, smoke tests, importers, and
    catalog tooling.
  - Tool-side source data belongs here if it exists only to support tools.
- `data/`
  - Writable runtime state for the current game project.
  - This is where live settings, profiles, saves, installed mods, cache, logs,
    and runtime-edited layout data belong.

Ownership rules
---------------

- The engine owns infrastructure, not gameplay semantics.
- The game owns gameplay state, rendering meaning, asset metadata meaning,
  serialization meaning, and networking policy.
- Defaults live with the thing that ships them.
- Mutable runtime state lives under `data/`.

Content vs runtime state
------------------------

- `engine/assets/`
  - Engine-owned fallback assets and generic built-in resources.
- `game/assets/`
  - Game-authored shipped assets.
- `game/mods/`
  - Game-shipped built-in mods or content packs.
- `data/mods/`
  - Installed, downloaded, or otherwise writable runtime mod state.
- `tools/mod_repo/`
  - Tool-side catalog/repository source used by the local mod server and smoke
    flows. This is not runtime game state.

Practical rules
---------------

- Do not put mutable user/runtime files under `engine/` or `game/`.
- Do not put gameplay-specific assets under `engine/assets/`.
- Do not treat `data/` as authored content. It is runtime state.
- If a file exists only to support a local tool or test harness, keep it under
  `tools/` rather than `game/` or `data/`.

Why this split
--------------

This keeps the code-first model intact:

- games are still normal C++ code using engine services directly
- the engine still helps with infrastructure
- the engine does not become a gameplay framework that owns the game's state
  model
