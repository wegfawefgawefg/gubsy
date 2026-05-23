# Dependency Packaging Plan

`gubsy` should be easy to consume from one checkout. The current sibling
checkout model makes local development convenient, but it should not be part of
the normal build contract.

## Goal

Make `gubsy` self-contained by default. The standalone glib repos remain useful
as upstream homes, reference projects, and testbeds, but consumer projects
should import Gubsy rather than individual glibs.

The desired user experience is:

1. A game can vendor or submodule `gubsy`.
2. `gubsy` can build `gubsy::engine` without requiring sibling directories next
   to the repo.
3. Moving useful glib code into Gubsy-owned engine modules is explicit and easy.

## Previous Problem

`CMakeLists.txt` previously looked for sibling checkouts of:

1. `gsexp`
2. `glayout`
3. `ginput`

That was acceptable for local development, but it made the repo layout part of
the public build contract. A consumer should not need this directory shape:

```text
Gamedev/
  gubsy/
  gsexp/
  glayout/
  ginput/
```

The default contract should instead be a single `gubsy` checkout.

## Current Repo Layout

The current transition state uses bundled first-party copies under `libs/`:

```text
gubsy/
  libs/
    gsexp/
    glayout/
    ginput/
```

This solved the sibling checkout problem, but it is not the final public shape.
The cleaner target is to move the useful pieces into Gubsy private modules
under `src/`. The concrete migration plan lives in
`docs/src_demo_refactor_plan.md`.

The important rule is that the normal build does not require sibling repos.

## Target Repo Layout

```text
gubsy/
  include/gubsy/
    public consumer headers
  src/
    layout/
    input/
    sexp/
    settings/
    audio/
    ...
  demo/
    bundled example consumer
```

Consumers should link `gubsy::engine` and include Gubsy headers. They should not
need to know whether a subsystem originally came from `glayout`, `ginput`, or
another standalone experiment.

## CMake Direction

The final CMake direction is one normal consumer-facing target:

```cmake
target_link_libraries(splonks PRIVATE gubsy::engine)
```

Avoid exposing separate glib targets to downstream games as part of the normal
story. Internal helper targets are fine if they keep Gubsy's build organized,
but they should not become the consumer contract unless there is a concrete
reason.

## Consumer Workflows

### Normal Engine Consumer

```sh
git clone git@github.com:wegfawefgawefg/gubsy.git
cmake -S gubsy -B gubsy/build
cmake --build gubsy/build
```

This should work without cloning `gsexp`, `glayout`, or `ginput` as siblings.

### Updating From A Standalone Glib

Develop ideas in the standalone repo if that is convenient. When the change is
ready, port the useful source into the matching Gubsy engine module.

Example:

```sh
gsexp       -> src/sexp
glayout     -> src/layout
ginput      -> src/input
```

Do not blindly copy a repo forever if the target is an internal Gubsy module.
Keep the module names and includes shaped around Gubsy.

## Implementation Checklist

1. Done: remove sibling checkout requirement by copying `gsexp`, `glayout`, and
   `ginput` into `libs/`.
2. Done: change glib dependency setup in `CMakeLists.txt` to use bundled copies.
3. Done: change missing-dependency errors to point at `libs/`.
4. Done: update README to document `libs/` as bundled first-party source.
5. Next: migrate `libs/gsexp` into `src/sexp`.
6. Next: migrate `libs/glayout` into `src/layout`.
7. Next: migrate `libs/ginput` into `src/input`.
8. Verify this build after each step:
   - Top-level `gubsy` checkout using bundled glibs.

## What This Does Not Change

This does not throw away the glib work.

The split still gives useful boundaries:

1. `gsexp` owns sexpr parsing and writing.
2. `glayout` owns layout data and editor mechanics.
3. `ginput` owns input profile storage and bind lookup.

The change is about where the public boundary lives. `gubsy` should consume
those boundaries internally without making users manage or think about separate
glib targets.

## Decision

Default to one Gubsy-facing API. Do not add sibling dependency resolution,
parent dependency override paths, or downstream glib target requirements unless
there is a concrete need.
