#pragma once

#include <cstddef>
#include <cstdint>

namespace viper {

struct RaceProgressionState {
    std::size_t lap_start_segment{};
    std::uint32_t completed_laps{};
    float elapsed_seconds{};
    bool finished{};
};

void initialize_race_progression(
    RaceProgressionState& state, std::size_t starting_segment) noexcept;

void advance_race_clock(
    RaceProgressionState& state, float delta_seconds) noexcept;

// Counts only forward crossings of the lap's starting route segment.
// target_laps is clamped to at least one.
[[nodiscard]] bool observe_race_route_transition(
    RaceProgressionState& state,
    std::size_t next_segment,
    bool forward,
    std::uint32_t target_laps) noexcept;

} // namespace viper
