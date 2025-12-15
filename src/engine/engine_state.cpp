#include "engine_state.hpp"
#include "globals.hpp"

bool init_engine_state() {
    if (!es)
        es = new EngineState{};
    es->mode = modes::TITLE;
    es->alerts.clear();
    return true;
}

void cleanup_engine_state() {
    delete es;
    es = nullptr;
}
