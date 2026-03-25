# Remove Globals Plan

This document describes the plan to remove the shared global pointers from the
engine and move toward explicit runtime contexts.

Why remove globals
------------------

The current globals helped early development, but they create the wrong shape
for a reusable library:

- hidden dependencies
- engine code reaching into game state too easily
- poor separation between engine and game
- harder testing and embedding
- harder reasoning about ownership and initialization order

We should prefer explicit state ownership over ambient globals.

What we have now
----------------

Today the repo uses shared globals such as:

- `es` for engine state
- `ss` for game state
- `gg` for graphics
- `aa` for audio
- `mm` for mod manager

Those make many call paths shorter, but they also make boundaries leak.

Target shape
------------

Create the runtime state explicitly in `main` and pass what is needed through
small context structs.

Examples:

- `EngineRuntime`
  - engine-owned state and services
- `AppRuntime`
  - engine runtime + game state + app-owned services
- `FrameContext`
  - per-frame state needed during update/render
- `MenuContext`
  - engine menu context plus an app context pointer if needed

The exact names can change. The important part is:

- explicit ownership
- explicit lifetime
- no hidden cross-layer reachability

What not to do
--------------

Do not replace globals with a disguised global service locator.

Bad replacement:

- singleton registries that act like globals under a different name
- broad "get everything" APIs that hide dependencies again

Good replacement:

- pass references to the state that is actually needed
- use small context structs to avoid giant parameter lists
- keep callbacks and app hooks explicit

Migration strategy
------------------

Do this in stages.

1. Stop introducing new global usage.
2. Move game state out of engine globals first.
   - `ss` is the highest-value removal because it forces the engine/game split.
3. Move bootstrapping to explicit app/runtime construction in `main`.
4. Replace broad global reads with narrow context parameters in hot paths.
5. Remove the remaining engine-side globals once most call sites already take
   explicit context.

Suggested order
---------------

1. Remove `ss` from engine globals.
2. Replace sample boot registration in `engine/run.cpp` with app hooks/context.
3. Separate engine runtime state from game runtime state.
4. Convert menu/debug systems to take explicit context instead of reading shared
   globals.
5. Remove or shrink the remaining globals (`es`, `gg`, `aa`, `mm`) as each
   subsystem gains an explicit owner.

Why this should stay boring
---------------------------

We do not need giant template machinery to remove globals.

The intended solution is:

- plain structs
- references
- pointers where appropriate
- explicit callbacks/hooks

This should make the engine easier to use, not harder.

Practical target
----------------

The correct end state is:

- runtime state is created explicitly in app code
- engine subsystems take explicit context or references
- game code passes its own state through app-owned hooks
- the engine no longer depends on ambient global state to function
