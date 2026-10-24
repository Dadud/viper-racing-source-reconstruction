#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace viper {

struct AcarCurveNodeObservation {
    std::uint32_t address{};
    std::uint32_t next_address{};
    float position_x_0x04{};
    float position_y_0x08{};
    float tangent_x_0x0c{};
    float tangent_y_0x10{};
    float update_denominator_0x20{};
};

struct AcarCurveTrackerObservation {
    std::uint32_t segment_node_address{};
    float fraction_0x08{};
    float target_x_0x18{};
    float target_y_0x1c{};
    bool fdiv_compatibility_enabled{};
    std::span<const AcarCurveNodeObservation> captured_nodes;
};

enum class AcarCurveTrackerStatus : std::uint8_t {
    updated,
    null_node_dereference,
    node_capture_incomplete,
};

struct AcarCurveTrackerPlan {
    AcarCurveTrackerStatus status{AcarCurveTrackerStatus::updated};
    std::uint32_t segment_node_address{};
    float fraction_0x08{};
    float last_distance_squared{};
    float last_tangent_error{};
    std::uint32_t iterations{};
    bool segment_valid{};
    bool output_written{};
    bool distance_rejected{};
    bool fraction_walked_forward{};
    bool fraction_walked_backward{};
    bool converged_by_tangent_tolerance{};
    bool iteration_limit_reached{};
    bool fdiv_compatibility_path_used{};
    bool x87_extended_precision_approximated{true};
};

// Projects FUN_0041D4F0's linked-segment Newton update without dereferencing
// native pointers. Captured node addresses stand in for the observed links.
[[nodiscard]] AcarCurveTrackerPlan plan_acar_curve_tracker_update(
    const AcarCurveTrackerObservation& observation);

} // namespace viper
