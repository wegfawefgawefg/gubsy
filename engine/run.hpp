#pragma once

struct EngineState;

struct GubsyAppHooks {
    void* app_context{nullptr};
    void (*on_mods_changed)(void* app_context){nullptr};
};

bool do_the_gubsy(EngineState& engine, const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy(EngineState& engine);
