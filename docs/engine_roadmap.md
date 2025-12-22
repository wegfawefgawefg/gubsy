# Engine Roadmap

This document tracks the remaining pillars for shipping a “complete” Gubsy 2D engine. Items are grouped by domain. Each entry notes the current status and a short description.

## Platform Targets
- **Windows / Linux / macOS** – ✅ Shipping (CMake project builds all three; input/audio/render tested).
- **Android** – ⏳ Pending. Need Gradle toolchain, SDL touch integration, on-screen controls, APK packaging.
- **iOS** – ⏳ Pending. Requires Xcode project, Metal/OpenGL ES layer, touch + sensor plumbing.
- **Touch UI** – ⏳ Tied to mobile targets; need touch gestures mapped into input system.

## Rendering & UI
- **Animated Sprites** – ⏳ Add sprite sheets + frame timelines and playback component.
- **Particle System** – ⏳ Simple CPU emitter for UI/gameplay FX with batching.
- **Layout Editor** – ✅ Complete (see `docs/ui_layout_editor.md`).
- **Layout NAVGRAPH Editor** – semi planned, uncomplete, old code exists. reimplementation needed. 
- **Menu Framework** – ✅ Dear ImGui prototype in engine; custom menu system in progress (see `docs/menu_system_plan2.md`).

## Input & Networking
- **Unified Input System** – ✅ Refactored to per-frame input frames (see `docs/input_system_plan.md`).
- **Rollback/Prediction Hooks** – ⏳ Provide helpers for input history and deterministic stepping.
- **Multiplayer/Lobby Tools** – ⏳ UI + API for session creation, invites, matchmaking (ties into Steam integration).

## Audio / Assets
- **Per-sound Metadata** – ⏳ Extend asset pipeline so each sound can define default gain/pan/looping.
- **Streaming Music** – ⏳ Add background music queue/ducking utilities.

## Modding
- **Runtime Mod Loader** – ✅ demos + docs (`completing_mods.md`).
- **Mod Browser/Menu** – ✅ engine download/install functions exist; UI still evolving.
- **Workshop/Steam Integration** – ⏳ depends on Steam tasks below.

## Steam Integration
- **Friends & Invites** – ⏳ Wrap Steam Friends/Invites APIs so games can open lobbies directly.
- **Networking/Relay** – ⏳ Use `SteamNetworkingSockets` for P2P relay + listen servers.
- **Cloud Saves** – ⏳ Provide engine hook to sync config/mod data to Steam Cloud.
- **Workshop** – ⏳ Optional; would allow uploading mods directly from engine UI.
- **Overlay hooks** – ❌ (out of scope; Valve handles overlay).

## Simulation Utilities
- **Spatial Index** – ⏳ Provide a uniform grid or BVH helper for collision queries.
- **Collision Helpers** – ⏳ Convenience functions for rect vs rect, grid vs rect, masks, etc.
- **Click Masks / Shape Tests** – ⏳ Helpers for polygon hit tests for custom UIs.

## Minor Features
- **Localization System** – ⏳ Asset pipeline + runtime lookup for localized strings.
- **Menu to Menu Navgraph** – ⏳ Define valid transitions between menus for smoother UX.
- **State Snapshotting/Scrubbing** – ⏳ Engine-level save/load of entire game state for debugging.
- **Debug Overlay** – ⏳ In-game overlay for FPS, memory usage, entity counts, etc.
- **Mode enter/exit hooks** – ✅ Engine-level hooks for game state transitions (menu <-> play).
- **Analytics Hooks** – ⏳ Simple event logging API for gameplay telemetry.
- **Achievements API** – ⏳ Engine-level API for defining and unlocking achievements.
- **Perf Profiling Tools** – ⏳ In-engine CPU/GPU profiling overlays and logging.


## Out of Scope
- Full 3D rendering pipeline.
- Story/cutscene authoring tools.
- A hard opinionated entity/component system (games define their own).

## Status Legend
- ✅ – Implemented
- ⏳ – Planned/in progress
- ❌ – Explicitly out of scope
