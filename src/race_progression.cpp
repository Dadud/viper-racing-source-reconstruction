#include "viper/race_progression.hpp"

#include "viper/race_loop_callbacks.hpp"

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

RouteMotionResult advance_route_motion(
    RouteMotionState& state,
    const std::span<const std::array<float, 2>> route,
    const bool steer_left,
    const bool steer_right,
    const bool accelerate,
    const bool brake,
    float delta_seconds,
    const std::uint32_t target_laps) noexcept
{
    RouteMotionResult result{};
    if (route.size() < 2U || state.route_segment >= route.size() ||
        state.race.finished || !std::isfinite(delta_seconds)) {
        result.finished = state.race.finished;
        return result;
    }
    delta_seconds = std::clamp(delta_seconds, 0.0F, 0.05F);
    advance_race_clock(state.race, delta_seconds);
    // Race setup 0x43AF60 resets +0x509358 to zero and initializes the
    // steering smoothing, gain, and response globals (+0x509428,
    // +0x509390, +0x5093A8) to 1.0. The keyboard controls are digital inputs,
    // corresponding to the native type-1 ramped binding path.
    RaceLoopSteeringState steering_state{state.steering};
    update_race_loop_steering(steering_state, {
        delta_seconds,
        1.0F,
        1.0F,
        1.0F,
        static_cast<float>(steer_left),
        static_cast<float>(steer_right),
        false,
        true,
        true,
        steer_left,
        steer_right,
    });
    state.steering = steering_state.value_0x509358;
    const float drive = static_cast<float>(accelerate) - static_cast<float>(brake);
    state.speed = std::clamp((state.speed + drive * 14.0F * delta_seconds) *
        std::pow(0.992F, delta_seconds * 60.0F), -8.0F, 38.0F);
    float distance = state.speed * delta_seconds;
    for (unsigned guard = 0U; std::abs(distance) > 1.0e-5F && guard < 128U; ++guard) {
        const auto& a = route[state.route_segment];
        const auto& b = route[(state.route_segment + 1U) % route.size()];
        const float dx = b[0] - a[0];
        const float dz = b[1] - a[1];
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!(length > 1.0e-5F)) {
            state.route_segment = (state.route_segment + 1U) % route.size();
            state.route_fraction = 0.0F;
            continue;
        }
        const float remaining = (distance > 0.0F
            ? 1.0F - state.route_fraction : state.route_fraction) * length;
        if (std::abs(distance) < remaining) {
            state.route_fraction += distance / length;
            distance = 0.0F;
        } else {
            const bool forward = distance > 0.0F;
            distance += distance > 0.0F ? -remaining : remaining;
            state.route_segment = (state.route_segment +
                (forward ? 1U : route.size() - 1U)) % route.size();
            state.route_fraction = forward ? 0.0F : 1.0F;
            result.lap_completed = observe_race_route_transition(state.race,
                state.route_segment, forward, target_laps) || result.lap_completed;
            if (state.race.finished) {
                state.speed = 0.0F;
                distance = 0.0F;
            }
        }
    }
    const auto& a = route[state.route_segment];
    const auto& b = route[(state.route_segment + 1U) % route.size()];
    const float dx = b[0] - a[0];
    const float dz = b[1] - a[1];
    const float tangent = std::atan2(dx, dz);
    const float route_x = a[0] + dx * state.route_fraction;
    const float route_z = a[1] + dz * state.route_fraction;
    const float lateral = state.steering * state.speed * std::abs(state.speed) * 0.0008F;
    result.x = route_x + std::cos(tangent) * lateral;
    result.z = route_z - std::sin(tangent) * lateral;
    result.yaw = tangent + state.steering * 0.35F;
    result.valid = std::isfinite(result.x) && std::isfinite(result.z) &&
        std::isfinite(result.yaw);
    result.finished = state.race.finished;
    return result;
}

} // namespace viper
