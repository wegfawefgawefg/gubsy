#include "src/async_matchmaking.hpp"

#include "src/room_matchmaking.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

struct AsyncMatchmakingClient::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<AsyncHeartbeatRequest> heartbeat_requests;
    std::vector<AsyncHeartbeatResult> heartbeat_results;
    bool stopping{false};
    std::thread worker;
    RoomServerMatchmaking matchmaking;
};

namespace {

AsyncHeartbeatResult process_heartbeat(RoomServerMatchmaking& matchmaking,
                                       const AsyncHeartbeatRequest& request) {
    AsyncHeartbeatResult result;
    result.request_id = request.request_id;
    const MatchmakingRoom* update = request.room_update ? &*request.room_update : nullptr;
    result.ok = matchmaking.heartbeat_room(request.server_url,
                                           request.room_code,
                                           request.member_id,
                                           request.display_name,
                                           request.host_secret,
                                           update,
                                           result.err);
    if (!result.ok)
        return result;

    result.has_room = matchmaking.fetch_room(request.server_url,
                                            request.room_code,
                                            result.room,
                                            result.err);
    return result;
}

} // namespace

AsyncMatchmakingClient::AsyncMatchmakingClient() : impl_(std::make_unique<Impl>()) {
    impl_->worker = std::thread([impl = impl_.get()]() {
        while (true) {
            AsyncHeartbeatRequest request;
            {
                std::unique_lock<std::mutex> lock(impl->mutex);
                impl->cv.wait(lock, [&]() {
                    return impl->stopping || !impl->heartbeat_requests.empty();
                });
                if (impl->stopping)
                    break;
                request = std::move(impl->heartbeat_requests.front());
                impl->heartbeat_requests.pop_front();
            }

            AsyncHeartbeatResult result = process_heartbeat(impl->matchmaking, request);

            {
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->heartbeat_results.push_back(std::move(result));
            }
        }
    });
}

AsyncMatchmakingClient::~AsyncMatchmakingClient() {
    shutdown();
}

void AsyncMatchmakingClient::enqueue_heartbeat(AsyncHeartbeatRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        impl_->heartbeat_requests.push_back(std::move(request));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncHeartbeatResult> AsyncMatchmakingClient::drain_heartbeat_results() {
    std::vector<AsyncHeartbeatResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->heartbeat_results);
    return results;
}

void AsyncMatchmakingClient::shutdown() {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stopping = true;
        impl_->heartbeat_requests.clear();
    }
    impl_->cv.notify_all();
    if (impl_->worker.joinable())
        impl_->worker.join();
}
