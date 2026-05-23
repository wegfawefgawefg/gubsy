#pragma once

#include "gubsy/engine_state.hpp"
#include "gubsy/menu/ids.hpp"

using GubsyRuntime = EngineState;

bool init_gubsy_runtime(GubsyRuntime& runtime, const GubsyAppConfig& config = {});
void cleanup_gubsy_runtime(GubsyRuntime& runtime);

const GubsyAppConfig& gubsy_runtime_config(const GubsyRuntime& runtime);
bool gubsy_runtime_has_menu_screen(const GubsyRuntime& runtime, MenuScreenId screen_id);
