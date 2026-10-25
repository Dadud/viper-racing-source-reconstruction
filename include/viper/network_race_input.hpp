#pragma once

#include "viper/network_session.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

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

// Reconstructed-client race agreement. Resource names and the selected
// single-race parameters must match before either client enters the race.
struct RaceConfiguration {
    std::string track;
    std::string car;
    std::uint16_t laps{};
    std::uint8_t opponent_strength{};
    std::uint8_t entrant_count{};
    bool operator==(const RaceConfiguration&) const = default;
};

std::vector<std::uint8_t> encode_race_configuration(
    const RaceConfiguration& configuration);
bool decode_race_configuration(std::span<const std::uint8_t> bytes,
                               RaceConfiguration& configuration);

} // namespace viper::net
