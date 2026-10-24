#pragma once

#include "viper/acar_curve_tracker.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace viper {

struct AcarDriverRouteRecord {
    std::int16_t owner_0x00{};
    // Two XZ vectors consumed by 0x41F0A0 and 0x41F0E0.
    std::array<float, 2> direction_0x04_0x08{};
    std::array<float, 2> comparison_0x0c_0x10{};
};

struct AcarDriverAiState {
    bool directional_candidate_valid_0xef4{};
    std::uint32_t directional_value_bits_0xedc{};
    float directional_score_0xee0{};
    std::uint32_t directional_aux_0xee4{};
    std::uint32_t directional_aux_0xee8{};
    bool directional_mode_0xeec{};
    bool direction_changed_0xef5{};
    bool forward_candidate_valid_0xed0{};
    float forward_value_0xed4{};
    std::uint32_t forward_aux_0xed8{};
};

struct AcarDriverAiObservation {
    std::uint32_t incoming_eax{};
    std::uint32_t profile_threshold_bits_0xf10{};
    std::uint32_t selected_driver_ordinal_0x510{};
    std::int16_t route_owner_0x00{};
    std::array<float, 2> route_direction_0x04_0x08{};
    std::array<float, 2> route_comparison_0x0c_0x10{};
    std::int16_t other_driver_index_0x00{};
    std::int16_t alternate_driver_index_0x02{};
    bool other_driver_registry_entry_present{};
    float velocity_x_0x234{};
    float velocity_z_0x23c{};
    float angle_gate_0xf1c{};
    bool fdiv_compatibility_enabled{};
};

struct AcarDriverAiPlan {
    std::uint8_t return_low_byte{};
    std::int32_t registry_lookup_index_0x41b050{};
    float normalized_velocity_x{};
    float normalized_velocity_z{};
    float forward_projection{};
    float lateral_projection{};
    float lateral_magnitude{};
    float comparison_score{};
    float forward_score{};
    bool profile_gate_passed{};
    bool route_accessors_negated{};
    bool directional_helper_called{};
    bool directional_candidate_stored{};
    bool direction_change_flag_set{};
    bool forward_helper_called{};
    bool forward_candidate_stored{};
    bool fdiv_compatibility_path_used{};
    bool finite_inputs_assumed{};
};

// Projects ACAR's route-relative routine at 0x42ADB0 and its candidate
// arbitration helpers 0x42B320/0x42B2E0. Route lookup and global registry
// results are observations; no caller relationship is inferred from adjacency.
[[nodiscard]] AcarDriverAiPlan update_acar_driver_ai_candidates(
    AcarDriverAiState& state,
    const AcarDriverAiObservation& observation) noexcept;

struct AcarDriverCallbackPlan {
    AcarDriverAiPlan route_candidate_plan{};
    bool fallback_helper_0x42b040_called{};
    bool fallback_helper_result{};
    bool callback_return_value{};
};

// Models the registered wrapper at 0x42AD80: 0x42ADB0 runs first, and its
// nonzero return bypasses fallback 0x42B040. The fallback's result is supplied.
[[nodiscard]] AcarDriverCallbackPlan evaluate_acar_driver_callback(
    AcarDriverAiState& state,
    const AcarDriverAiObservation& observation,
    bool fallback_helper_result) noexcept;

struct AcarFallbackState {
    float candidate_range_0xef8{};
    std::uint32_t candidate_value_0xefc{};
    std::uint32_t candidate_endpoint_0xf04{};
    bool candidate_valid_0xf08{};
    bool candidate_replaced_0xf09{};
    std::uint32_t route_value_0xf0c{};
    std::int32_t route_direction_0xf18{};
    std::uint32_t copied_route_value_0xfd0{};
    float mode_value_0xde0{};
    float damping_value_0xde4{};
    float response_value_0xde8{};
    float mode_value_0xdec{};
    float steering_value_0x500{};
    float steering_scale_0xdcc{};
    bool alternate_wheel_pair_gate_0xbe4{};
    float wheel_pair_value_0x630{};
    float wheel_pair_value_0x7d4{};
    float wheel_pair_value_0x978{};
    float wheel_pair_value_0xb1c{};
};

struct AcarSecondaryCandidateResult {
    bool candidate_replaced{};
    bool replaced_flag_set{};
};

// Models 0x42B3E0's range-minimum candidate and its +0xF09 replacement flag.
[[nodiscard]] AcarSecondaryCandidateResult update_acar_secondary_candidate(
    AcarFallbackState& state,
    float candidate_range,
    std::uint32_t candidate_value,
    std::uint32_t candidate_endpoint) noexcept;

struct AcarRecoveryUpdateResult {
    float steering_delta{};
    float requested_steering{};
    float applied_steering{};
    float wheel_pair_output{};
    bool alternate_wheel_pair_selected{};
};

// Models 0x42ACB0's fixed fallback state writes and the 0x4330F0 smoothed
// steering/control update. This mutates only the portable projection state.
[[nodiscard]] AcarRecoveryUpdateResult apply_acar_recovery_state(
    AcarFallbackState& state) noexcept;

struct AcarRouteFallbackObservation {
    AcarCurveLookupObservation curve_lookup{};
    float profile_threshold_0xf10{};
    float lateral_tolerance_0xf14{};
    std::uint32_t selected_driver_ordinal_0x510{};
    std::int16_t route_owner_0x00{};
    std::array<float, 2> route_direction_0x04_0x08{};
    std::array<float, 4> target_route_values{};
    float velocity_x_0x234{};
    float velocity_z_0x23c{};
    bool fdiv_compatibility_enabled{};
};

struct AcarRouteFallbackPlan {
    AcarCurveLookupPlan curve_lookup{};
    std::optional<float> curve_position_x;
    std::optional<float> curve_position_y;
    float route_lateral_error{};
    float route_forward_projection{};
    float remaining_distance_0xf10{};
    float approach_ratio{};
    float steering_candidate{};
    float interpolated_route_scalar_0x3c{};
    bool lookup_called{};
    bool input_incomplete{};
    bool curve_candidate_found{};
    bool curve_node_inputs_complete{};
    bool distance_and_speed_gates_passed{};
    bool lateral_tolerance_passed{};
    bool recovery_state_branch_reached{};
    bool secondary_candidate_branch_reached{};
    bool secondary_candidate_rejected_below_one{};
    bool returned_true{};
    std::optional<AcarSecondaryCandidateResult> secondary_candidate_result;
    std::optional<AcarRecoveryUpdateResult> recovery_state_result;
};

enum class AcarRouteSteeringMessageCall : std::uint8_t {
    none,
    message_0x4d5288,
    message_0x4d528c,
    message_0x4d52bc,
};

struct AcarRouteSteeringObservation {
    float profile_threshold_0xf10{};
    std::optional<float> current_segment_scalar_0x34;
    std::optional<float> next_segment_scalar_0x34;
    float segment_fraction_0x1064{};
    bool fdiv_compatibility_enabled{};
    bool steering_gate_suppressed_0x101d{};
    bool alternate_route_mode_0xf30{};
    float route_offset_0xf84{};
    std::optional<float> route_reference_scalar_0xf58_plus_0x34;
};

struct AcarRouteSteeringPlan {
    AcarRouteSteeringMessageCall message_call{
        AcarRouteSteeringMessageCall::none};
    std::optional<float> interpolated_segment_scalar;
    std::optional<float> normalized_route_scalar;
    std::optional<bool> segment_scalar_below_two;
    std::optional<bool> route_drive_gate;
    std::optional<bool> update_branch_active;
    std::optional<float> route_distance;
    std::optional<float> output_0;
    std::optional<float> output_1;
    bool fdiv_compatibility_path_used{};
    bool input_incomplete{};
    bool output_written{};
    bool early_return{};
};

// Projects route-aware output steering helper 0x42A6D0.
[[nodiscard]] AcarRouteSteeringPlan plan_acar_route_steering(
    const AcarRouteSteeringObservation& observation) noexcept;

struct AcarForwardCandidateSteeringObservation {
    bool forward_candidate_valid_0xed0{};
    std::uint32_t forward_candidate_value_bits_0xed4{};
    float forward_candidate_auxiliary_0xed8{};
    float incoming_output_0{};
    bool fdiv_compatibility_enabled{};
};

struct AcarForwardCandidateSteeringPlan {
    AcarRouteSteeringMessageCall message_call{
        AcarRouteSteeringMessageCall::none};
    std::optional<float> output_0;
    std::optional<float> normalized_candidate;
    bool fdiv_compatibility_path_used{};
    bool candidate_threshold_passed{};
    bool quotient_gate_passed{};
    bool output_written{};
};

// Projects selected-forward-candidate steering helper 0x42B450.
[[nodiscard]] AcarForwardCandidateSteeringPlan
plan_acar_forward_candidate_steering(
    const AcarForwardCandidateSteeringObservation& observation) noexcept;

struct AcarComposedDriverCallbackPlan {
    AcarDriverAiPlan route_candidate_plan{};
    bool fallback_helper_0x42b040_called{};
    std::optional<AcarRouteFallbackPlan> fallback_plan;
    std::optional<bool> callback_return_value;
};

// Projects the route geometry and state selection in fallback helper 0x42B040.
// The global route table row and node +0x3c values are explicit observations.
[[nodiscard]] AcarRouteFallbackPlan plan_acar_route_fallback(
    AcarFallbackState& state,
    const AcarRouteFallbackObservation& observation);

// Composes 0x42AD80, 0x42ADB0, and its conditional 0x42B040 fallback.
[[nodiscard]] AcarComposedDriverCallbackPlan evaluate_acar_driver_callback_with_fallback(
    AcarDriverAiState& driver_state,
    AcarFallbackState& fallback_state,
    const AcarDriverAiObservation& driver_observation,
    const AcarRouteFallbackObservation& fallback_observation);

} // namespace viper
