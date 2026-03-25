#pragma once

#include <string>
#include <vector>

#include "engine/net_transport.hpp"

bool sync_transport_packet_serialize(const NetTransportPacket& packet,
                                     std::vector<std::uint8_t>& out,
                                     std::string& err);
bool sync_transport_packet_deserialize(const std::vector<std::uint8_t>& bytes,
                                       NetTransportPacket& out,
                                       std::string& err);
