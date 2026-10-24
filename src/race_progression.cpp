#include "viper/race_progression.hpp"

#include <algorithm>
#include <cmath>

namespace viper {

void initialize_race_progression(
    RaceProgressionState& state, const std::size_t starting_segment) noexcept
{
    state = {};
    state.lap_start_segment = starting_segment;
}

void advance_race_clock(
    RaceProgressionState& state, const float delta_seconds) noexcept
{
    if (state.finished || !std::isfinite(delta_seconds) || delta_seconds <= 0.0F)
        return;
    state.elapsed_seconds += delta_seconds;
}

bool observe_race_route_transition(
    RaceProgressionState& state,
    const std::size_t next_segment,
    const bool forward,
    const std::uint32_t target_laps) noexcept
{
    if (state.finished || !forward || next_segment != state.lap_start_segment)
        return false;
    ++state.completed_laps;
    state.finished = state.completed_laps >= std::max(1U, target_laps);
    return true;
}

} // namespace viper
