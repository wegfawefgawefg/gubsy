#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct SyncConnectionInfo {
    bool active{false};
    bool is_host{false};
    std::string server_url;
    std::string room_code;
    std::string host_secret;
    std::string local_member_id;
    std::string remote_endpoint;
};

struct SyncSessionHooks {
    void* ctx{nullptr};
    bool (*query_connection)(void* ctx, SyncConnectionInfo& out){nullptr};
    void (*query_member_ids)(void* ctx, std::vector<std::string>& out){nullptr};
    void (*tick_presence)(void* ctx){nullptr};
    double (*query_now)(void* ctx){nullptr};
};

struct SyncDriver {
    void* ctx{nullptr};
    bool (*build_local_input)(void* ctx, nlohmann::json& out){nullptr};
    void (*predict)(void* ctx,
                    const std::vector<std::string>& member_ids,
                    const std::vector<nlohmann::json>& current_inputs,
                    const std::vector<nlohmann::json>& previous_inputs,
                    float dt){nullptr};
    bool (*capture_snapshot)(void* ctx,
                             const std::vector<std::string>& member_ids,
                             std::uint64_t sim_frame,
                             nlohmann::json& out){nullptr};
    bool (*apply_snapshot)(void* ctx,
                           const nlohmann::json& snapshot,
                           std::vector<std::string>& member_ids_out){nullptr};
    void (*apply_local_view_input)(void* ctx, const nlohmann::json& input){nullptr};
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

void sync_session_configure(const SyncSessionHooks& hooks, const SyncDriver& driver);
void sync_session_reset();
bool sync_session_active();
SyncStepResult sync_session_step(float dt);
const std::string& sync_session_status_text();
const std::string& sync_session_last_error();
const std::string& sync_session_advertised_endpoint();
