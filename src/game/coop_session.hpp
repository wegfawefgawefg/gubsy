#pragma once

#include <string>

struct SyncSessionStats;

struct CoopStepResult {
    bool handled{false};
    int bonk_count{0};
};

void coop_session_reset();
bool coop_session_active();
CoopStepResult coop_session_step();
const std::string& coop_session_status_text();
const std::string& coop_session_last_error();
bool coop_session_query_stats(SyncSessionStats& out);
