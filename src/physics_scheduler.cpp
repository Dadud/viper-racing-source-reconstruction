#include "viper/physics_scheduler.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace viper {
namespace {

std::int32_t wrapping_clock_difference(const std::int32_t now,
                                       const std::int32_t then) noexcept
{
    const auto now_bits = std::bit_cast<std::uint32_t>(now);
    const auto then_bits = std::bit_cast<std::uint32_t>(then);
    return std::bit_cast<std::int32_t>(now_bits - then_bits);
}

std::int32_t to_32_bit_result(const double rounded) noexcept
{
    if (!std::isfinite(rounded) || rounded < -0x1p63 || rounded >= 0x1p63) {
        return 0;
    }
    const auto wide = static_cast<std::int64_t>(rounded);
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(wide));
}

} // namespace

PhysicsStepSchedule schedule_physics_steps(
    PhysicsStepAccumulator& accumulator,
    const PhysicsStepSchedulerInput& input) noexcept
{
    PhysicsStepSchedule result{};
    if (input.scheduler_mode == 0) {
        result.steps = 24 / input.clock_divisor;
        const auto difference = static_cast<std::uint8_t>(
            input.simulation_tick_low_byte - static_cast<std::uint8_t>(result.steps));
        if (input.simulation_tick_low_byte < 0x80U && difference > 0x80U) {
            result.sleep_ms = 20U;
        }
    } else if (input.scheduler_mode == 2) {
        const auto previous = accumulator.mode_two_toggle_0x08;
        result.steps = previous;
        accumulator.mode_two_toggle_0x08 = previous == 0U ? 1U : 0U;
    }

    const bool use_elapsed_schedule = input.scheduler_mode == 1
        && !input.gate_0x508a40
        && !input.gate_0x508760;
    if (!use_elapsed_schedule) {
        return result;
    }

    if (accumulator.recapture_anchor_0x09) {
        accumulator.simulation_anchor_0x00 = input.task_simulation_time;
        accumulator.clock_anchor_0x04 = input.clock_now;
        accumulator.recapture_anchor_0x09 = false;
        result.anchor_recaptured = true;
        return result;
    }

    const auto clock_delta = wrapping_clock_difference(
        input.clock_now, accumulator.clock_anchor_0x04);
    constexpr float milliseconds_to_seconds = std::bit_cast<float>(
        std::uint32_t{0x3a83126fU});
    const float estimated_simulation_time = static_cast<float>(
        static_cast<double>(clock_delta) * milliseconds_to_seconds
        + accumulator.simulation_anchor_0x00);
    const double elapsed = static_cast<double>(estimated_simulation_time)
        - input.task_simulation_time;
    constexpr float step_bias = std::bit_cast<float>(std::uint32_t{0x3c83126fU});
    constexpr float steps_per_second = std::bit_cast<float>(
        std::uint32_t{0x4279ffffU});
    const double rounded_steps = std::nearbyint(
        (elapsed + static_cast<double>(step_bias))
        * static_cast<double>(steps_per_second));
    result.steps = to_32_bit_result(rounded_steps);

    if (input.cap_to_four) {
        if (result.steps > 4) {
            result.steps = 4;
            result.catchup_limited = true;
        }
    } else if (result.steps < 0 || result.steps > 2) {
        accumulator.recapture_anchor_0x09 = true;
        result.steps = 2;
        result.catchup_limited = true;
    }
    return result;
}

} // namespace viper
