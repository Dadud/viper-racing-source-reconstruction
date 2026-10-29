#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "viper/model_resource.hpp"
#include "viper/ncar_state.hpp"
#include "viper/vehicle_setup.hpp"

namespace viper {

inline constexpr std::uint32_t ncar_shared_zero_wheel_reference_address_0x508e80 =
    0x00508e80U;

// Materializes the 0x441C10 reference-vector pointer and its copied payload.
[[nodiscard]] bool materialize_ncar_wheel_reference_vector(
    NcarWheelRuntimeObjectState& destination,
    std::uint32_t wheel_object_offset,
    std::uint32_t guest_pointer,
    PhysicsVector3 copied_value) noexcept;

struct NcarIntegratorWheelPairDifferentialInput {
    float first_value_0x63c{};
    float first_reference_0x7e0{};
    float first_scale_0xdd4{};
    float second_value_0x984{};
    float second_reference_0xb28{};
    float second_scale_0xdd8{};
    std::array<std::uint8_t, 4> contact_bytes{};
};

struct NcarIntegratorWheelPairDifferentialResult {
    float value_0x850{};
    float value_0x6ac{};
    float value_0xb98{};
    float value_0x9f4{};
};

// Builds the paired differential inputs from the four wheel updates that
// precede those reads in 0x431150. The scales and contact gates remain
// explicit body-step inputs; updated +0xE8 values come from each wheel result.
[[nodiscard]] std::optional<NcarIntegratorWheelPairDifferentialInput>
make_ncar_integrator_wheel_pair_differential_input_from_updates(
    const std::array<WheelUpdatePipelineResult, 4>& wheel_updates,
    float first_scale_0xdd4,
    float second_scale_0xdd8,
    std::array<std::uint8_t, 4> contact_bytes) noexcept;

// Projects two paired wheel differences into the four observed car fields.
[[nodiscard]] NcarIntegratorWheelPairDifferentialResult
apply_ncar_integrator_wheel_pair_differentials(
    const NcarIntegratorWheelPairDifferentialInput& input) noexcept;

// Persists the shared integrator's four signed pair outputs to the byte-backed
// wheel +0x158 lanes. The array is committed atomically if all bounded writes
// are valid; callers still own the provenance/completeness of the inputs.
[[nodiscard]] bool apply_ncar_integrator_wheel_pair_differentials_to_wheels(
    std::array<NcarWheelRuntimeObjectState, 4>& wheel_objects,
    std::uint32_t first_wheel_object_offset,
    const NcarIntegratorWheelPairDifferentialResult& result) noexcept;

struct NcarIntegratorCommonForceInput {
    std::array<std::uint8_t, 4> contact_bytes{};
    PhysicsVector3 velocity_delta_0x234_0xdfc{};
    PhysicsVector3 transformed_velocity_delta_0xdf0{};
    PhysicsVector3 force_axis_0x38{};
    PhysicsVector3 reference_axis_0x44{};
    PhysicsVector3 body_axis_0x50{};
    std::array<float, 9> transform_0x38_to_0x58{};
    float resistance_scale_0x14e{};
    float resistance_scale_0x14f{};
    float resistance_scale_0x150{};
    float ground_scale_0x538{};
    float ground_scale_0x53c{};
    float ground_scale_0x540{};
    PhysicsVector3 application_point{};
    PhysicsVector3 body_center{};
};

struct NcarIntegratorCommonForceResult {
    std::uint32_t contact_byte_sum{};
    bool high_contact_branch{};
    PhysicsVector3 local_force{};
    PhysicsVector3 force_vector{};
    bool accumulated{};
};

// Models the contact-count split and force passed to 0x43E6F0 after the
// paired differentials in 0x431150. Matrix/helper outputs remain inputs.
[[nodiscard]] NcarIntegratorCommonForceResult apply_ncar_integrator_common_force(
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorCommonForceInput& input) noexcept;

struct NcarIntegratorVelocityForceInput {
    PhysicsVector3 velocity_vector{};
    PhysicsVector3 projection_axis_0x50{};
    PhysicsVector3 force_axis_0x44{};
    std::array<float, 2> quadratic_scale{};
    std::array<float, 2> damping_scale{};
    std::array<PhysicsVector3, 2> application_points{};
    PhysicsVector3 body_center{};
    float mass_0x200{};
};

struct NcarIntegratorVelocityForceResult {
    float speed{};
    float projection{};
    std::array<PhysicsVector3, 2> force_vectors{};
    std::array<bool, 2> accumulated{};
    float per_step_energy_0xeb4{};
};

// Models the two speed/projection force calls following the common force in
// 0x431150. The transformed application points remain explicit helper outputs.
[[nodiscard]] NcarIntegratorVelocityForceResult apply_ncar_integrator_velocity_forces(
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorVelocityForceInput& input) noexcept;

struct WheelConditionalTireForceInput {
    bool feature_gate_0x168{};
    float tire_scalar_0x10{};
    bool low_traction_flag_0x19c{};
    float response_scale{};
    PhysicsVector3 corrected_tire_vector_0x100{};
    PhysicsVector3 forward_axis_0x38{};
    PhysicsVector3 body_velocity_0x234{};
    PhysicsVector3 body_center_0x5c{};
};

struct WheelConditionalTireForceResult {
    bool gate_reached{};
    float traction_weight{};
    float velocity_squared{};
    PhysicsVector3 force_vector{};
    bool accumulated{};
};

// Models the mode-zero, feature-gated third force call in 0x442530.
[[nodiscard]] WheelConditionalTireForceResult apply_wheel_conditional_tire_force(
    NcarForceMomentAccumulator& accumulator,
    std::int32_t car_mode_0x4f4,
    const WheelConditionalTireForceInput& input) noexcept;

struct NcarAttachedObjectForceInput {
    PhysicsVector3 transformed_relative_velocity{};
    PhysicsVector3 object_basis_0x38{};
    PhysicsVector3 object_basis_0x44{};
    PhysicsVector3 body_axis_0x50{};
    PhysicsVector3 application_point_0x28{};
    PhysicsVector3 body_center{};
    float coefficient_0x04{};
    float coefficient_0x08{};
    float target_projection_0x38{};
    bool select_basis_0x44{};
};

struct NcarAttachedObjectForceResult {
    float speed{};
    float normalized_projection{};
    float slip_factor{};
    float resistance_factor{};
    PhysicsVector3 force{};
    bool applied{};
};

// Models the force law in 0x440B40. Relative velocity and basis vectors are
// outputs of its preceding body-space helper calls and remain explicit.
[[nodiscard]] NcarAttachedObjectForceResult apply_ncar_attached_object_force(
    NcarForceMomentAccumulator& accumulator,
    const NcarAttachedObjectForceInput& input) noexcept;

// Applies the 0x442530 -> 0x43E6F0 tire-force call to the shared NCAR accumulator.
[[nodiscard]] bool accumulate_wheel_tire_body_force(
    NcarForceMomentAccumulator& body_accumulator,
    const WheelTireBasisUpdateResult& wheel_response,
    const PhysicsVector3& application_point_0xc8,
    const PhysicsVector3& body_center_0x5c) noexcept;

struct WheelSecondaryBodyForceInput {
    PhysicsVector3 source_vector_0x10_0x18{};
    float factor_0x74{};
    float factor_0x48{};
    float factor_0x78{};
    PhysicsVector3 wheel_point_0xc8{};
    PhysicsVector3 body_center_0x5c{};
    float wheel_point_weight{};
    float body_center_weight{};
};

struct WheelSecondaryBodyForceResult {
    PhysicsVector3 force_vector{};
    PhysicsVector3 application_point{};
    bool accumulated{};
};

// Reconstructs the second unconditional 0x43E6F0 call in the wheel updater.
[[nodiscard]] WheelSecondaryBodyForceResult accumulate_wheel_secondary_body_force(
    NcarForceMomentAccumulator& body_accumulator,
    const WheelSecondaryBodyForceInput& input) noexcept;

struct WheelBodyForceSequenceResult {
    bool tire_vector_accumulated{};
    WheelSecondaryBodyForceResult secondary_force{};
};

struct NcarPostBodyState {
    std::array<float, 3> vector_0x382{};
    std::array<float, 3> angular_accumulator_0x3a9{};
    float maximum_speed_0x38c{};
    std::array<float, 3> cleared_vector_0x37f{};
    std::uint8_t timeout_pending_0x48c{};
    std::uint8_t timeout_auxiliary_0x48d{};
    std::array<std::uint16_t, 16> timeout_values_0x48e{};
    float timeout_start_0x488{};
    std::array<std::uint32_t, 5> resource_descriptor_0x10{};
    std::array<std::uint8_t, 5> resource_flag_0x15{};
    std::uint32_t integrator_mesh_descriptor_0x10{};
    std::uint8_t integrator_mesh_descriptor_flag_0x15{};
};

struct NcarImpactMeshGroupInput {
    std::span<std::array<float, 8>> destination_vertices;
    bool destination_vertex_buffer_resolved{};
    std::span<const std::uint16_t> source_vertex_indices;
    bool source_vertex_index_buffer_resolved{};
    std::optional<std::uint32_t> descriptor_pointer_0x10;
};

struct NcarIntegratorVirtualRecordCallbackInput {
    BodyIntegrationVirtualRecordCall record{};
    std::array<float, 3> body_local_offset_0x28{};
    std::array<float, 9> body_orientation_matrix_0x38{};
    std::array<float, 3> body_center_0x5c{};
    bool global_update_gate_0x425870{};
    bool fdiv_compatibility_enabled{};
    std::span<std::array<float, 8>> mesh_vertices;
    bool mesh_vertex_buffer_resolved{};
    std::int16_t mesh_vertex_index_begin{};
    std::int16_t mesh_vertex_index_end{};
    bool mesh_vertex_index_range_resolved{};
    std::optional<std::uint32_t> mesh_descriptor_pointer_0x10;
    std::array<NcarImpactMeshGroupInput, 5> mesh_groups{};
    float current_time_0x425790{};
    std::uint32_t vehicle_ordinal_0x510{};
};

struct NcarMeshBoundsMaskInput {
    std::span<std::uint16_t> row_words;
    float row_begin{};
    float row_end{};
    float column_begin{};
    float column_end{};
};

struct NcarMeshBoundsMaskResult {
    std::int32_t row_begin_index{};
    std::int32_t row_end_index{};
    std::int32_t column_begin_index{};
    std::int32_t column_end_index{};
    std::uint16_t column_bits{};
    std::size_t rows_written{};
    std::uint64_t rows_out_of_range{};
};

struct NcarIntegratorMeshResponseResult {
    bool handler_reached{};
    bool timeout_state_written{};
    bool input_complete{};
    bool nearest_vertex_found{};
    std::size_t nearest_vertex_index{};
    PhysicsVector3 mesh_focus_center{};
    float displacement_scale{};
    std::size_t vertex_displacement_stores{};
    std::array<NcarMeshBoundsMaskResult, 2> bounds_mask_updates{};
    bool main_descriptor_refresh_reached{};
    std::array<std::size_t, 5> group_vertices_copied{};
    std::array<std::size_t, 5> group_vertices_unprocessed{};
    std::array<bool, 5> group_descriptor_refresh_reached{};
};

struct NcarIntegratorVirtualRecordCallbackResult {
    bool callback_reached{};
    float force_magnitude_squared{};
    bool global_gate_passed{};
    bool impact_threshold_passed{};
    bool timeout_state_written{};
    float timeout_start_time_written{};
    std::uint32_t event_helper{0x004273C0};
    std::uint8_t event_type{5};
    std::uint8_t event_payload_size{0x0e};
    bool event_payload_bytes_resolved{};
    std::array<std::uint8_t, 14> event_payload{};
    PhysicsVector3 event_local_position{};
    PhysicsVector3 event_local_force{};
    bool secondary_virtual_call_reached{};
    NcarIntegratorMeshResponseResult mesh_response{};
    std::uint32_t secondary_virtual_wrapper{0x00427E00};
    std::uint32_t secondary_virtual_target{0x00430610};
};

// Reconstructs the bounded row/column bit-mask writes in 0x430B70.
[[nodiscard]] NcarMeshBoundsMaskResult apply_ncar_mesh_bounds_mask(
    const NcarMeshBoundsMaskInput& input) noexcept;

[[nodiscard]] NcarIntegratorMeshResponseResult apply_ncar_integrator_mesh_response(
    NcarPostBodyState& state,
    const NcarIntegratorVirtualRecordCallbackInput& input,
    const PhysicsVector3& local_force,
    const PhysicsVector3& local_position) noexcept;

// Models the PCAR/NCAR vtable +0x38 method reached by duration-gated records.
[[nodiscard]] NcarIntegratorVirtualRecordCallbackResult
apply_ncar_integrator_virtual_record_callback(
    NcarPostBodyState& state,
    const NcarIntegratorVirtualRecordCallbackInput& input) noexcept;

struct NcarPostBodyInput {
    // 0xD88 vector transformed by the post-0x43ECE0 orientation matrix. The
    // optional derivation is enabled by the composed four-wheel body step.
    std::array<float, 3> source_vector_0xd88{};
    // Car/body local point at +0x28 used by the integrator impact event.
    std::array<float, 3> body_local_offset_0x28{};
    std::array<float, 9> orientation_matrix_0x38{};
    bool derive_vector_0x382{};
    // Used when deriving is disabled, preserving direct helper-level callers.
    std::array<float, 3> transformed_vector_0x382{};
    std::array<float, 3> body_axis_0x14{};
    float half_offset_scale_0x385{};
    std::array<float, 3> angular_velocity_0xa2{};
    std::array<float, 3> reference_axis_0x44{};
    std::array<float, 3> linear_velocity_0x234{};
    std::array<std::uint8_t, 4> wheel_contact_bytes{};
    // Raw float bits at car offsets +0x670, +0x814, +0x9B8, and +0xB5C.
    std::array<std::uint32_t, 4> wear_value_bits{};
    float current_time_0x425790{};
    bool global_update_gate_0x425870{};
    bool fdiv_compatibility_enabled{};
    std::span<std::array<float, 8>> integrator_mesh_vertices;
    bool integrator_mesh_vertex_buffer_resolved{};
    std::int16_t integrator_mesh_vertex_index_begin{};
    std::int16_t integrator_mesh_vertex_index_end{};
    bool integrator_mesh_vertex_index_range_resolved{};
    std::optional<std::uint32_t> integrator_mesh_descriptor_pointer_0x10;
    std::array<NcarImpactMeshGroupInput, 5> integrator_mesh_groups{};
    std::array<NcarResetResourceGroupInput, 5> timeout_resource_groups{};
    std::uint32_t vehicle_ordinal_0x510{};
};

struct NcarPostBodyResult {
    bool angular_accumulators_updated{};
    bool maximum_speed_store_reached{};
    bool timeout_callback_0x433920_reached{};
    bool wheel_damage_flags_cleared{};
    bool backend_virtual_0x478_reached{};
    // The integrator's final global-interface +0x38 callback is unconditional.
    bool integrator_callback_0x508a04_slot_0x38_reached{};
    std::uint32_t integrator_callback_vehicle_ordinal_0x510{};
    std::vector<NcarIntegratorVirtualRecordCallbackResult>
        integrator_virtual_record_callbacks;
    std::uint8_t resource_groups_visited{};
    std::array<std::size_t, 5> resource_records_copied{};
    std::array<std::size_t, 5> resource_records_unprocessed{};
    bool resource_group_inputs_complete{true};
};

// Models the direct/numeric tail and timeout-reset effects after 0x43ECE0.
[[nodiscard]] NcarPostBodyResult advance_ncar_post_body_tail(
    NcarPostBodyState& state,
    const NcarPostBodyInput& input) noexcept;

// Returns the 0x433A00 four-bit mask using the original signed DWORD compares.
[[nodiscard]] std::uint8_t ncar_integrator_wear_gate_0x433a00(
    const std::array<std::uint32_t, 4>& wear_value_bits) noexcept;

struct NcarIntegratorModeForceInput {
    std::int32_t mode_0x4f4{-1};
    std::array<std::uint8_t, 4> contact_bytes{};
    std::array<std::uint32_t, 4> wear_value_bits{};
    std::array<std::uint8_t, 2> leading_wheel_damage_bytes{};
    PhysicsVector3 angular_velocity_0x240{};
    PhysicsVector3 body_axis_0x44{};
    PhysicsVector3 linear_velocity_0x234{};
    PhysicsVector3 body_axis_0x50{};
    PhysicsVector3 force_axis_0x38{};
    float force_scale_0xdcc{};
    float force_scale_0x500{};
    PhysicsVector3 application_point{};
    PhysicsVector3 body_center{};
    bool fdiv_compatibility_enabled{};
};

struct NcarIntegratorModeForceResult {
    bool supported_mode{};
    bool contact_gate_passed{};
    bool forward_gate_passed{};
    PhysicsVector3 force_vector{};
    bool accumulated{};
};

// Reconstructs the mode-0/mode-1 force branch in 0x431150; raw wear bit
// comparisons and the call-site application point remain explicit.
[[nodiscard]] NcarIntegratorModeForceResult apply_ncar_integrator_mode_force(
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorModeForceInput& input) noexcept;

// Applies the two adjacent 0x43E6F0 calls in their observed order.
[[nodiscard]] WheelBodyForceSequenceResult accumulate_wheel_body_force_sequence(
    NcarForceMomentAccumulator& body_accumulator,
    const WheelTireBasisUpdateResult& wheel_response,
    const PhysicsVector3& application_point_0xc8,
    const PhysicsVector3& body_center_0x5c,
    const WheelSecondaryBodyForceInput& secondary_force) noexcept;

struct WheelCollisionImpulseApplicationResult {
    WheelCollisionImpulseResult response{};
    bool point_impulse_accumulated{};
    bool damage_flag_written{};
};

struct FourWheelCollisionImpulseResult {
    std::array<WheelCollisionImpulseApplicationResult, 4> wheel_results{};
    std::array<std::uint8_t, 4> wheel_damage_flags{};
    std::array<std::size_t, 4> wheel_subobject_offsets{};
    std::size_t wheel_calls{};
};

struct NcarIntegratorPrefixInput {
    // Car +0x4FC controls the first-frame child-state/velocity reset.
    std::uint32_t state_0x4fc{};
    bool force_gate_0xe3d{};
    float mass_0x7c{};
};

struct NcarIntegratorPrefixResult {
    bool initialization_reset_reached{};
    std::uint32_t child_value_0xdec{};
    std::uint32_t child_value_0xde4{0x3f800000U};
    bool contact_virtual_callback_0x50_reached{};
    bool initial_force_call_0x43e6f0_reached{};
    PhysicsVector3 application_point{};
    PhysicsVector3 point_velocity{};
    PhysicsVector3 force{};
    bool force_accumulated{};
};

// Models the pre-wheel prefix of 0x431150: conditional child/default and
// velocity stores, followed by the contact-gated vertical force accumulator.
[[nodiscard]] NcarIntegratorPrefixResult apply_ncar_integrator_prefix(
    BodyIntegrationKinematicsInput& kinematics,
    NcarForceMomentAccumulator& accumulator,
    const NcarIntegratorPrefixInput& input,
    const std::array<std::uint8_t, 4>& contact_bytes) noexcept;

struct NcarIntegratorTimerState {
    float ramp_0x50c{};
    float record_time_0xe44{};
};

// Values copied by 0x419E80 into one ordinal row before the shared pair pass.
// Unregistered slots retain the matrix allocator's zero-filled values.
struct NcarOrdinalPairRowInput {
    // Distinguishes a known unregistered slot from a missing owner/registration
    // observation. Native unregistered slots are zero-filled during the row
    // gather, but an absent caller row is not evidence of non-registration.
    bool registration_resolved{};
    bool registered{};
    float position_x_0x5c{};
    float position_z_0x64{};
    float velocity_x_0x234{};
    float velocity_z_0x23c{};
};

struct NcarOrdinalPairRecord {
    std::int16_t endpoint_a_ordinal_0x00{};
    std::int16_t endpoint_b_ordinal_0x02{};
    float position_delta_x_0x04{};
    float position_delta_z_0x08{};
    float velocity_delta_x_0x0c{};
    float velocity_delta_z_0x10{};
};

struct NcarOrdinalPairLookupResult {
    bool input_complete{};
    std::uint32_t row_ordinal{};
    std::vector<std::optional<std::uint32_t>> sorted_partner_ordinals;
    std::optional<NcarOrdinalPairRecord> record;
};

// Rebuilds the middle n-entry row pointer list initialized by 0x41EAB0,
// populated by 0x41EC40, sorted by the 0x41EE90 comparator and indexed by
// 0x433570. Input rows are in descriptor ordinal order; null registration
// entries retain the native zero-filled position/velocity values.
[[nodiscard]] NcarOrdinalPairLookupResult resolve_ncar_ordinal_pair_lookup(
    std::span<const NcarOrdinalPairRowInput> rows,
    std::uint32_t ordinal);

struct NcarIntegratorTimerInput {
    // Raw global counter consumed by 0x425790, scaled by the binary's 0.016f.
    std::uint32_t timer_ticks_0x508774{};
    std::uint32_t vehicle_ordinal_0x510{};
    // Distinguish a known null record from an unavailable table lookup.
    bool record_lookup_resolved{};
    bool record_nonnull{};
    float record_x_0x04{};
    float record_y_0x08{};
    // When available, this supersedes the explicit record observation above
    // and reconstructs the descriptor's selected pair record from native row
    // inputs. The span length is the descriptor dimension.
    bool ordinal_rows_resolved{};
    std::span<const NcarOrdinalPairRowInput> ordinal_rows;
};

struct NcarIntegratorTimerResult {
    std::uint32_t vehicle_ordinal_0x510{};
    bool time_window_reached{};
    float elapsed_time{};
    bool quadratic_ramp_branch{};
    bool ordinal_record_lookup_reached{};
    bool record_radius_gate_passed{};
    bool record_time_advanced{};
    bool input_complete{true};
};

// Reconstructs 0x433570's 6.2-second gate, 6.0-second bit cutoff, ramp
// formula, and nearby-record timestamp update. Table lookup data stays input.
[[nodiscard]] NcarIntegratorTimerResult advance_ncar_integrator_timer(
    NcarIntegratorTimerState& state,
    const NcarIntegratorTimerInput& input);

struct NcarIntegratorEarlyBoundsResult {
    bool within_bounds{};
    bool callback_0x508a04_slot_0x3c_reached{};
    std::uint32_t callback_vehicle_ordinal_0x510{};
};

// Projects the raw DWORD range checks at 0x432130 and its inverse caller gate.
[[nodiscard]] NcarIntegratorEarlyBoundsResult
plan_ncar_integrator_early_bounds(
    const PhysicsVector3& position,
    std::uint32_t vehicle_ordinal_0x510) noexcept;

struct NcarIntegratorCurveGateState {
    std::uint8_t parent_flag_0xc3c{};
    float value_0xde0{};
};

struct NcarIntegratorCurveGateInput {
    std::uint8_t parent_gate_0xc3d{};
    std::uint32_t parent_value_bits_0xc7c{};
    std::uint8_t curve_gate_0x41{};
};

struct NcarIntegratorSignalState {
    float stored_value_0xddc{};
    std::uint8_t copied_flag_0xe49{};
    std::uint32_t value_bits_0xc7c{};
    float limit_scale_0xc8c{};
    float callback_scale_0xc90{};
    float smoothed_value_0xc94{};
};

struct NcarIntegratorSignalInput {
    std::uint8_t ramp_gate_0xbe4{};
    std::uint32_t previous_value_bits_0xd70{};
    float callback_value_0x339{};
    float multiplier_0xdd0{};
    float callback_value_0xc84{};
    float callback_value_0xc80{};
};

struct NcarIntegratorSignalResult {
    bool ramp_setter_0x43f8e0_reached{};
    bool helper_0x43f830_reached{};
    bool paired_setters_reached{};
    float clamp_bound{};
    float smoothed_value{};
    float paired_setter_value{};
    float paired_setter_opposite{};
};

struct NcarIntegratorWheelCallbackSetupInput {
    // Constructor source-record fields read by 0x433C00 at +0x58, +0x5C,
    // and +0x12C respectively.
    float source_width_0x58{};
    float source_ratio_0x5c{};
    float source_value_0x12c{};
};

struct NcarIntegratorWheelCallbackSetup {
    float value_0xd84{};
    float value_0xd90{};
    float angle_0xdcc{};
};

// Reconstructs constructor stores in 0x433C00 from its source setup record.
[[nodiscard]] NcarIntegratorWheelCallbackSetup
initialize_ncar_integrator_wheel_callback_setup(
    const NcarIntegratorWheelCallbackSetupInput& input) noexcept;

// Reads the three mapped source values at runtime-record +0x58/+0x5C/+0x12C.
// Returns no result when either mapped field falls outside the input record.
[[nodiscard]] std::optional<NcarIntegratorWheelCallbackSetup>
initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
    std::span<const std::byte> runtime_record) noexcept;

struct NcarWheelVertexAnchorInput {
    // Mapped NCAR source-record values read by 0x433C00 at +0x58, +0x5C,
    // and +0x128. Vertex positions are the first three floats of each FNIM row.
    float source_width_0x58{};
    float source_ratio_0x5c{};
    float source_offset_0x128{};
    // Physics-options byte copied by 0x425420 from param_1 + 0x24 and
    // queried by 0x425870 before the constructor selects the model vertex.
    bool model_vertex_anchor_enabled{true};
    std::span<const ModelVertex> model_vertices;
};

enum class NcarWheelAnchorSource : std::uint8_t {
    model_vertex,
    shared_zero_vector
};

struct NcarWheelVertexAnchorResult {
    std::array<std::array<float, 3>, 4> candidate_positions{};
    std::array<std::optional<std::size_t>, 4> vertex_indices{};
    std::array<float, 4> nearest_distances{};
    std::array<NcarWheelAnchorSource, 4> sources{};
    std::array<std::array<float, 3>, 4> selected_anchor_positions{};
};

// Reconstructs the constructor's option-gated source selection and strict
// nearest-vertex scan. A disabled gate selects the shared zero vector.
[[nodiscard]] NcarWheelVertexAnchorResult select_ncar_wheel_vertex_anchors(
    const NcarWheelVertexAnchorInput& input) noexcept;

// Validates that each selected anchor matches its option-gated source:
// model-vertex sources require an index; shared-zero sources require no index
// and retain the zero-vector coordinates.
[[nodiscard]] bool ncar_wheel_vertex_anchor_result_complete(
    const NcarWheelVertexAnchorResult& result) noexcept;

// Reads mapped fields +0x58/+0x5C/+0x128 and selects anchors directly from
// parsed FNIM rows. Returns no result if the bounded runtime record is short.
[[nodiscard]] std::optional<NcarWheelVertexAnchorResult>
select_ncar_wheel_vertex_anchors_from_runtime_record(
    std::span<const std::byte> runtime_record,
    std::span<const ModelVertex> model_vertices,
    bool model_vertex_anchor_enabled = true) noexcept;

struct NcarWheelCoreInitializationInput {
    std::span<const std::byte> runtime_record;
    std::size_t wheel_index{};
    std::array<float, 3> selected_model_vertex{};
    // Car-object +0x1F0, set by the observed NCAR body initializer.
    float body_mass_scale_0x1f0{};
    bool fdiv_compatibility_enabled{};
};

struct NcarWheelCoreInitialization {
    std::size_t wheel_index{};
    bool front_axle{};
    float side_sign{};
    std::array<float, 3> candidate_position{};
    std::array<float, 3> position_relative_to_selected_vertex{};
    std::array<float, 3> selected_model_vertex{};
    float value_0x3c{};
    float value_0x5c{};
    float angle_0x60{};
    float angle_0x64{};
    float value_0x44{};
    float value_0x48{};
    float value_0x40{};
    float value_0x4c{};
    float coefficient_0x9c{};
    float coefficient_0xa0{};
    WheelPiecewiseTerms suspension_piecewise_terms{};
};

// Applies the constructor outputs modeled above to the byte-backed native
// wheel object, preserving its initialized-byte mask.
[[nodiscard]] bool apply_ncar_wheel_core_initialization(
    NcarWheelRuntimeObjectState& destination,
    std::uint32_t wheel_object_offset,
    const NcarWheelCoreInitialization& initialization) noexcept;

// Reconstructs position and steering stores from wheel constructor 0x441C10.
// Missing record fields or wheel indices outside [0, 4) yield no result.
[[nodiscard]] std::optional<NcarWheelCoreInitialization>
initialize_ncar_wheel_core(
    const NcarWheelCoreInitializationInput& input) noexcept;

struct NcarFourWheelCoreInitializationInput {
    std::span<const std::byte> runtime_record;
    std::span<const ModelVertex> model_vertices;
    // Car-object +0x1F0, set by the observed NCAR body initializer.
    float body_mass_scale_0x1f0{};
    bool fdiv_compatibility_enabled{};
    // Resolved byte copied by the game into DAT_0050878c before 0x433C00.
    bool model_vertex_anchor_enabled{true};
};

struct NcarFourWheelCoreInitialization {
    NcarWheelVertexAnchorResult anchors{};
    std::array<std::optional<NcarWheelCoreInitialization>, 4> wheels{};
    bool input_complete{};
};

// Composes the observed 0x433C00 anchor scan and four 0x441C10 core setups.
// Missing mapped fields or any unavailable anchor leaves the batch incomplete.
[[nodiscard]] NcarFourWheelCoreInitialization initialize_ncar_four_wheel_core(
    const NcarFourWheelCoreInitializationInput& input) noexcept;

struct NcarWheelTireSetup {
    std::size_t wheel_index{};
    float loader_dimension_argument{};
    float tire_response_object_0x2c{};
    TireResponseObject response_object{};
};

// Reconstructs 0x441C10's axle setup scale and 0x435B50's division by TIRE
// float 0, then initializes the complete 0x40-byte response object.
[[nodiscard]] std::optional<NcarWheelTireSetup>
initialize_ncar_wheel_tire_setup(
    std::span<const std::byte> runtime_record,
    std::size_t wheel_index,
    const TireDefinition& tire,
    bool fdiv_compatibility_enabled) noexcept;

struct NcarIntegratorWheelScalarInput {
    std::uint32_t selector_bits_0x4f8{};
    float angle_0xdcc{};
    float angle_scale_0x500{};
    float value_0xd84{};
    float value_0xd90{};
    // When enabled, constructor-derived values replace the three explicit
    // object fields above; +0x500 remains supplied by interpolated state.
    bool derive_constructor_setup{};
    NcarIntegratorWheelCallbackSetupInput constructor_setup{};
    std::uint32_t value_bits_0x244{};
    PhysicsVector3 body_velocity_0x234{};
    float value_0xde4{};
    std::uint32_t value_bits_0xde8{};
};

struct NcarIntegratorWheelScalarResult {
    bool callback_0x4c_reached{};
    bool callback_selector_nonzero{};
    bool response_gate_passed{};
    bool negative_pair_gate_passed{};
    std::uint8_t adjusted_lane{0xffU};
    float speed_magnitude{};
    float tangent_response{};
    float target_response{};
    std::array<float, 4> seed_values{};
    std::array<float, 4> callback_values{};
    std::array<float, 4> wheel_values_0xe0{};
};

// Models the post-controller signal stores and bounded smoother at 0x431150.
// Virtual callback returns are supplied as observations; the two setter
// arguments are reported without emulating their external target objects.
[[nodiscard]] NcarIntegratorSignalResult advance_ncar_integrator_signal(
    NcarIntegratorSignalState& state,
    const NcarIntegratorSignalInput& input,
    std::uint8_t copied_flag_0xc3c) noexcept;

// Reconstructs the shared vtable +0x4C implementation at 0x432170, then
// applies its caller's optional sign reversal and four +0xE0 stores.
[[nodiscard]] NcarIntegratorWheelScalarResult
advance_ncar_integrator_wheel_scalars(
    const NcarIntegratorWheelScalarInput& input) noexcept;

// Reads the sourced 0x431150/+0x4C callback operands from a byte-backed body,
// computes 0x432170, and atomically stores its four wheel +0xE0 outputs.
// Missing or non-finite source fields leave all wheel records unchanged.
[[nodiscard]] std::optional<NcarIntegratorWheelScalarResult>
advance_ncar_integrator_wheel_scalars_from_body(
    const NcarBodyRuntimeObjectState& body,
    std::array<NcarWheelRuntimeObjectState, 4>& wheel_objects,
    std::uint32_t first_wheel_object_offset) noexcept;

struct NcarIntegratorCurveGateResult {
    bool curve_helper_0x4402d0_reached{};
    bool curve_byte_0x40_written{};
    bool parent_flag_0xc3c_written{};
    bool setter_0x4331a0_reached{};
};

// Models the guarded +0xBFC curve callback and following +0xDE0 setter.
[[nodiscard]] NcarIntegratorCurveGateResult
advance_ncar_integrator_curve_gate(
    NcarIntegratorCurveGateState& state,
    const NcarIntegratorCurveGateInput& input) noexcept;

struct NcarIntegratorChildControlStepInput {
    NcarIntegratorChildControl child{};
    NcarAnimationControllerTargets targets{};
    NcarAnimationControllerObservation observation{};
    std::uint32_t car_value_0xdec_bits{};
};

struct NcarIntegratorChildControlStepResult {
    NcarIntegratorChildControlResult setup{};
    NcarAnimationControllerResult controller{};
    NcarIntegratorChildControl child{};
    NcarAnimationControllerTargets targets{};
};

// Composes 0x43FF80, 0x43FF70, and 0x43FAE0 in their observed order.
[[nodiscard]] NcarIntegratorChildControlStepResult
advance_ncar_integrator_child_control_step(
    const NcarIntegratorChildControlStepInput& input,
    std::uint32_t car_value_0xde0_bits);

enum class FourWheelBodyStepInputGroup : std::uint32_t {
    wheel_runtime_state = 1U << 0U,
    contact_and_surface_state = 1U << 1U,
    integrator_callback_state = 1U << 2U,
    body_force_context = 1U << 3U,
    body_integration_state = 1U << 4U,
    embedded_record_state = 1U << 5U,
    virtual_record_state = 1U << 6U,
    attached_object_state = 1U << 7U,
};

[[nodiscard]] constexpr std::uint32_t four_wheel_body_step_group_bit(
    const FourWheelBodyStepInputGroup group) noexcept
{
    return static_cast<std::uint32_t>(group);
}

inline constexpr std::uint32_t four_wheel_body_step_core_input_groups =
    four_wheel_body_step_group_bit(FourWheelBodyStepInputGroup::wheel_runtime_state) |
    four_wheel_body_step_group_bit(FourWheelBodyStepInputGroup::contact_and_surface_state) |
    four_wheel_body_step_group_bit(FourWheelBodyStepInputGroup::integrator_callback_state) |
    four_wheel_body_step_group_bit(FourWheelBodyStepInputGroup::body_force_context) |
    four_wheel_body_step_group_bit(FourWheelBodyStepInputGroup::body_integration_state);

struct FourWheelBodyStepInput {
    // Mark a group only when its values have been resolved from current,
    // evidence-backed caller state. Default-zero aggregate initialization is
    // intentionally not treated as proof that a native input was resolved.
    std::uint32_t resolved_input_groups{};
    std::array<std::uint8_t, 4> wheel_damage_flags_0x20{};
    bool wheel_update_prefix_enabled{};
    std::array<WheelUpdatePrefixInput, 4> wheel_update_prefix_inputs{};
    bool wheel_spin_preprojection_enabled{};
    std::array<WheelSpinPreprojectionInput, 4> wheel_spin_preprojection_inputs{};
    // Final vectors presented to 0x43E7F0 when raw moment derivation is off.
    std::array<PhysicsVector3, 4> wheel_moment_vectors_0x258{};
    bool derive_wheel_moment_vectors{};
    std::array<WheelMomentVectorInput, 4> wheel_moment_vector_inputs{};
    bool wheel_contact_pipeline_enabled{};
    std::array<WheelContactState, 4> wheel_contact_states{};
    std::array<WheelUpdatePipelineInput, 4> wheel_update_pipeline_inputs{};
    std::array<WheelCollisionImpulseInput, 4> wheel_collision_inputs{};
    std::array<WheelAnimationBasisInput, 4> wheel_animation_basis_inputs{};
    // Inputs to the direct +0xF4/+0x100/+0x7C stores in each 0x442530 call.
    std::array<WheelTireDynamicsState, 4> wheel_tire_dynamics_states{};
    std::array<WheelTireDynamicsInput, 4> wheel_tire_dynamics_inputs{};
    bool wheel_tire_accumulators_enabled{};
    std::array<WheelTireAccumulatorState, 4> wheel_tire_accumulator_states{};
    std::array<WheelTireAccumulatorInput, 4> wheel_tire_accumulator_inputs{};
    // When set, the raw per-wheel TIRE definitions provide the curve set and
    // trigonometric coefficients used by response and dynamics stages.
    bool derive_wheel_tire_setup{};
    std::array<TireDefinition, 4> wheel_tire_definitions{};
    std::array<TireWheelCurveFactorInput, 4> wheel_tire_curve_factor_inputs{};
    std::array<WheelTireResponseVectorInput, 4> wheel_tire_response_vector_inputs{};
    std::array<WheelTireBasisUpdateInput, 4> wheel_tire_basis_update_inputs{};
    std::array<PhysicsVector3, 4> wheel_application_points_0xc8{};
    std::array<WheelSecondaryBodyForceInput, 4> wheel_secondary_body_forces{};
    std::array<WheelConditionalTireForceInput, 4> wheel_conditional_tire_force_inputs{};
    // response_local_0x90 is used when spin preprojection is disabled; when
    // enabled, that stage supplies the source local before vector projection.
    std::array<WheelRateFinalizationInput, 4> wheel_rate_finalization_inputs{};
    NcarIntegratorBookkeepingState integrator_bookkeeping_state{};
    NcarIntegratorBookkeepingInput integrator_bookkeeping_input{};
    NcarIntegratorPrefixInput integrator_prefix_input{};
    NcarIntegratorTimerState integrator_timer_state{};
    NcarIntegratorTimerInput integrator_timer_input{};
    NcarIntegratorCurveGateState integrator_curve_gate_state{};
    NcarIntegratorCurveGateInput integrator_curve_gate_input{};
    NcarIntegratorSignalState integrator_signal_state{};
    NcarIntegratorSignalInput integrator_signal_input{};
    NcarIntegratorWheelScalarInput integrator_wheel_scalar_input{};
    NcarIntegratorChildControlStepInput integrator_child_control_input{};
    NcarIntegratorModeForceInput mode_force_input{};
    NcarIntegratorWheelPairDifferentialInput wheel_pair_differential_input{};
    NcarIntegratorCommonForceInput common_force_input{};
    NcarIntegratorVelocityForceInput velocity_force_input{};
    bool attached_object_force_gate_0xbe4{};
    std::array<NcarAttachedObjectForceInput, 4> attached_object_force_inputs{};
    float attached_object_tuning_0x500{};
    float attached_object_tuning_0x508{};
    std::array<WheelAnimationTimerState, 4> wheel_animation_states{};
    VehicleAnimationChannelState vehicle_animation_channel_0x18{};
    VehicleAnimationChannelState vehicle_animation_channel_0x5c{};
    float animation_current_time{};
    BodyIntegrationTailState body_tail_state{};
    NcarPointImpulseAccumulator point_impulse_state{};
    // Carries the force/moment sums produced by the four wheel updates into
    // the counted embedded-record pass and then body force collection.
    NcarForceMomentAccumulator force_moment_state{};
    float energy_accumulator_0xe40{};
    std::span<EmbeddedRecordState> embedded_record_states;
    std::span<const PhysicsVector3> embedded_record_local_points_0x00;
    std::size_t embedded_record_count_0x7b{};
    const TerrainSurfaceResourcePayload* terrain{};
    EmbeddedRecordUpdateInput embedded_record_body_input{};
    bool compatibility_division_enabled{};
    std::span<const float> packed_virtual_records;
    BodyIntegrationKinematicsInput kinematics{};
    NcarPostBodyState post_body_state{};
    NcarPostBodyInput post_body_input{};
};

struct FourWheelBodyStepResult {
    FourWheelCollisionImpulseResult wheel_collisions{};
    std::array<WheelUpdatePrefixResult, 4> wheel_update_prefix_results{};
    std::array<PhysicsVector3, 4> wheel_moment_vectors_applied{};
    std::array<WheelMomentVectorResult, 4> wheel_moment_vector_results{};
    // +0xB0/+0xB4/+0xB8 written by this step's later 0x443F60 call, for use
    // as the pre-update local axis on the next wheel step.
    std::array<PhysicsVector3, 4> wheel_moment_local_axes_next_0xb0{};
    std::array<WheelContactState, 4> wheel_contact_states{};
    std::array<WheelUpdatePipelineResult, 4> wheel_contact_pipeline_results{};
    std::array<PhysicsVector3, 4> wheel_point_velocities_0x43e9b0{};
    std::array<WheelSpinPreprojectionResult, 4> wheel_spin_preprojection_results{};
    std::array<PhysicsVector3, 4> wheel_application_points_applied_0xc8{};
    std::array<bool, 4> wheel_suspension_force_accumulated{};
    std::array<WheelAnimationBasisState, 4> wheel_animation_basis_states{};
    std::array<WheelBodyForceSequenceResult, 4> wheel_body_forces{};
    std::array<WheelTireDynamicsResult, 4> wheel_tire_dynamics_results{};
    std::array<WheelTireDynamicsState, 4> wheel_tire_dynamics_states{};
    std::array<TireWheelCurveFactorInput, 4>
        wheel_tire_curve_factor_inputs_applied{};
    std::array<WheelTireAccumulatorResult, 4> wheel_tire_accumulator_results{};
    std::array<WheelTireAccumulatorState, 4> wheel_tire_accumulator_states{};
    std::array<TireResponseCurveSet, 4> wheel_tire_curves{};
    std::array<PhysicsVector3, 4> wheel_tire_response_vectors{};
    std::array<float, 4> wheel_rate_response_local_0x90{};
    std::array<WheelTireBasisUpdateResult, 4> wheel_tire_basis_updates{};
    std::array<WheelConditionalTireForceResult, 4> wheel_conditional_tire_forces{};
    std::array<WheelRateFinalizationResult, 4> wheel_rate_finalization_results{};
    NcarIntegratorBookkeepingResult integrator_bookkeeping{};
    NcarIntegratorPrefixResult integrator_prefix{};
    NcarIntegratorTimerResult integrator_timer{};
    NcarIntegratorTimerState integrator_timer_state{};
    NcarIntegratorEarlyBoundsResult integrator_early_bounds{};
    NcarIntegratorCurveGateResult integrator_curve_gate{};
    NcarIntegratorCurveGateState integrator_curve_gate_state{};
    NcarIntegratorChildControlStepResult integrator_child_control{};
    NcarIntegratorSignalResult integrator_signal{};
    NcarIntegratorSignalState integrator_signal_state{};
    NcarIntegratorWheelScalarResult integrator_wheel_scalars{};
    NcarIntegratorModeForceResult mode_force{};
    NcarIntegratorWheelPairDifferentialResult wheel_pair_differentials{};
    bool wheel_pair_history_derived_from_updates{};
    NcarIntegratorCommonForceResult common_force{};
    NcarIntegratorVelocityForceResult velocity_forces{};
    std::vector<NcarIntegratorVirtualRecordCallbackResult>
        integrator_virtual_record_callbacks;
    std::array<float, 4> attached_object_targets_0x38{};
    std::array<NcarAttachedObjectForceResult, 4> attached_object_forces{};
    std::optional<VehicleAnimationFrameResult> periodic_animation;
    EmbeddedRecordBatchUpdateResult embedded_records{};
    BodyIntegrationForceCollectionResult force_collection{};
    BodyIntegrationKinematicsResult kinematics{};
    NcarPostBodyResult post_body{};
    NcarPostBodyState post_body_state{};
    BodyIntegrationTailState body_tail_state{};
    NcarPointImpulseAccumulator point_impulse_state{};
    NcarIntegratorBookkeepingState integrator_bookkeeping_state{};
    std::array<WheelAnimationTimerState, 4> wheel_animation_states{};
    VehicleAnimationChannelState vehicle_animation_channel_0x18{};
    VehicleAnimationChannelState vehicle_animation_channel_0x5c{};
    NcarForceMomentAccumulator force_moment_state{};
    float energy_accumulator_0xe40{};
    bool input_complete{true};
    std::uint32_t required_input_groups{};
    std::uint32_t missing_input_groups{};
};

// Composes the 0x42F100 solve, conditional wheel damage-byte store, and
// 0x43E830 point-impulse accumulator call from the 0x442530 wheel update.
[[nodiscard]] WheelCollisionImpulseApplicationResult
apply_wheel_collision_impulse(
    NcarPointImpulseAccumulator& accumulator,
    std::uint8_t& wheel_damage_flag_0x20,
    const WheelCollisionImpulseInput& input) noexcept;

// Applies the four per-wheel collision gates in the 0x431150 wheel order to
// one shared body impulse accumulator.
[[nodiscard]] FourWheelCollisionImpulseResult apply_four_wheel_collision_impulses(
    NcarPointImpulseAccumulator& accumulator,
    const std::array<std::uint8_t, 4>& wheel_damage_flags_0x20,
    const std::array<WheelCollisionImpulseInput, 4>& inputs) noexcept;

// Composes four wheel impulse collection, the counted embedded-record contact
// loop, 0x43ECE0 force collection, and body kinematics. Unresolved virtual
// callback accumulator effects remain explicit inputs.
[[nodiscard]] FourWheelBodyStepResult advance_four_wheel_body_step(
    const FourWheelBodyStepInput& input);

} // namespace viper
