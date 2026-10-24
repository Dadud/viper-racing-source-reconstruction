#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "viper/world_track_objects.hpp"

namespace viper {

inline constexpr std::size_t single_race_driver_order_capacity = 16;
// Reconstructs 0x41C8E0's fresh-session initialization of 0x503D40.
[[nodiscard]] std::array<std::int32_t, single_race_driver_order_capacity>
initialize_single_race_driver_order() noexcept;

struct SingleRaceEntrantObservation {
    // 0x40A180 reads this value from the car_count field and passes count - 1
    // to 0x40A850, which sets +0xCA8 to (count - 1) + 1.
    std::int32_t configured_car_count{};
    // Values at 0x503D40; 0x41C9B0 finds the first value equal to count - 1.
    std::span<const std::int32_t> driver_order_values;
    std::int32_t opponent_strength_value{};
    // For each AI driver slot 0..6, the string read from its driver object
    // at +0x210 by 0x41C9F0.
    std::span<const std::string_view> ai_names_by_slot;
    // Values copied/resolved by the setup-data helpers before 0x40A850.
    std::optional<std::string_view> player_car_resource_name;
    std::optional<std::string_view> resolved_player_name;
    bool player_name_ini_key_found{};
    std::optional<std::int32_t> player_paintjob_value;
    bool global_0x4d10fc_nonzero{};
    bool global_0x4eccc0_nonzero{};
    // Only used when the driver-order search misses and 0x41C9B0 calls
    // 0x411000 before returning index zero.
    std::optional<bool> panic_helper_returned;
};

struct SingleRaceEntrantRecordPlan {
    std::size_t record_index{};
    std::uint64_t record_offset{};
    std::uint32_t marker_value{};
    std::optional<std::string> display_name;
    std::optional<std::string> car_resource_name;
    std::optional<std::uint32_t> word_at_0xc0;
    std::uint8_t byte_at_0xc4{};
};

struct SingleRaceEntrantPlan {
    bool input_complete{true};
    bool opponent_strength_global_written{};
    std::int32_t opponent_strength_global_value{};
    std::int32_t participant_count{};
    std::optional<std::size_t> player_record_index;
    bool driver_order_lookup_panicked{};
    bool panic_continuation_unknown{};
    bool panic_prevented_record_build{};
    std::vector<SingleRaceEntrantRecordPlan> records;
};

// Projects 0x40A850's 0xC8-stride participant-row writes. 0x40A620 confirms
// that marker 0 selects the human initializer and marker 1 selects the AI
// initializer. Human rows store the inverted paintjob value at +0xC0 and a
// two-global conjunction at +0xC4; AI rows store driver slot and zero there.
[[nodiscard]] SingleRaceEntrantPlan build_single_race_entrants(
    const SingleRaceEntrantObservation& observation);

// Joins the current global car-row index in the base observation to its
// configured participant row. Placement geometry, coordinates, and the
// global ordinal stay caller inputs; count, subtype marker, and car stem come
// from the entrant plan.
[[nodiscard]] std::optional<CarTrackLineObservation>
make_single_race_car_track_line_observation(
    const SingleRaceEntrantPlan& entrants,
    CarTrackLineObservation placement_observation);

enum class SingleRaceResourceActionKind : std::uint8_t {
    unload_resource_set,
    load_resource_set,
};

struct SingleRaceResourceAction {
    SingleRaceResourceActionKind kind{};
    std::string resource_name;
    std::uint32_t helper{};
    std::optional<std::size_t> entrant_index;
};

enum class SingleRaceEntrantInitializerKind : std::uint8_t {
    player_vehicle,
    player_vehicle_skipped_by_global,
    ai_vehicle,
    marker_2_noop,
    panic_path,
};

struct SingleRaceEntrantInitializerPlan {
    std::size_t entrant_index{};
    std::uint32_t marker_value{};
    SingleRaceEntrantInitializerKind kind{};
    std::optional<std::uint32_t> initializer_address;
    bool global_0x4e322c_gate_checked{};
    bool vehicle_record_offset_0x34_used{};
    bool entrant_car_name_offset_0x39_used{};
};

struct SingleRaceSetupPostReadObservation {
    std::string_view selected_track_resource_name;
    std::int32_t difficulty_resource_index{};
    std::int32_t participant_count{};
    std::span<const SingleRaceEntrantRecordPlan> entrants;
    bool global_0x4e322c_nonzero{};
    // Only used for marker 3 or an unexpected marker reaching 0x411000.
    std::optional<bool> panic_helper_returned;
};

struct SingleRaceSetupPostReadPlan {
    bool input_complete{true};
    bool loading_screen_setup_called{true};
    bool opponent_strength_global_written{true};
    bool difficulty_index_out_of_range{};
    bool panic_helper_called{};
    bool panic_prevented_remaining_initializers{};
    bool panic_continuation_unknown{};
    std::array<std::uint32_t, 3> setup_call_order{
        0x40a7b0U, 0x40a710U, 0x40a620U};
    std::vector<SingleRaceResourceAction> resource_actions;
    std::vector<SingleRaceEntrantInitializerPlan> entrant_initializers;
};

// Projects the resource and per-row setup reached through 0x40A5D0.
[[nodiscard]] SingleRaceSetupPostReadPlan plan_single_race_setup_post_read(
    const SingleRaceSetupPostReadObservation& observation);

enum class VehicleInitializerStep : std::uint8_t {
    test_ai_record_for_4d3e88,
    build_ai_fallback_record,
    copy_ai_fallback_record_0x8c,
    resolve_ai_car_configuration,
    resolve_ai_wheel_table,
    initialize_ai_vehicle,
    write_vehicle_scalar_0x04,
    scale_vehicle_scalar_0x4c,
    scale_vehicle_scalar_0x48,
    load_human_car_record_resource,
    copy_human_record_0x8c,
};

struct VehicleInitializerWrapperPlan {
    std::uint32_t entry_address{};
    std::vector<VehicleInitializerStep> ordered_steps;
    std::optional<std::uint32_t> human_car_setup_loader_address;
    std::uint32_t record_copy_bytes{};
    bool ai_fallback_branch_conditional{};
    bool values_require_runtime_observations{true};
};

// Projects the wrapper-level behavior of 0x41A000 (AI) and 0x45EFD0
// (human). Arithmetic results and resource/configuration inputs remain data.
[[nodiscard]] VehicleInitializerWrapperPlan plan_vehicle_initializer_wrapper(
    std::uint32_t initializer_address);

enum class AiVehicleConfigurationStep : std::uint8_t {
    build_configuration_resource_name,
    load_configuration_resource,
    query_configuration_record,
    write_fallback_coefficients,
    calculate_configuration_coefficients,
    unload_configuration_resource,
};

struct AiVehicleConfigurationObservation {
    // Return status from 0x45E2F0 after load/version/optional-upgrade handling.
    std::optional<bool> configuration_data_available;
    // Parsed by parse_car_file_payload; 0x41A0C0 disables upgrade overlays.
    std::optional<CarFilePayload> loaded_car_file;
    // Caller values: 0x41A0C0 param_1[0] and param_3.
    std::optional<float> vehicle_scalar_0x00;
    std::optional<float> call_scale;
    std::optional<bool> compatibility_division_helper_enabled;
};

struct AiVehicleConfigurationPlan {
    bool input_complete{};
    bool fallback_coefficients_written{};
    bool coefficient_loop_reached{};
    bool exact_five_entry_table_selected{};
    bool compatibility_helper_branch{};
    bool base_division_helper_path{};
    bool numeric_result_approximate{true};
    std::int32_t coefficient_writes{};
    bool coefficient_values_calculated{};
    std::vector<float> coefficient_values;
    std::array<std::uint32_t, 7> fallback_coefficient_bits{{
        0x402a3d71U, 0x3fe3d70aU, 0x3fa66666U, 0x3f800000U,
        0x3f3d70a4U, 0x3f000000U, 0x00000000U,
    }};
    std::array<std::uint32_t, 5> exact_five_entry_table_bits{{
        0x3e800000U, 0x3ecccccdU, 0x3f333333U, 0x3f59999aU,
        0x3f800000U,
    }};
    std::array<std::uint32_t, 6> other_count_table_bits{{
        0x3e570a3dU, 0x3ea8f5c3U, 0x3f000000U, 0x3f28f5c3U,
        0x3f547ae1U, 0x3f800000U,
    }};
    std::vector<AiVehicleConfigurationStep> ordered_steps;
};

// Projects the resource/configuration branch in 0x41A0C0. Coefficient
// operands are retained as binary32 inputs because their source records vary.
[[nodiscard]] AiVehicleConfigurationPlan plan_ai_vehicle_configuration(
    const AiVehicleConfigurationObservation& observation);

} // namespace viper
