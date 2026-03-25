#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/session_link.hpp"
#include "engine/session_contract.hpp"

struct SyncSessionHooks {
    SessionLinkHooks link{};
    void (*query_member_ids)(void* ctx, std::vector<std::string>& out){nullptr};
};

struct SyncDriver {
    void* ctx{nullptr};
    bool (*build_local_input)(void* ctx, std::vector<std::uint8_t>& out){nullptr};
    void (*predict)(void* ctx,
                    const std::vector<std::string>& member_ids,
                    const std::vector<std::vector<std::uint8_t>>& current_inputs,
                    const std::vector<std::vector<std::uint8_t>>& previous_inputs,
                    float dt){nullptr};
    bool (*capture_snapshot)(void* ctx,
                             const std::vector<std::string>& member_ids,
                             std::uint64_t sim_frame,
                             std::vector<std::uint8_t>& out){nullptr};
    bool (*apply_snapshot)(void* ctx,
                           const std::vector<std::uint8_t>& snapshot,
                           std::vector<std::string>& member_ids_out){nullptr};
    void (*apply_local_view_input)(void* ctx, const std::vector<std::uint8_t>& input){nullptr};
    void (*begin_reconcile)(void* ctx){nullptr};
    void (*finish_reconcile)(void* ctx,
                             const std::vector<std::string>& member_ids,
                             const std::string& local_member_id,
                             bool is_host){nullptr};
    void (*tick_correction)(void* ctx,
                            const std::vector<std::string>& member_ids,
                            const std::string& local_member_id,
                            bool is_host,
                            float dt){nullptr};
    void (*reset_runtime)(void* ctx){nullptr};
};

struct SyncStepResult {
    bool handled{false};
};

struct SyncSessionStats {
    bool active{false};
    bool is_host{false};
    bool has_authoritative_snapshot{false};
    bool snapshot_timed_out{false};
    std::size_t member_count{0};
    std::size_t pending_local_input_count{0};
    std::uint64_t sim_frame{0};
    std::uint64_t last_applied_snapshot_frame{0};
    std::uint64_t last_acked_local_input_seq{0};
    double connected_for_sec{0.0};
    double packet_idle_sec{0.0};
    double snapshot_idle_sec{0.0};
};

void sync_session_configure(const SyncSessionHooks& hooks, const SyncDriver& driver);
void sync_session_reset();
bool sync_session_active();
SyncStepResult sync_session_step(float dt);
const std::string& sync_session_status_text();
const std::string& sync_session_last_error();
const std::string& sync_session_advertised_endpoint();
bool sync_session_query_stats(SyncSessionStats& out);
