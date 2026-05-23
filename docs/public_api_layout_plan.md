# Public API Layout Plan

`gubsy` is moving toward being imported as a C++ game kit. That means the public
headers a game includes should be clear and stable. Internal implementation
files should be free to move without breaking a game like `splonks-cpp`.

## Decision

Use a normal C++ library layout:

```text
include/gubsy/
  app.hpp
  run.hpp
  runtime.hpp
  menu/
  layout/
  input/
  profiles/
  settings/
  lobby/
  mods/

src/
  app/
  menu/
  layout/
  input/
  profiles/
  settings/
  lobby/
  mods/
```

`include/gubsy/` is the public API.

`src/` is private implementation.

## Why Not Expose `src/` Directly

It is technically possible to put public headers in `src/` or `engine/` and add
that directory as a public include path.

Example:

```cmake
target_include_directories(gubsy_engine PUBLIC src)
```

That works, but it makes every helper header in `src/` easy for consumers to
include. Internal files then accidentally become public API, and later cleanup
breaks users.

The safer rule is:

```cmake
target_include_directories(gubsy_engine
  PUBLIC
    include
  PRIVATE
    src
)
```

Consumers can include:

```cpp
#include <gubsy/layout/layout.hpp>
#include <gubsy/input/binds.hpp>
#include <gubsy/menu/menu.hpp>
```

Consumers should not include private implementation headers.

## Current State

The repo still has most reusable code under `engine/`, and many headers include
other headers as:

```cpp
#include "engine/run.hpp"
#include "engine/menu/menu_manager.hpp"
```

That is acceptable during migration, but it should not become the long-term
consumer-facing API.

## Target Consumer Shape

A game should consume one normal target:

```cmake
add_subdirectory(path/to/gubsy)
target_link_libraries(splonks PRIVATE gubsy::engine)
```

The repo has `tools/consumer_smoke` to verify this mode. It is a separate CMake
project that imports Gubsy with `add_subdirectory`, links only `gubsy::engine`,
and includes only `gubsy/...` headers.

Game code should include Gubsy headers through the `gubsy/` prefix:

```cpp
#include <gubsy/app.hpp>
#include <gubsy/run.hpp>
#include <gubsy/runtime.hpp>
#include <gubsy/layout/layout.hpp>
#include <gubsy/input/binds.hpp>
#include <gubsy/input/profile_settings.hpp>
#include <gubsy/input/sources.hpp>
#include <gubsy/input/types.hpp>
#include <gubsy/menu/ids.hpp>
#include <gubsy/lobby/session_contract.hpp>
#include <gubsy/lobby/session_link.hpp>
#include <gubsy/profiles/player.hpp>
#include <gubsy/profiles/user_profile.hpp>
#include <gubsy/settings/game_settings.hpp>
#include <gubsy/settings/schema.hpp>
#include <gubsy/settings/top_level_settings.hpp>
#include <gubsy/settings/types.hpp>
```

The game should not need to know whether a subsystem was once a standalone
`glayout`, `ginput`, or `gsexp` repo.

Basic runtime setup should also use public names:

```cpp
GubsyRuntime runtime{};
GubsyAppConfig config{};
init_gubsy_runtime(runtime, config);
cleanup_gubsy_runtime(runtime);
```

The older `init_engine_state` and `cleanup_engine_state` names remain internal
migration details. Consumer smoke tests should avoid them.

`GubsyRuntime` is an owning public wrapper around the internal `EngineState`.
Consumers should not include or depend on `engine/engine_state.hpp`.

## Migration Order

1. Done: create `include/gubsy/` facade headers for the APIs Splonks would use first.
2. Done: keep the existing `engine/` implementation compiling underneath.
3. Done: add `include/` as the public include directory for `gubsy_engine`.
4. Done: add public runtime init/cleanup/query helpers so consumer examples do
   not call `init_engine_state` directly.
5. Keep `engine/` or later `src/` as a private include directory.
6. Move implementation files from `engine/` to `src/` by subsystem.
7. Move current first-party `libs/` code into the matching Gubsy modules as part
   of the subsystem migrations.
8. Stop documenting `engine/...` includes once public `gubsy/...` headers exist.
9. Remove direct consumer reliance on standalone glib targets.

## First Facade Headers To Add

Start with the APIs needed by a Splonks-style consumer:

1. Done: `include/gubsy/app.hpp`
2. Done: `include/gubsy/run.hpp`
3. Done: `include/gubsy/runtime.hpp`
4. Done: `include/gubsy/input/binds.hpp`
5. Done: `include/gubsy/profiles/profiles.hpp`
6. Done: `include/gubsy/settings/settings.hpp`
7. Done: `include/gubsy/menu/menu.hpp`
8. Done: `include/gubsy/layout/layout.hpp`
9. Done: `include/gubsy/lobby/session.hpp`
10. Done: `include/gubsy/menu/types.hpp`
11. Done: `include/gubsy/menu/ids.hpp`
12. Done: `include/gubsy/input/types.hpp`
13. Done: `include/gubsy/settings/types.hpp`
14. Done: `include/gubsy/settings/schema.hpp`
15. Done: `include/gubsy/lobby/session_contract.hpp`
16. Done: `include/gubsy/lobby/net_transport.hpp`
17. Done: `include/gubsy/lobby/matchmaking.hpp`
18. Done: `include/gubsy/lobby/session_link.hpp`
19. Done: `include/gubsy/profiles/user_profile.hpp`
20. Done: `include/gubsy/profiles/player.hpp`
21. Done: `include/gubsy/settings/game_settings.hpp`
22. Done: `include/gubsy/settings/top_level_settings.hpp`
23. Done: `include/gubsy/input/profile_settings.hpp`
24. Done: `include/gubsy/input/sources.hpp`

Do not expose everything at once. Expose only APIs that are ready to be used by
a game importing Gubsy.

## Naming Rule

Public headers use the `gubsy/` prefix.

Private headers should not be included by consumers. If a private header becomes
useful to a game, promote a small public header intentionally instead of making
the private path public.

## Relationship To `engine/`

`engine/` is a transitional implementation folder. It can stay while the public
API takes shape. The long-term goal is not to make games include `engine/...`;
the long-term goal is to let games include `gubsy/...`.

## Definition Of Done

This migration is done when:

1. A downstream game can build against `gubsy::engine` using only
   `#include <gubsy/...>` headers.
2. `engine/` is no longer a public include path.
3. Private helpers live under `src/` or another private-only path.
4. The bundled sample still builds as a normal consumer.
5. Splonks can use Gubsy menu/profile/input/lobby APIs without cloning or
   linking separate glib repos.
