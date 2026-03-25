#pragma once

struct GubsyAppHooks {
    void (*on_mods_changed)(){nullptr};
};

bool do_the_gubsy(const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy();
