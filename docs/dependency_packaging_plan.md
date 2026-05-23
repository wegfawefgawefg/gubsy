# Dependency Packaging Plan

`gubsy` should be easy to consume from one checkout. The current sibling
checkout model makes local development convenient, but it should not be part of
the normal build contract.

## Goal

Make `gubsy` self-contained by default. The standalone glib repos remain useful
as upstream homes for reusable code, but `gubsy` should build from bundled
first-party copies.

The desired user experience is:

1. A game can vendor or submodule `gubsy`.
2. `gubsy` can build `gubsy::engine` without requiring sibling directories next
   to the repo.
3. Updating a glib version inside `gubsy` is explicit and easy.

## Previous Problem

`CMakeLists.txt` previously looked for:

1. `../gsexp`
2. `../glayout`
3. `../ginput`

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

## Repo Layout

Use bundled first-party copies under `libs/`:

```text
gubsy/
  libs/
    gsexp/
    glayout/
    ginput/
```

Use copied source snapshots first. That is the simplest consumer model: `gubsy`
contains the exact source it builds against.

Git submodules or git subtrees can still be considered later if manual copying
becomes irritating. The important rule is that the normal build does not require
sibling repos.

## CMake Resolution

Resolve each glib dependency from its bundled first-party copy:

1. Use bundled `libs/` copies.
2. Otherwise fail with a specific message explaining which bundled dependency is
   missing.

Bundled deps should be the default. Do not add extra override paths until there
is a concrete need.

## Consumer Workflows

### Normal Engine Consumer

```sh
git clone git@github.com:wegfawefgawefg/gubsy.git
cmake -S gubsy -B gubsy/build
cmake --build gubsy/build
```

This should work without cloning `gsexp`, `glayout`, or `ginput` as siblings.

### Updating A Bundled Glib

Develop glib changes in the standalone repo first. When the change is ready,
update the bundled copy in `gubsy`.

Copy the updated glib source into `libs/` and commit it in `gubsy`:

```sh
rm -rf gubsy/libs/gsexp
cp -R gsexp gubsy/libs/gsexp
git -C gubsy add libs/gsexp
```

Ignore the copied glib's `.git` directory if present. The important point is
that `gubsy` records the exact source it builds against.

## Implementation Checklist

1. Done: add `libs/gsexp`, `libs/glayout`, and `libs/ginput` as
   copied bundled source.
2. Done: change glib dependency setup in `CMakeLists.txt` to use bundled copies.
3. Done: change missing-dependency errors to point at `third_party/`.
4. Done: update README to document `libs/` as bundled first-party source.
5. Verify this build:
   - Top-level `gubsy` checkout using bundled glibs.

## What This Does Not Change

This does not undo the glib split.

The split still gives useful boundaries:

1. `gsexp` owns sexpr parsing and writing.
2. `glayout` owns layout data and editor mechanics.
3. `ginput` owns input profile storage and bind lookup.

The change is only about dependency packaging. `gubsy` should consume those
boundaries without making users manage a pile of sibling checkouts.

## Decision

Default to bundled source. Do not add sibling dependency resolution or parent
dependency override paths until there is a concrete need.
