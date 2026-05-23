# Source And Demo Refactor Plan

Gubsy is moving from a fork-and-edit repo shape toward an importable C++ game
kit. The public API work is already in place under `include/gubsy/`. The next
refactor is about making the private implementation layout match that goal.

## Target Shape

```text
include/gubsy/
  public headers used by games

src/
  app/
  input/     replaces libs/ginput and current input implementation pieces
  layout/    replaces libs/glayout and current layout implementation pieces
  sexp/      replaces libs/gsexp and current sexpr parsing helpers
  menu/
  settings/
  profiles/
  lobby/
  mods/
  audio/
  platform/

demo/
  bundled example program that imports and exercises Gubsy

tools/
  smoke tests and maintenance tools

third_party/
  external source drops that are not Gubsy-owned modules
```

`include/gubsy/` is the public API.

`src/` is private implementation.

`demo/` replaces the old idea of a checked-out game living inside Gubsy. It is
an example consumer, not the intended downstream project structure.

## Decisions

1. `src/input/` replaces `libs/ginput`.
2. `src/layout/` replaces `libs/glayout`.
3. `src/sexp/` replaces `libs/gsexp`.
4. `demo/` should become `demo/` once the library boundary is stable.
5. Consumers link `gubsy::engine` and include `gubsy/...` headers.
6. Consumers do not link standalone `ginput`, `glayout`, or `gsexp` targets.
7. The standalone `g*` repos can remain useful as reference projects, but they
   are not the normal way to consume Gubsy.

## Why This Is Better Than `libs/`

`libs/` fixed the sibling-checkout problem, but it still makes first-party
Gubsy code look like third-party dependencies. These libraries are Gubsy-owned
modules. Their code should live where Gubsy's private implementation lives.

Moving them into `src/` gives a simpler consumer story:

```cmake
add_subdirectory(path/to/gubsy)
target_link_libraries(game PRIVATE gubsy::engine)
```

The game does not need to know which internals came from old ripout repos.

## Migration Order

1. Keep `include/gubsy/` stable while moving private files.
2. Move `libs/gsexp` into `src/sexp` first because it is small and used by the
   other imported modules.
3. Move `libs/glayout` into `src/layout` and keep the current Gubsy layout
   editor behavior unchanged.
4. Move `libs/ginput` into `src/input` and keep input profile behavior
   unchanged.
5. Move current `engine/` implementation files into matching `src/` folders by
   subsystem.
6. Rename `demo/` to `demo/` after `gubsy::engine` builds cleanly as a library.
7. Remove temporary forwarding headers and old private include paths only after
   all internal includes use the new paths.

## Per-Step Rules

1. Make small commits after each subsystem move.
2. Preserve user-visible behavior unless the commit explicitly says otherwise.
3. Do not start `gassets`, `gaudio`, `ganim`, `gparticles`, `gmods`, or
   `gnetcode` during this refactor.
4. Do not expose `src/` as a public include path.
5. Do not make downstream games manage standalone glib targets.
6. Avoid broad style rewrites while moving files.
7. Split files only when the split improves ownership or navigation.

## Verification After Each Slice

Run:

```sh
./scripts/build.sh
ctest --test-dir build --output-on-failure
./scripts/check_consumption_boundary.sh
```

Also keep `tools/consumer_smoke` passing. That smoke test is the guard that a
normal external game can import Gubsy without private include paths.

## Definition Of Done

1. `libs/gsexp`, `libs/glayout`, and `libs/ginput` are gone from the normal
   build.
2. Equivalent code lives under `src/sexp`, `src/layout`, and `src/input`.
3. The bundled example lives under `demo/`, not `demo/`.
4. `include/gubsy/` remains the only public Gubsy include root.
5. `gubsy::engine` remains the only normal consumer target.
6. The demo and consumer smoke test build and run with the same behavior as
   before the refactor.
