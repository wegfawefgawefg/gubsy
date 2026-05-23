#pragma once

#include "gubsy/app.hpp"
#include "gubsy/menu/ids.hpp"

struct EngineState;

class GubsyRuntime {
public:
    GubsyRuntime();
    ~GubsyRuntime();

    GubsyRuntime(const GubsyRuntime&) = delete;
    GubsyRuntime& operator=(const GubsyRuntime&) = delete;

    GubsyRuntime(GubsyRuntime&& other) noexcept;
    GubsyRuntime& operator=(GubsyRuntime&& other) noexcept;

private:
    EngineState* engine_{nullptr};

    friend EngineState& gubsy_runtime_engine(GubsyRuntime& runtime);
    friend const EngineState& gubsy_runtime_engine(const GubsyRuntime& runtime);
};

bool init_gubsy_runtime(GubsyRuntime& runtime, const GubsyAppConfig& config = {});
void cleanup_gubsy_runtime(GubsyRuntime& runtime);

const GubsyAppConfig& gubsy_runtime_config(const GubsyRuntime& runtime);
bool gubsy_runtime_has_menu_screen(const GubsyRuntime& runtime, MenuScreenId screen_id);
