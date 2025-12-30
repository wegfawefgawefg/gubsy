# Binds Menu Plan

## Goals
- Add a top-level **Binds Profiles** manager in Settings.
- Allow creating/editing binds profiles, with per-action bindings.
- Add a choose-input screen filtered by input type.
- Keep UI layouts to 1280x720 only.
- Keep files ~300–500 LOC by splitting screens.

## Screen Map

### 1) Binds Profiles (Manager)
- Paged list of binds profiles.
- First slot: **New Binds Profile** (creates + opens editor).
- Selecting an existing profile opens editor.
- Back returns to Settings hub.

### 2) Binds Profile Editor
- Slot 1: **Name** (text input).
- Then a list of **Game Actions** (from game action registry).
- Selecting an action opens Action Bindings editor.
- Reset button: **Reset to Defaults** (single press, toast).
- Back returns to Binds Profiles manager.

### 3) Action Bindings Editor
- Title: action name.
- Slots of mappings (paged).
- Any empty slot is selectable; selecting opens Choose Input.
- Filled slot shows input label + Delete action.
- Always ensure there are more slots: if page full, add another page of empty slots.
- Back returns to Binds Profile Editor.

### 4) Choose Input
- Paged list + search.
- Filter inputs by action type (button vs axis).
- Selecting an input assigns it to the chosen slot, then returns to Action Bindings editor.

## Data + Flow
- Binds profiles come from `es->binds_profiles` / disk.
- A “current binds profile” is selected in the manager and passed to editor screens.
- Action list comes from game action registry (TBD source).
- Action type (button/axis) determines valid inputs.

## Open Questions (Resolve in code discovery)
- Where to pull the action list + action type (button/axis)?
- How input options are represented for binding (device + control IDs).

## Implementation Slices
1) Binds Profiles manager screen (Settings entry + list).
2) Binds Profile editor screen (name + action list + reset).
3) Action Bindings editor screen (paged mappings + delete + empty slot).
4) Choose Input screen (search + filter + select).
