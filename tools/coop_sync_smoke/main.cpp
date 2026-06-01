#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "src/matchmaking.hpp"
#include "src/net_transport.hpp"
#include "src/room_matchmaking.hpp"
#include "src/session_contract.hpp"
#include "src/sync_payload_codec.hpp"
#include "src/sync_transport_udp.hpp"
#include "demo/actions.hpp"
#include "demo/coop_protocol.hpp"
#include "demo/coop_sim.hpp"
#include "demo/settings.hpp"
#include "demo/state.hpp"

namespace {

void set_down(InputFrame& frame, int action) {
    frame.down_bits |= (1u << action);
}

InputFrame scripted_host_input(int frame_index) {
    InputFrame frame;
    if (frame_index < 36)
        set_down(frame, GameAction::RIGHT);
    if (frame_index == 40)
        set_down(frame, GameAction::USE);
    return frame;
}

InputFrame scripted_guest_input(int frame_index) {
    InputFrame frame;
    if (frame_index < 20)
        set_down(frame, GameAction::RIGHT);
    if (frame_index >= 20 && frame_index < 40)
        set_down(frame, GameAction::DOWN);
    if (frame_index == 18)
        set_down(frame, GameAction::USE);
    return frame;
}

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <typename PollFn>
bool wait_until(double timeout_sec, PollFn&& poll_fn) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(timeout_sec));
    while (std::chrono::steady_clock::now() < deadline) {
        if (poll_fn())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return poll_fn();
}

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

void compare_states(const State& host,
                    const State& client,
                    const std::vector<std::string>& host_ids,
                    const std::vector<std::string>& client_ids) {
    require(host.players.size() == client.players.size(), "player count mismatch");
    require(host_ids == client_ids, "member order mismatch");
    for (std::size_t i = 0; i < host.players.size(); ++i) {
        require(nearly_equal(host.players[i].pos.x, client.players[i].pos.x), "player x mismatch");
        require(nearly_equal(host.players[i].pos.y, client.players[i].pos.y), "player y mismatch");
    }
    require(nearly_equal(host.bonk.cooldown, client.bonk.cooldown), "bonk cooldown mismatch");
    require(nearly_equal(host.bar_height, client.bar_height), "bar height mismatch");
}

} // namespace

int main(int argc, char** argv) {
    std::string server_url = "http://127.0.0.1:8788";
    if (argc > 1)
        server_url = argv[1];

    UdpSyncNetTransport host_transport;
    UdpSyncNetTransport guest_transport;

    try {
        RoomServerMatchmaking matchmaking;
        std::string err;
        MatchmakingRoom room;
        room.session_name = "Coop Smoke";
        room.host_name = "Host";
        room.privacy = 2;
        room.max_players = 4;
        room.contract.game_version = "0.1.0";
        room.contract.net_protocol = session_contract_default_net_protocol();
        room.contract.mod_hash = "smoke";
        room.contract.required_mod_ids = {"base", "smoke"};
        room.contract.session_phase = "in_game";

        MatchmakingCreateResult created;
        require(matchmaking.create_room(server_url, room, created, err), err);
        const std::string room_code = created.room_code;
        const std::string host_secret = created.host_secret;
        const std::string host_member_id = created.member_id;

        std::string guest_member_id;
        require(matchmaking.join_room(server_url, room_code, "Guest", "", guest_member_id, err), err);

        require(host_transport.ensure_host(room_code, err), err);
        room.room_code = room_code;
        room.contract.realtime_endpoint = host_transport.public_endpoint();
        require(matchmaking.heartbeat_room(server_url,
                                           room_code,
                                           host_member_id,
                                           "Host",
                                           host_secret,
                                           &room,
                                           err),
                err);

        MatchmakingRoom fetched_room;
        require(matchmaking.fetch_room(server_url, room_code, fetched_room, err), err);
        require(!fetched_room.contract.realtime_endpoint.empty(), "missing realtime endpoint");
        require(guest_transport.ensure_client(room_code, fetched_room.contract.realtime_endpoint, err), err);

        State host_state;
        State guest_state;
        std::vector<std::string> host_member_ids{host_member_id, guest_member_id};
        std::vector<std::string> guest_member_ids;
        std::vector<InputFrame> host_current(2);
        std::vector<InputFrame> host_previous(2);
        InputFrame latest_guest_input;
        std::uint64_t latest_guest_seq = 0;

        ensure_demo_player_count(host_state, host_member_ids.size());

        for (int frame_index = 0; frame_index < 90; ++frame_index) {
            const InputFrame host_input = scripted_host_input(frame_index);
            const InputFrame guest_input = scripted_guest_input(frame_index);

            NetTransportPacket guest_packet;
            guest_packet.kind = NetPacketKind::Input;
            guest_packet.room_code = room_code;
            guest_packet.member_id = guest_member_id;
            guest_packet.seq = static_cast<std::uint64_t>(frame_index + 1);
            require(sync_payload_encode_json(input_frame_to_json(guest_input), guest_packet.payload, err), err);
            require(guest_transport.send(guest_packet, err), err);

            std::vector<NetTransportPacket> incoming_inputs;
            bool host_has_guest = false;
            const bool got_guest_input = wait_until(1.0, [&]() {
                require(host_transport.poll(incoming_inputs, err), err);
                for (const NetTransportPacket& entry : incoming_inputs) {
                    if (entry.kind != NetPacketKind::Input || entry.member_id != guest_member_id)
                        continue;
                    nlohmann::json input_json;
                    require(sync_payload_decode_json(entry.payload, input_json, err), err);
                    require(input_frame_from_json(input_json, latest_guest_input), "failed to decode guest input");
                    latest_guest_seq = entry.seq;
                    host_has_guest = true;
                }
                return host_has_guest;
            });
            require(got_guest_input, "missing guest input");

            host_current[0] = host_input;
            host_current[1] = latest_guest_input;
            ensure_demo_player_count(host_state, host_member_ids.size());
            simulate_demo_world(host_state, host_current, host_previous, FIXED_TIMESTEP);

            nlohmann::json acked_inputs{
                {host_member_id, static_cast<std::uint64_t>(frame_index + 1)},
                {guest_member_id, latest_guest_seq},
            };
            NetTransportPacket snapshot_packet;
            snapshot_packet.kind = NetPacketKind::Snapshot;
            snapshot_packet.room_code = room_code;
            require(sync_payload_encode_json(nlohmann::json{
                {"sim_frame", static_cast<std::uint64_t>(frame_index + 1)},
                {"driver_snapshot",
                 coop_snapshot_to_json(capture_coop_snapshot(host_state,
                                                             host_member_ids,
                                                             static_cast<std::uint64_t>(frame_index + 1)))},
                {"acked_inputs", std::move(acked_inputs)},
            }, snapshot_packet.payload, err), err);
            require(host_transport.send(snapshot_packet, err), err);

            NetTransportPacket latest_snapshot;
            bool has_snapshot = false;
            const bool got_snapshot = wait_until(1.0, [&]() {
                std::vector<NetTransportPacket> polled;
                std::vector<NetTransportPacket> host_flush;
                require(host_transport.poll(host_flush, err), err);
                require(guest_transport.poll(polled, err), err);
                for (const NetTransportPacket& packet : polled) {
                    if (packet.kind != NetPacketKind::Snapshot)
                        continue;
                    latest_snapshot = packet;
                    has_snapshot = true;
                }
                return has_snapshot;
            });
            require(got_snapshot, "missing guest snapshot");

            nlohmann::json snapshot_json;
            require(sync_payload_decode_json(latest_snapshot.payload, snapshot_json, err), err);
            CoopStateSnapshot snapshot;
            require(coop_snapshot_from_json(snapshot_json.at("driver_snapshot"), snapshot),
                    "failed to decode snapshot");
            apply_coop_snapshot(snapshot, guest_state, guest_member_ids);
            compare_states(host_state, guest_state, host_member_ids, guest_member_ids);
            host_previous = host_current;
        }

        host_transport.reset();
        guest_transport.reset();

        require(matchmaking.leave_room(server_url, room_code, guest_member_id, {}, err), err);
        require(matchmaking.leave_room(server_url, room_code, host_member_id, host_secret, err), err);

        std::cout << "[coop_sync_smoke] ok\n";
        return 0;
    } catch (const std::exception& e) {
        host_transport.reset();
        guest_transport.reset();
        std::cerr << "[coop_sync_smoke] " << e.what() << "\n";
        return 1;
    }
}
