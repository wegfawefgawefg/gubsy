#include "src/alerts.hpp"
#include "src/engine_state.hpp"

#include <algorithm>

void age_and_prune_alerts(EngineState& engine, float dt) {
    for (auto& al : engine.alerts)
        al.age += dt;
    engine.alerts.erase(std::remove_if(engine.alerts.begin(),
                                       engine.alerts.end(),
                                       [](const Alert& al) {
                                           return al.purge_eof ||
                                                  (al.ttl >= 0.0f && al.age > al.ttl);
                                       }),
                        engine.alerts.end());
}

void add_alert(EngineState& engine, const std::string& text) {
    Alert al;
    al.text = text;
    al.ttl = 1.2f;
    engine.alerts.push_back(al);
}
