#pragma once

struct GubsyAppHooks {
    void* app_context{nullptr};
    void (*on_mods_changed)(void* app_context){nullptr};
};

bool do_the_gubsy(const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy();
