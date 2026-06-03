#include "src/async_matchmaking.hpp"

#include "src/room_matchmaking.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace {

struct AsyncMatchmakingWorkItem {
    enum class Kind {
        Heartbeat,
        RoomList,
        CreateRoom,
        CreateJoinAttempt,
    };

    Kind kind{Kind::Heartbeat};
    AsyncHeartbeatRequest heartbeat;
    AsyncRoomListRequest room_list;
    AsyncCreateRoomRequest create_room;
    AsyncCreateJoinAttemptRequest create_join_attempt;
};

} // namespace

struct AsyncMatchmakingClient::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<AsyncMatchmakingWorkItem> requests;
    std::vector<AsyncHeartbeatResult> heartbeat_results;
    std::vector<AsyncRoomListResult> room_list_results;
    std::vector<AsyncCreateRoomResult> create_room_results;
    std::vector<AsyncCreateJoinAttemptResult> create_join_attempt_results;
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

AsyncRoomListResult process_room_list(RoomServerMatchmaking& matchmaking,
                                      const AsyncRoomListRequest& request) {
    AsyncRoomListResult result;
    result.request_id = request.request_id;
    result.ok = matchmaking.list_rooms(request.server_url, result.rooms, result.err);
    return result;
}

AsyncCreateRoomResult process_create_room(RoomServerMatchmaking& matchmaking,
                                          const AsyncCreateRoomRequest& request) {
    AsyncCreateRoomResult result;
    result.request_id = request.request_id;
    result.ok = matchmaking.create_room(request.server_url, request.room, result.create, result.err);
    if (!result.ok)
        return result;
    result.has_room = matchmaking.fetch_room(request.server_url,
                                            result.create.room_code,
                                            result.room,
                                            result.err);
    return result;
}

AsyncCreateJoinAttemptResult process_create_join_attempt(
    RoomServerMatchmaking& matchmaking, const AsyncCreateJoinAttemptRequest& request) {
    AsyncCreateJoinAttemptResult result;
    result.request_id = request.request_id;
    result.ok = matchmaking.create_join_attempt(request.server_url,
                                                request.room_code,
                                                request.display_name,
                                                result.join_attempt,
                                                result.err);
    return result;
}

} // namespace

AsyncMatchmakingClient::AsyncMatchmakingClient() : impl_(std::make_unique<Impl>()) {
    impl_->worker = std::thread([impl = impl_.get()]() {
        while (true) {
            AsyncMatchmakingWorkItem request;
            {
                std::unique_lock<std::mutex> lock(impl->mutex);
                impl->cv.wait(lock, [&]() {
                    return impl->stopping || !impl->requests.empty();
                });
                if (impl->stopping)
                    break;
                request = std::move(impl->requests.front());
                impl->requests.pop_front();
            }

            if (request.kind == AsyncMatchmakingWorkItem::Kind::Heartbeat) {
                AsyncHeartbeatResult result = process_heartbeat(impl->matchmaking,
                                                                request.heartbeat);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->heartbeat_results.push_back(std::move(result));
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::RoomList) {
                AsyncRoomListResult result = process_room_list(impl->matchmaking,
                                                               request.room_list);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->room_list_results.push_back(std::move(result));
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::CreateRoom) {
                AsyncCreateRoomResult result = process_create_room(impl->matchmaking,
                                                                   request.create_room);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->create_room_results.push_back(std::move(result));
            } else {
                AsyncCreateJoinAttemptResult result =
                    process_create_join_attempt(impl->matchmaking, request.create_join_attempt);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->create_join_attempt_results.push_back(std::move(result));
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
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::Heartbeat;
        item.heartbeat = std::move(request);
        impl_->requests.push_back(std::move(item));
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

void AsyncMatchmakingClient::enqueue_room_list(AsyncRoomListRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::RoomList;
        item.room_list = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncRoomListResult> AsyncMatchmakingClient::drain_room_list_results() {
    std::vector<AsyncRoomListResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->room_list_results);
    return results;
}

void AsyncMatchmakingClient::enqueue_create_room(AsyncCreateRoomRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::CreateRoom;
        item.create_room = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncCreateRoomResult> AsyncMatchmakingClient::drain_create_room_results() {
    std::vector<AsyncCreateRoomResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->create_room_results);
    return results;
}

void AsyncMatchmakingClient::enqueue_create_join_attempt(
    AsyncCreateJoinAttemptRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::CreateJoinAttempt;
        item.create_join_attempt = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncCreateJoinAttemptResult>
AsyncMatchmakingClient::drain_create_join_attempt_results() {
    std::vector<AsyncCreateJoinAttemptResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->create_join_attempt_results);
    return results;
}

void AsyncMatchmakingClient::shutdown() {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stopping = true;
        impl_->requests.clear();
    }
    impl_->cv.notify_all();
    if (impl_->worker.joinable())
        impl_->worker.join();
}
