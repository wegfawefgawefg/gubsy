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
        FetchRoom,
        CreateRoom,
        CreateJoinAttempt,
        JoinRoom,
        LeaveRoom,
        RemoveMember,
    };

    Kind kind{Kind::Heartbeat};
    AsyncHeartbeatRequest heartbeat;
    AsyncRoomListRequest room_list;
    AsyncFetchRoomRequest fetch_room;
    AsyncCreateRoomRequest create_room;
    AsyncCreateJoinAttemptRequest create_join_attempt;
    AsyncJoinRoomRequest join_room;
    AsyncLeaveRoomRequest leave_room;
    AsyncRemoveMemberRequest remove_member;
};

} // namespace

struct AsyncMatchmakingClient::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<AsyncMatchmakingWorkItem> requests;
    std::vector<AsyncHeartbeatResult> heartbeat_results;
    std::vector<AsyncRoomListResult> room_list_results;
    std::vector<AsyncFetchRoomResult> fetch_room_results;
    std::vector<AsyncCreateRoomResult> create_room_results;
    std::vector<AsyncCreateJoinAttemptResult> create_join_attempt_results;
    std::vector<AsyncJoinRoomResult> join_room_results;
    std::vector<AsyncLeaveRoomResult> leave_room_results;
    std::vector<AsyncRemoveMemberResult> remove_member_results;
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

AsyncFetchRoomResult process_fetch_room(RoomServerMatchmaking& matchmaking,
                                        const AsyncFetchRoomRequest& request) {
    AsyncFetchRoomResult result;
    result.request_id = request.request_id;
    result.ok = matchmaking.fetch_room(request.server_url,
                                       request.room_code,
                                       result.room,
                                       result.err);
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

AsyncJoinRoomResult process_join_room(RoomServerMatchmaking& matchmaking,
                                      const AsyncJoinRoomRequest& request) {
    AsyncJoinRoomResult result;
    result.request_id = request.request_id;
    result.ok = matchmaking.join_room(request.server_url,
                                      request.room_code,
                                      request.display_name,
                                      request.join_token,
                                      result.member_id,
                                      result.err);
    if (!result.ok)
        return result;
    result.has_room = matchmaking.fetch_room(request.server_url,
                                            request.room_code,
                                            result.room,
                                            result.err);
    return result;
}

AsyncLeaveRoomResult process_leave_room(RoomServerMatchmaking& matchmaking,
                                        const AsyncLeaveRoomRequest& request) {
    AsyncLeaveRoomResult result;
    result.request_id = request.request_id;
    result.ok = matchmaking.leave_room(request.server_url,
                                       request.room_code,
                                       request.member_id,
                                       request.host_secret,
                                       result.err);
    return result;
}

AsyncRemoveMemberResult process_remove_member(RoomServerMatchmaking& matchmaking,
                                              const AsyncRemoveMemberRequest& request) {
    AsyncRemoveMemberResult result;
    result.request_id = request.request_id;
    result.target_name = request.target_name;
    result.ok = matchmaking.remove_member(request.server_url,
                                          request.room_code,
                                          request.host_secret,
                                          request.target_member_id,
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
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::FetchRoom) {
                AsyncFetchRoomResult result = process_fetch_room(impl->matchmaking,
                                                                 request.fetch_room);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->fetch_room_results.push_back(std::move(result));
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::CreateRoom) {
                AsyncCreateRoomResult result = process_create_room(impl->matchmaking,
                                                                   request.create_room);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->create_room_results.push_back(std::move(result));
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::CreateJoinAttempt) {
                AsyncCreateJoinAttemptResult result =
                    process_create_join_attempt(impl->matchmaking, request.create_join_attempt);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->create_join_attempt_results.push_back(std::move(result));
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::JoinRoom) {
                AsyncJoinRoomResult result = process_join_room(impl->matchmaking,
                                                               request.join_room);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->join_room_results.push_back(std::move(result));
            } else if (request.kind == AsyncMatchmakingWorkItem::Kind::LeaveRoom) {
                AsyncLeaveRoomResult result = process_leave_room(impl->matchmaking,
                                                                 request.leave_room);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->leave_room_results.push_back(std::move(result));
            } else {
                AsyncRemoveMemberResult result = process_remove_member(impl->matchmaking,
                                                                       request.remove_member);
                std::lock_guard<std::mutex> lock(impl->mutex);
                impl->remove_member_results.push_back(std::move(result));
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

void AsyncMatchmakingClient::enqueue_fetch_room(AsyncFetchRoomRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::FetchRoom;
        item.fetch_room = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncFetchRoomResult> AsyncMatchmakingClient::drain_fetch_room_results() {
    std::vector<AsyncFetchRoomResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->fetch_room_results);
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

void AsyncMatchmakingClient::enqueue_join_room(AsyncJoinRoomRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::JoinRoom;
        item.join_room = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncJoinRoomResult> AsyncMatchmakingClient::drain_join_room_results() {
    std::vector<AsyncJoinRoomResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->join_room_results);
    return results;
}

void AsyncMatchmakingClient::enqueue_leave_room(AsyncLeaveRoomRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::LeaveRoom;
        item.leave_room = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncLeaveRoomResult> AsyncMatchmakingClient::drain_leave_room_results() {
    std::vector<AsyncLeaveRoomResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->leave_room_results);
    return results;
}

void AsyncMatchmakingClient::enqueue_remove_member(AsyncRemoveMemberRequest request) {
    if (!impl_)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->stopping)
            return;
        AsyncMatchmakingWorkItem item;
        item.kind = AsyncMatchmakingWorkItem::Kind::RemoveMember;
        item.remove_member = std::move(request);
        impl_->requests.push_back(std::move(item));
    }
    impl_->cv.notify_one();
}

std::vector<AsyncRemoveMemberResult> AsyncMatchmakingClient::drain_remove_member_results() {
    std::vector<AsyncRemoveMemberResult> results;
    if (!impl_)
        return results;
    std::lock_guard<std::mutex> lock(impl_->mutex);
    results.swap(impl_->remove_member_results);
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
