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
    const float throttle,
    const float brake,
    float delta_seconds,
    const std::uint32_t target_laps,
    const bool reverse) noexcept
{
    RouteMotionResult result{};
    if (route.size() < 2U || state.route_segment >= route.size() ||
        state.race.finished || !std::isfinite(delta_seconds)) {
        result.finished = state.race.finished;
        return result;
    }
    // Native 0x425920 passes elapsed time to 0x43B290 clamped to [0.01, 0.25].
    // Keep the live route-driven prototype on that recovered frame cadence
    // until its physical update replaces this provisional integrator.
    delta_seconds = std::clamp(delta_seconds, 0.01F, 0.25F);
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
    // This adapter is not the native tire/body simulation. Preserve the
    // recovered ramped control levels at this boundary, including the native
    // 0.2 activity gates. The route model's force magnitudes remain provisional.
    // Braking reduces speed magnitude; Reverse independently requests negative
    // drive when the brake gate is inactive.
    const float throttle_level = std::isfinite(throttle)
        ? std::clamp(throttle, 0.0F, 1.0F) : 0.0F;
    const float brake_level = std::isfinite(brake)
        ? std::clamp(brake, 0.0F, 1.0F) : 0.0F;
    if (brake_level > 0.2F) {
        const float braking_delta = 24.0F * brake_level * delta_seconds;
        if (state.speed > 0.0F)
            state.speed = std::max(0.0F, state.speed - braking_delta);
        else if (state.speed < 0.0F)
            state.speed = std::min(0.0F, state.speed + braking_delta);
    } else {
        const float drive = (throttle_level > 0.2F ? throttle_level : 0.0F) -
            static_cast<float>(reverse);
        state.speed += drive * 14.0F * delta_seconds;
    }
    state.speed = std::clamp(state.speed *
        std::pow(0.992F, delta_seconds * 60.0F), -8.0F, 38.0F);
    float distance = state.speed * delta_seconds;
    float route_distance_delta = 0.0F;
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
            route_distance_delta += distance;
            state.route_fraction += distance / length;
            distance = 0.0F;
        } else {
            const bool forward = distance > 0.0F;
            route_distance_delta += forward ? remaining : -remaining;
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
    state.route_distance_travelled += route_distance_delta;
    const bool lap_completed = result.lap_completed;
    result = project_route_motion_state(state, route);
    result.lap_completed = lap_completed;
    return result;
}

RouteMotionResult project_route_motion_state(
    const RouteMotionState& state,
    const std::span<const std::array<float, 2>> route) noexcept
{
    RouteMotionResult result{};
    result.finished = state.race.finished;
    if (route.size() < 2U || state.route_segment >= route.size() ||
        !std::isfinite(state.route_fraction) || state.route_fraction < 0.0F ||
        state.route_fraction > 1.0F || !std::isfinite(state.steering) ||
        !std::isfinite(state.speed) ||
        !std::isfinite(state.route_distance_travelled)) return result;

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
    return result;
}

bool commit_route_motion_after_terrain_query(
    RouteMotionState& state,
    const RouteMotionState& candidate,
    const bool terrain_query_succeeded) noexcept
{
    if (!terrain_query_succeeded) return false;
    state = candidate;
    return true;
}

std::vector<RouteRaceStanding> build_route_race_standings(
    const RouteMotionState& player_state,
    const std::span<const RouteMotionState> participants,
    const std::size_t player_participant_index)
{
    std::vector<RouteRaceStanding> standings;
    standings.reserve(participants.size());
    if (player_state.race.finished &&
        std::isfinite(player_state.race.elapsed_seconds) &&
        player_state.race.elapsed_seconds >= 0.0F) {
        standings.push_back({player_participant_index,
            player_state.race.elapsed_seconds, true});
    }
    for (std::size_t index = 0; index < participants.size(); ++index) {
        if (index == player_participant_index) continue;
        const auto& state = participants[index];
        if (!state.race.finished || !std::isfinite(state.race.elapsed_seconds) ||
            state.race.elapsed_seconds < 0.0F) continue;
        standings.push_back({index, state.race.elapsed_seconds, false});
    }
    std::stable_sort(standings.begin(), standings.end(),
        [](const RouteRaceStanding& left, const RouteRaceStanding& right) {
            if (left.finish_time_seconds != right.finish_time_seconds)
                return left.finish_time_seconds < right.finish_time_seconds;
            return left.participant_index < right.participant_index;
        });
    return standings;
}

std::vector<RouteRacePosition> build_route_race_positions(
    const RouteMotionState& player_state,
    const std::span<const RouteMotionState> participants,
    const std::size_t player_participant_index)
{
    std::vector<RouteRacePosition> positions;
    positions.reserve(participants.size());
    const auto append = [&positions](const RouteMotionState& state,
                                     const std::size_t index,
                                     const bool player) {
        if (!std::isfinite(state.route_distance_travelled)) return;
        if (state.race.finished &&
            (!std::isfinite(state.race.elapsed_seconds) ||
             state.race.elapsed_seconds < 0.0F)) return;
        positions.push_back({index, state.route_distance_travelled,
            state.race.elapsed_seconds, state.race.finished, player});
    };
    append(player_state, player_participant_index, true);
    for (std::size_t index = 0; index < participants.size(); ++index) {
        if (index == player_participant_index) continue;
        append(participants[index], index, false);
    }
    std::stable_sort(positions.begin(), positions.end(),
        [](const RouteRacePosition& left, const RouteRacePosition& right) {
            if (left.finished != right.finished) return left.finished;
            if (left.finished &&
                left.finish_time_seconds != right.finish_time_seconds) {
                return left.finish_time_seconds < right.finish_time_seconds;
            }
            if (!left.finished &&
                left.route_distance_travelled != right.route_distance_travelled) {
                return left.route_distance_travelled >
                    right.route_distance_travelled;
            }
            return left.participant_index < right.participant_index;
        });
    return positions;
}

} // namespace viper
