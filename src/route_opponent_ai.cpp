#include "viper/route_opponent_ai.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace viper {
namespace {

struct RoutePosition {
    float distance{};
    bool valid{};
};

bool route_lengths(
    const std::span<const std::array<float, 2>> route,
    float& total) noexcept
{
    if (route.size() < 2U) return false;
    total = 0.0F;
    for (std::size_t index = 0U; index < route.size(); ++index) {
        const auto& a = route[index];
        const auto& b = route[(index + 1U) % route.size()];
        if (!std::isfinite(a[0]) || !std::isfinite(a[1]) ||
            !std::isfinite(b[0]) || !std::isfinite(b[1])) return false;
        const float dx = b[0] - a[0];
        const float dz = b[1] - a[1];
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!std::isfinite(length) || length <= 1.0e-5F) return false;
        total += length;
    }
    return std::isfinite(total) && total > 0.0F;
}

RoutePosition position_on_route(
    const RouteMotionState& motion,
    const std::span<const std::array<float, 2>> route) noexcept
{
    if (motion.route_segment >= route.size() ||
        !std::isfinite(motion.route_fraction) ||
        motion.route_fraction < 0.0F || motion.route_fraction > 1.0F)
        return {};
    float distance = 0.0F;
    for (std::size_t index = 0U; index <= motion.route_segment; ++index) {
        const auto& a = route[index];
        const auto& b = route[(index + 1U) % route.size()];
        const float dx = b[0] - a[0];
        const float dz = b[1] - a[1];
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!std::isfinite(length) || length <= 1.0e-5F) return {};
        if (index < motion.route_segment) distance += length;
        else distance += length * motion.route_fraction;
    }
    return {distance, std::isfinite(distance)};
}

} // namespace

RouteOpponentAiDecision decide_route_opponent_action(
    RouteOpponentAiState state,
    const std::span<const std::array<float, 2>> route,
    const std::span<const RouteMotionState> participants,
    const std::size_t self_index) noexcept
{
    RouteOpponentAiDecision result{};
    result.next_state = state;
    if (self_index >= participants.size()) return result;

    float track_length = 0.0F;
    if (!route_lengths(route, track_length)) return result;
    const auto& self = participants[self_index];
    const auto self_position = position_on_route(self, route);
    if (!self_position.valid || !std::isfinite(self.speed)) return result;
    result.valid = true;
    if (self.race.finished) {
        result.next_state.braking_latched = false;
        return result;
    }

    float nearest_gap = track_length;
    float leader_speed = 0.0F;
    for (std::size_t index = 0U; index < participants.size(); ++index) {
        if (index == self_index || participants[index].race.finished ||
            !std::isfinite(participants[index].speed)) continue;
        const auto candidate_position = position_on_route(participants[index], route);
        if (!candidate_position.valid) continue;
        float gap = candidate_position.distance - self_position.distance;
        if (gap <= 1.0e-3F) gap += track_length;
        if (gap <= 1.0e-3F || gap >= nearest_gap) continue;
        nearest_gap = gap;
        leader_speed = std::max(0.0F, participants[index].speed);
        result.leader_index = index;
    }

    // These policy values are deliberately isolated here. They make the
    // prototype opponents respond to traffic while native ACAR inputs remain
    // unavailable; they must not be reported as original-game constants.
    constexpr float open_route_target_speed = 28.0F;
    constexpr float maximum_target_speed = 32.0F;
    if (result.leader_index == std::numeric_limits<std::size_t>::max()) {
        result.target_speed = open_route_target_speed;
        result.distance_to_leader = track_length;
    } else {
        result.distance_to_leader = nearest_gap;
        const float safe_gap = std::clamp(3.0F + std::max(0.0F, self.speed),
            8.0F, 32.0F);
        result.target_speed = std::clamp(leader_speed +
            (nearest_gap - safe_gap) * 0.75F, 0.0F, maximum_target_speed);
    }

    const float closing_speed = self.speed - result.target_speed;
    const bool braking = state.braking_latched
        ? closing_speed > 0.25F
        : closing_speed > 1.5F;
    result.next_state.braking_latched = braking;
    if (braking) {
        result.brake = std::clamp(closing_speed / 12.0F, 0.35F, 1.0F);
    } else if (result.target_speed > self.speed + 0.5F) {
        result.throttle = 1.0F;
    }
    return result;
}

} // namespace viper
