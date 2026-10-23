#pragma once

#include <cstdint>

namespace viper {

struct PhysicsStepAccumulator {
    float simulation_anchor_0x00{};
    std::int32_t clock_anchor_0x04{};
    std::uint8_t mode_two_toggle_0x08{};
    bool recapture_anchor_0x09{};
};

struct PhysicsStepSchedulerInput {
    int scheduler_mode{};
    bool gate_0x508a40{};
    bool gate_0x508760{};
    bool cap_to_four{};
    int clock_divisor{};
    std::uint8_t simulation_tick_low_byte{};
    std::int32_t clock_now{};
    float task_simulation_time{};
};

struct PhysicsStepSchedule {
    std::int32_t steps{1};
    std::uint32_t sleep_ms{};
    bool catchup_limited{};
    bool anchor_recaptured{};
};

// Models the state branches and catch-up caps in 0x422740. In mode 0,
// clock_divisor must be nonzero, matching the binary's integer divide.
[[nodiscard]] PhysicsStepSchedule schedule_physics_steps(
    PhysicsStepAccumulator& accumulator,
    const PhysicsStepSchedulerInput& input) noexcept;

} // namespace viper
