#include "viper/legacy_timer.hpp"

namespace viper {

LegacyTimerIteration schedule_legacy_timer_iteration(
    LegacyTimerWorkerState& state,
    const bool stop_requested,
    const std::int32_t elapsed_ms) noexcept
{
    if (stop_requested) {
        return {false, true, 0U, false, false};
    }

    if (elapsed_ms < state.interval_ms) {
        const auto remaining = static_cast<std::uint32_t>(state.interval_ms)
            - static_cast<std::uint32_t>(elapsed_ms);
        return {true, false, remaining, false, false};
    }

    ++state.overruns_since_backoff;
    if (state.overruns_since_backoff < 60U) {
        return {true, false, 0U, true, false};
    }

    state.overruns_since_backoff = 0U;
    ++state.overrun_groups;
    return {true, false, 20U, false, true};
}

} // namespace viper
