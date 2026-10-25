#pragma once

#include "viper/network_session.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace viper::net {

// Reconstructed-client logical controls. Values are the route runtime's
// provisional normalized steering/throttle/brake outputs, not native packets.
struct RaceInput {
    float steering{};
    float throttle{};
    float brake{};
};

inline constexpr std::size_t race_input_payload_size = 12;

std::array<std::uint8_t, race_input_payload_size> encode_race_input(
    const RaceInput& input);
bool decode_race_input(std::span<const std::uint8_t> bytes,
                       RaceInput& input) noexcept;

} // namespace viper::net
