#pragma once

#include "viper/race_progression.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <span>

namespace viper {

// Persistent state for the current route-based opponent policy. This is an
// authored runtime policy, not a reconstruction of the original ACAR AI.
struct RouteOpponentAiState {
    bool braking_latched{};
};

struct RouteOpponentAiDecision {
    RouteOpponentAiState next_state{};
    bool valid{};
    bool steer_left{};
    bool steer_right{};
    float throttle{};
    float brake{};
    float target_speed{};
    float distance_to_leader{};
    std::size_t leader_index{std::numeric_limits<std::size_t>::max()};
};

// Produces a deterministic safe-follow action from route progress owned by the
// current runtime. `participants` must be one immutable snapshot for the
// fixed step, with the player at its rendered entrant index. The caller should
// commit next_state only when the corresponding staged motion is accepted.
// Speed targets are specific to the provisional route-motion adapter.
[[nodiscard]] RouteOpponentAiDecision decide_route_opponent_action(
    RouteOpponentAiState state,
    std::span<const std::array<float, 2>> route,
    std::span<const RouteMotionState> participants,
    std::size_t self_index) noexcept;

} // namespace viper
