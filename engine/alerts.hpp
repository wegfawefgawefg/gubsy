// Alert utilities.
// Responsibility: aging and pruning ephemeral UI alerts each frame.
#pragma once

#include <cstddef> // For std::size_t, etc.
#include <string> // For std::string

struct Alert {
    // Use std::string explicitly to avoid ambiguity if 'string' is defined elsewhere
    std::string text; 
    float age{0.0f};
    float ttl{2.0f};
    bool purge_eof{false};
};

// Age and prune alerts using provided dt seconds.
struct EngineState;

void age_and_prune_alerts(EngineState& engine, float dt);

// Add a new alert with the given text.
void add_alert(EngineState& engine, const std::string& text);
