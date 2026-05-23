#pragma once

#include "gubsy/app.hpp"
#include "gubsy/runtime.hpp"

bool do_the_gubsy(GubsyRuntime& runtime, const GubsyAppHooks& hooks = {});
bool stop_doing_the_gubsy(GubsyRuntime& runtime);
