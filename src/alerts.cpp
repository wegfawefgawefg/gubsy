#include "src/alerts.hpp"
#include "src/engine_state.hpp"

#include <algorithm>

namespace {

constexpr std::size_t kMaxAlerts = 8;

} // namespace

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
    add_alert(engine, text, AlertSeverity::Info);
}

void add_alert(EngineState& engine, const std::string& text, AlertSeverity severity) {
    Alert al;
    al.text = text;
    al.ttl = severity == AlertSeverity::Error ? 2.4f : 1.8f;
    al.severity = severity;
    engine.alerts.push_back(al);
    while (engine.alerts.size() > kMaxAlerts)
        engine.alerts.erase(engine.alerts.begin());
}
