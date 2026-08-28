# GView integration plan

## Milestone status

The standalone composition boundary is implemented. Gubsy now discovers sibling
GSEXP/GLayout/GView checkouts for developer iteration and otherwise fetches
pinned revisions. The engine links their public targets and no longer carries
tracked duplicate layout/S-expression implementation files.

Public adapters provide:

- Typed model reads/writes/conditions and direct Gubsy event dispatch.
- Conversion from the existing mapped `MenuInputState` to semantic GView
  navigation, avoiding a second controller binding map.
- Native pointer/text event accumulation without controller-as-mouse behavior.
- Game, engine, data, and runtime-mod asset-root resolution.
- Optional hosting of the standalone live authoring suite in Gubsy's existing
  ImGui frame.

The adapter smoke and full Gubsy test matrix validate the boundary. Consumer
mode omits authoring while retaining the runtime API. The renderer remains an
explicit game integration choice until the reviewed GView UI replaces a real
menu: a game may link `gview::sdl3` or consume renderer-neutral paint commands
through its existing backend without Gubsy forcing that policy into GView.

The complete 18-state executable, visual evidence, controller self-test, and
benchmarks live in `gubsy-ui-kit-trials/gview`. A different-game UI suite and
then Splonks migration remain the next milestones, not hidden work in this one.

## Goal

Package standalone GLayout and GView cleanly inside the Gubsy ecosystem while
keeping them independently consumable. Generalize proven Gubsy/Splonks UI
authoring tools so each Gubsy game receives the same workflow without rebuilding
it locally.

## Dependency ownership

- Standalone `glayout` is authoritative for layout code.
- Standalone `gview` is authoritative for presentation, interaction, controls,
  and optional GView tools.
- Gubsy provides adapters, standard registration, engine state scenarios, asset
  access, Gubsy actions/events, and debug-window hosting.
- Do not retain copied source trees after validated dependency integration.
- Preserve unrelated Gubsy behavior and avoid dirty Splonks worktree mutation.

## Existing tools to generalize

The Splonks-embedded Gubsy currently demonstrates:

- Independent internal-render and window resolution controls.
- Grouped desktop/tablet/phone presets.
- Form-factor forcing, safe-area controls, fit/stretch, sampling, zoom, and pan.
- Active-layout following and stored variant selection.
- Layout creation/duplication and stable generated object IDs.
- Selection, multi-selection, dragging, resizing, numeric editing, snapping,
  undo/redo, and saving.
- Explicit per-widget directed navigation relationships.

Search histories and nearby repositories for the remembered visual navigation
editor before rebuilding it. Current inspected source exposes the graph and
runtime behavior but constructs most edges manually.

## Standard Gubsy tool suite

A single debug registration should expose:

- GLayout hierarchy/layout editor.
- GView presentation/state inspector.
- Directed focus-graph editor.
- View and fake scenario selection.
- Resolution/aspect/DPI/form-factor/safe-area simulation.
- Live reload and persistence.
- Runtime timing, dirty-domain, and allocation inspection.

The suite uses optional ImGui targets. Release consumers do not link or retain
the tools unless explicitly enabled.

## Runtime adapters

Gubsy supplies adapters for:

- Asset lookup and texture/font lifetime.
- SDL/GPU renderer hosting used by Gubsy games.
- Gubsy input actions, local-player devices, and controller hotplug.
- Gubsy event dispatch and settings/profile state.
- Custom game render surfaces and render-target composition.

Adapters do not force those policies back into standalone GLayout/GView cores.

## Migration discipline

Integrate into the clean standalone Gubsy repository first. Treat the dirty
Splonks checkout as read-only evidence. Validate with focused tools/tests before
removing duplicated code. Final Splonks menu migration occurs only after the
GView trial and later multi-game UI suite are reviewed and stable.

## Code quality

- Roughly 300-500 lines maximum per source file.
- Cohesive domains and mostly flat organization.
- Terse what-is comments above paragraph blocks.
- Direct, low-magic, debugger-friendly C++.
- No broad unrelated cleanup during integration.
