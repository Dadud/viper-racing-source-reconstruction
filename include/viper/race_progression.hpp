#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>

namespace viper {

struct RaceProgressionState {
    std::size_t lap_start_segment{};
    std::uint32_t completed_laps{};
    float elapsed_seconds{};
    bool finished{};
};

struct RouteMotionState {
    std::size_t route_segment{};
    float route_fraction{};
    float steering{};
    float speed{};
    RaceProgressionState race;
};

struct RouteMotionResult {
    float x{};
    float z{};
    float yaw{};
    bool valid{};
    bool lap_completed{};
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

// Advances the route-following runtime model used by the Win32 game loop.
// Terrain sampling and rendering remain platform-owned operations.
[[nodiscard]] RouteMotionResult advance_route_motion(
    RouteMotionState& state,
    std::span<const std::array<float, 2>> route,
    bool steer_left,
    bool steer_right,
    bool accelerate,
    bool brake,
    float delta_seconds,
    std::uint32_t target_laps) noexcept;

} // namespace viper
