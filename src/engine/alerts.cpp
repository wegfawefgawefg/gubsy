#include "globals.hpp"

#include <algorithm>

void age_and_prune_alerts(float dt) {
    for (auto& al : es->alerts) al.age += dt;
    es->alerts.erase(std::remove_if(es->alerts.begin(), es->alerts.end(),
                                      [](const Alert& al) {
                                          return al.purge_eof || (al.ttl >= 0.0f && al.age > al.ttl);
                                      }),
                       es->alerts.end());
}

void add_alert(const std::string& text) {
    Alert al;
    al.text = text;
    al.ttl = 1.2f;
    es->alerts.push_back(al);
}