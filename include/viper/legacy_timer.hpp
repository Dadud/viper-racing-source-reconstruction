#pragma once

#include <cstdint>

namespace viper {

struct LegacyTimerWorkerState {
    std::int32_t interval_ms{};
    std::uint8_t overruns_since_backoff{};
    std::uint32_t overrun_groups{};
};

struct LegacyTimerIteration {
    bool invoke_callback{};
    bool unregister_device_timer{};
    std::uint32_t sleep_ms{};
    bool yield_only{};
    bool overrun_group_completed{};
};

// Models one post-callback scheduling decision in the worker at 0x414C30.
// elapsed_ms follows its signed 32-bit comparison against the interval.
[[nodiscard]] LegacyTimerIteration schedule_legacy_timer_iteration(
    LegacyTimerWorkerState& state,
    bool stop_requested,
    std::int32_t elapsed_ms) noexcept;

} // namespace viper
