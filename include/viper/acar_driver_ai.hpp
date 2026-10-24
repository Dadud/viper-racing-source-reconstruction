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

} // namespace viper
