#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <vector>

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
    // Signed distance traversed along the route adapter's polyline. This is
    // runtime route state, not native rigid-body displacement.
    float route_distance_travelled{};
};

struct RouteMotionResult {
    float x{};
    float z{};
    float yaw{};
    bool valid{};
    bool lap_completed{};
    bool finished{};
};

struct RouteRaceStanding {
    std::size_t participant_index{};
    float finish_time_seconds{};
    bool player{};
};

struct RouteRacePosition {
    std::size_t participant_index{};
    float route_distance_travelled{};
    float finish_time_seconds{};
    bool finished{};
    bool player{};
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
    float throttle,
    float brake,
    float delta_seconds,
    std::uint32_t target_laps,
    bool reverse = false) noexcept;

// Commits a staged route update only after its runtime terrain query succeeds.
// A failed query leaves lap time, route progress, steering, and speed untouched.
[[nodiscard]] bool commit_route_motion_after_terrain_query(
    RouteMotionState& state,
    const RouteMotionState& candidate,
    bool terrain_query_succeeded) noexcept;

// Builds deterministic finish standings from completed finite route-race
// states. `participants` is indexed by rendered entrant slot; player_state is
// supplied separately because the runtime keeps it outside that array.
[[nodiscard]] std::vector<RouteRaceStanding> build_route_race_standings(
    const RouteMotionState& player_state,
    std::span<const RouteMotionState> participants,
    std::size_t player_participant_index);

// Ranks every finite route-runtime participant. Finished entrants rank ahead
// of active entrants; finishers use finish time, and active entrants use
// signed distance travelled with entrant slot as a stable tie-break.
[[nodiscard]] std::vector<RouteRacePosition> build_route_race_positions(
    const RouteMotionState& player_state,
    std::span<const RouteMotionState> participants,
    std::size_t player_participant_index);

} // namespace viper
