#pragma once

#include "viper/race_progression.hpp"
#include "viper/network_session.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace viper::net {

inline constexpr std::size_t race_state_payload_size = 40U;

// Complete route-runtime state for one reconstructed peer. This is the
// existing route-motion prototype state, not native rigid-body physics.
std::array<std::uint8_t, race_state_payload_size> encode_race_state(
    const RouteMotionState& state);
bool decode_race_state(std::span<const std::uint8_t> bytes,
                       RouteMotionState& state) noexcept;

// Validates monotonic runtime state against the preceding peer snapshot.
// Forward tick gaps are allowed; a repeated tick must carry identical state.
bool valid_race_state_update(const RouteMotionState& previous,
                             const RouteMotionState& current,
                             std::uint32_t previous_tick,
                             std::uint32_t current_tick) noexcept;

// Reconstructed-client race agreement. Resource names and the selected
// single-race parameters must match before either client enters the race.
struct RaceConfiguration {
    std::string track;
    std::string car;
    std::uint16_t laps{};
    std::uint8_t opponent_strength{};
    std::uint8_t entrant_count{};
    std::uint8_t player_slot{};
    bool operator==(const RaceConfiguration&) const = default;
};

// Peers may choose different player slots, but all shared race settings must
// match and each peer must own a distinct entrant.
bool compatible_race_configurations(const RaceConfiguration& local,
                                    const RaceConfiguration& remote) noexcept;

std::vector<std::uint8_t> encode_race_configuration(
    const RaceConfiguration& configuration);
bool decode_race_configuration(std::span<const std::uint8_t> bytes,
                               RaceConfiguration& configuration);

} // namespace viper::net
