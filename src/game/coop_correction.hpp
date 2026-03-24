#pragma once

#include <string>
#include <vector>

struct State;

void demo_sync_correction_reset();
void demo_sync_correction_begin(const State& state, const std::vector<std::string>& member_ids);
void demo_sync_correction_finish(State& state,
                                 const std::vector<std::string>& member_ids,
                                 const std::string& local_member_id,
                                 bool is_host);
void demo_sync_correction_tick(State& state,
                               const std::vector<std::string>& member_ids,
                               const std::string& local_member_id,
                               bool is_host,
                               float dt);
