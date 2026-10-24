#pragma once

#include <array>
#include <cstdint>

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

} // namespace viper
