#pragma once

#include "viper/acar_curve_tracker.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

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

enum class AcarControlResponseMessageCall : std::uint8_t {
    none,
    message_0x4d52ac,
    message_0x4d52b0,
    message_0x4d52b4,
    message_0x4d52b8,
    message_0x4d52c0,
};

struct AcarControlResponseObservation {
    float state_0xef0{};
    float steering_output_0{};
    float accumulator_input_1{};
    float route_value_0xf14{};
    float lateral_value_0xf18{};
    std::uint32_t lateral_sign_reference_bits_0xf20{};
    std::uint32_t steering_relation_bits_0xf1c{};
};

struct AcarControlResponsePlan {
    std::vector<AcarControlResponseMessageCall> message_calls;
    float next_state_0xef0{};
    float accumulator_output_1{};
    std::optional<float> steering_output_0;
    std::optional<float> state_0x1074;
    bool edge_brake_gate_passed{};
    bool lateral_slam_gate_passed{};
    bool lateral_slam_state_written{};
    bool state_decay_branch_reached{};
    bool output_1_written{};
};

// Projects control-response and lateral-slam helper 0x42B570.
[[nodiscard]] AcarControlResponsePlan plan_acar_control_response(
    const AcarDriverAiState& driver_state,
    const AcarControlResponseObservation& observation) noexcept;

enum class AcarBehaviorControlTailCall : std::uint8_t {
    helper_0x428cd0,
    helper_0x428b40,
    helper_0x428e20,
    helper_0x433230,
    helper_0x4331d0,
    helper_0x4331c0,
};

struct AcarBehaviorGeometryObservation {
    float vector_x_0xf74{};
    float vector_y_0xf78{};
    std::uint32_t vector_mode_bits_0xf80{};
    std::uint32_t fallback_direction_bits_0xf64{};
    float steering_scale_0xdcc{};
    float steering_gain_0xf50{};
    bool fdiv_compatibility_enabled{};
};

struct AcarBehaviorGeometryPlan {
    float initial_local_output_0{};
    std::optional<float> effective_vector_x;
    std::optional<float> effective_vector_y;
    std::optional<float> angle_radians;
    bool vector_angle_branch_reached{};
    bool vector_normalization_reached{};
    bool fdiv_compatibility_path_used{};
};

// Projects the vector-angle seed and signed fallback in callback fragment
// 0x42B7B0.
[[nodiscard]] AcarBehaviorGeometryPlan plan_acar_behavior_geometry_seed(
    const AcarBehaviorGeometryObservation& observation) noexcept;

struct AcarSteeringHistoryTailObservation {
    float profile_step_0xf58_plus_0x28{};
    float steering_value_0xf4c{};
    float response_ramp_0xf48{};
    std::uint32_t previous_control_bits_0x500{};
    float speed_0x1020{};
    float steering_scale_0xdcc{};
    bool fdiv_compatibility_enabled{};
};

struct AcarSteeringHistoryTailPlan {
    float maximum_delta{};
    float limited_delta{};
    float steering_value_0xf4c{};
    float response_ramp_0xf48{};
    float control_value_0x500{};
    float wheel_control_0x630{};
    float wheel_control_0x7d4{};
    bool response_ramp_increased{};
    bool steering_clamped{};
    bool fdiv_compatibility_path_used{};
};

// Projects callback tail helper 0x428CD0's rate-limited steering history.
[[nodiscard]] AcarSteeringHistoryTailPlan plan_acar_steering_history_tail(
    float steering_target,
    const AcarSteeringHistoryTailObservation& observation) noexcept;

struct AcarAlternateRouteThrottleObservation {
    bool alternate_route_mode_0xf30{};
    std::optional<float> route_value_0xf0c;
    std::optional<float> route_scalar_0xfbc;
    std::optional<float> existing_throttle_limit_0x103c;
};

struct AcarAlternateRouteThrottlePlan {
    std::optional<float> effective_throttle_limit_0x103c;
    float scaled_route_delta{};
    bool throttle_limit_written{};
    bool input_incomplete{};
};

// Projects the callback's alternate-route write to +0x103C.
[[nodiscard]] AcarAlternateRouteThrottlePlan
plan_acar_alternate_route_throttle_limit(
    const AcarAlternateRouteThrottleObservation& observation) noexcept;

enum class AcarThrottleTailMessageCall : std::uint8_t {
    water_cut,
    offground_cut,
};

struct AcarThrottleTailObservation {
    std::optional<float> profile_throttle_0xf58_plus_0x1c;
    std::optional<float> throttle_limit_0x103c;
    std::array<std::optional<bool>, 4> water_cut_wheel_contacts{};
    std::optional<bool> contact_helper_result_0x89c;
    std::optional<bool> contact_helper_result_0xa40;
    std::optional<float> fade_state_0xfe0;
};

struct AcarThrottleTailPlan {
    std::optional<float> throttle_before_offground;
    std::optional<float> fade_state_0xfe0;
    std::optional<float> throttle_output_0xde0;
    std::array<AcarThrottleTailMessageCall, 2> message_calls{};
    std::uint8_t message_call_count{};
    std::optional<bool> second_contact_helper_called;
    std::optional<bool> water_gate_passed;
    std::optional<bool> offground_gate_passed;
    bool input_incomplete{};
};

// Projects callback tail helper 0x428B40 and its +0xDE0 setter.
[[nodiscard]] AcarThrottleTailPlan plan_acar_throttle_tail(
    const AcarThrottleTailObservation& observation) noexcept;

struct AcarBrakeTailObservation {
    std::optional<float> motion_axis_0x50;
    std::optional<float> velocity_x_0x234;
    std::optional<float> motion_axis_0x58;
    std::optional<float> velocity_z_0x23c;
    std::optional<float> motion_axis_0x238;
    std::optional<float> velocity_y_0x54;
    std::optional<std::uint32_t> route_distance_bits_0xf10;
    std::array<std::optional<std::uint8_t>, 4> wheel_flags{};
    std::optional<float> brake_state_0xde4;
    std::optional<float> profile_blend_0xf58_plus_0x08;
    std::optional<float> speed_0x1020;
};

struct AcarBrakeTailPlan {
    std::optional<float> clamped_brake_input;
    std::optional<long double> motion_alignment_dot;
    std::optional<bool> panic_brake_gate_passed;
    std::optional<bool> wheel_flag_sum_is_four;
    std::optional<float> brake_output_0xde4;
    bool panic_brake_message_called{};
    bool blend_branch_reached{};
    bool input_incomplete{};
};

// Projects callback tail helper 0x428E20 and its +0xDE4 setter.
[[nodiscard]] AcarBrakeTailPlan plan_acar_brake_tail(
    float brake_input,
    const AcarBrakeTailObservation& observation) noexcept;

struct AcarBehaviorControlChainObservation {
    AcarBehaviorGeometryObservation geometry{};
    AcarRouteSteeringObservation route_steering{};
    AcarControlResponseObservation control_response{};
    AcarForwardCandidateSteeringObservation forward_steering{};
    AcarSteeringHistoryTailObservation steering_history{};
    AcarThrottleTailObservation throttle_tail{};
    AcarBrakeTailObservation brake_tail{};
    std::optional<float> route_value_0xf0c;
    std::optional<float> route_scalar_0xfbc;
    std::optional<float> existing_throttle_limit_0x103c;
    std::optional<std::uint32_t> post_0x43fdd0_state_0xcbc;
    float clamp_value_0x1018{};
    float speed_0x1020{};
};

struct AcarBehaviorControlChainPlan {
    std::optional<AcarRouteSteeringPlan> route_steering_plan;
    std::optional<AcarControlResponsePlan> control_response_plan;
    std::optional<AcarForwardCandidateSteeringPlan> forward_steering_plan;
    AcarBehaviorGeometryPlan geometry_plan{};
    std::optional<AcarSteeringHistoryTailPlan> steering_history_plan;
    AcarAlternateRouteThrottlePlan alternate_route_throttle_plan{};
    std::optional<AcarThrottleTailPlan> throttle_tail_plan;
    std::optional<AcarBrakeTailPlan> brake_tail_plan;
    std::vector<AcarBehaviorControlTailCall> tail_call_order;
    float local_output_0{};
    float local_output_1{};
    float final_argument_0x2c{};
    float tail_argument_0x428cd0{};
    float tail_argument_0x428b40{1.0F};
    float tail_argument_0x428e20{};
    float tail_argument_0x4331d0{};
    float tail_argument_0x4331c0{1.0F};
    std::optional<float> state_0xde0_after_callback;
    std::optional<float> state_0xde4_after_callback;
    std::optional<float> state_0xde8_after_callback;
    std::optional<float> state_0xdec_after_callback;
    std::optional<std::uint32_t> state_0xd64_after_callback;
    bool helper_0x42a6d0_called{};
    bool helper_0x42b570_called{};
    bool helper_0x42b450_called{};
    bool helper_0x43fdd0_called{};
    bool input_incomplete{};
    bool tail_reached{};
};

// Composes the recoverable helper chain in callback fragment 0x42B7B0.
// Geometry-loop seeds and the stack local at +0x2C remain explicit inputs.
[[nodiscard]] AcarBehaviorControlChainPlan compose_acar_behavior_control_chain(
    const AcarDriverAiState& driver_state,
    const AcarBehaviorControlChainObservation& observation);

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
