#include "ginput/types.hpp"

#include <cstdint>

namespace ginput {
namespace {

constexpr std::uint32_t extended_flag = 1u << 29u;
constexpr int kind_shift = 26;
constexpr int device_shift = 16;
constexpr std::uint32_t kind_mask = 0x7u;
constexpr std::uint32_t device_mask = 0x3ffu;
constexpr std::uint32_t code_mask = 0xffffu;
constexpr int any_device_encoded = static_cast<int>(device_mask);

std::uint32_t encode_device_id(int device_id) {
    if (device_id == any_device_id) {
        return device_mask;
    }
    return static_cast<std::uint32_t>(device_id) & device_mask;
}

int decode_device_id(std::uint32_t encoded_id) {
    if (encoded_id == static_cast<std::uint32_t>(any_device_encoded)) {
        return any_device_id;
    }
    return static_cast<int>(encoded_id);
}

bool has_extended_flag(EncodedControl encoded) {
    return (static_cast<std::uint32_t>(encoded) & extended_flag) != 0u;
}

EncodedControl encode_parts(DeviceKind kind, int device_id, std::uint32_t code) {
    const std::uint32_t raw = extended_flag |
                              ((static_cast<std::uint32_t>(kind) & kind_mask) << kind_shift) |
                              (encode_device_id(device_id) << device_shift) | (code & code_mask);
    return static_cast<EncodedControl>(raw);
}

} // namespace

EncodedControl encode_button(DeviceButton button) {
    return encode_parts(button.kind, button.device_id, static_cast<std::uint32_t>(button.code));
}

EncodedControl encode_axis_1d(DeviceAxis1D axis) {
    return encode_parts(axis.kind, axis.device_id, static_cast<std::uint32_t>(axis.code));
}

EncodedControl encode_axis_2d(DeviceAxis2D axis) {
    const std::uint32_t packed = ((static_cast<std::uint32_t>(axis.y_code) & 0xffu) << 8u) |
                                 (static_cast<std::uint32_t>(axis.x_code) & 0xffu);
    return encode_parts(axis.kind, axis.device_id, packed);
}

bool decode_button(EncodedControl encoded, DeviceButton& out) {
    if (!has_extended_flag(encoded)) {
        return false;
    }
    const std::uint32_t raw = static_cast<std::uint32_t>(encoded);
    out.kind = static_cast<DeviceKind>((raw >> kind_shift) & kind_mask);
    out.device_id = decode_device_id((raw >> device_shift) & device_mask);
    out.code = static_cast<int>(raw & code_mask);
    return true;
}

bool decode_axis_1d(EncodedControl encoded, DeviceAxis1D& out) {
    if (!has_extended_flag(encoded)) {
        return false;
    }
    const std::uint32_t raw = static_cast<std::uint32_t>(encoded);
    out.kind = static_cast<DeviceKind>((raw >> kind_shift) & kind_mask);
    out.device_id = decode_device_id((raw >> device_shift) & device_mask);
    out.code = static_cast<int>(raw & code_mask);
    return true;
}

bool decode_axis_2d(EncodedControl encoded, DeviceAxis2D& out) {
    if (!has_extended_flag(encoded)) {
        return false;
    }
    const std::uint32_t raw = static_cast<std::uint32_t>(encoded);
    const std::uint32_t packed = raw & code_mask;
    out.kind = static_cast<DeviceKind>((raw >> kind_shift) & kind_mask);
    out.device_id = decode_device_id((raw >> device_shift) & device_mask);
    out.x_code = static_cast<int>(packed & 0xffu);
    out.y_code = static_cast<int>((packed >> 8u) & 0xffu);
    return true;
}

} // namespace ginput
