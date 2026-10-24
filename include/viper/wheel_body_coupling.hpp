#pragma once

#include "viper/ncar_state.hpp"
#include "viper/vehicle_setup.hpp"

namespace viper {

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

// Projects two paired wheel differences into the four observed car fields.
[[nodiscard]] NcarIntegratorWheelPairDifferentialResult
apply_ncar_integrator_wheel_pair_differentials(
    const NcarIntegratorWheelPairDifferentialInput& input) noexcept;

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
    std::array<std::uint32_t, 8> timeout_values_0x48e{};
    float timeout_start_0x488{};
    std::array<std::uint32_t, 5> resource_descriptor_0x10{};
    std::array<std::uint8_t, 5> resource_flag_0x15{};
};

struct NcarPostBodyInput {
    // 0xD88 vector transformed by the post-0x43ECE0 orientation matrix. The
    // optional derivation is enabled by the composed four-wheel body step.
    std::array<float, 3> source_vector_0xd88{};
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

struct NcarIntegratorTimerInput {
    // Raw global counter consumed by 0x425790, scaled by the binary's 0.016f.
    std::uint32_t timer_ticks_0x508774{};
    std::uint32_t vehicle_ordinal_0x510{};
    // Distinguish a known null record from an unavailable table lookup.
    bool record_lookup_resolved{};
    bool record_nonnull{};
    float record_x_0x04{};
    float record_y_0x08{};
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
    const NcarIntegratorTimerInput& input) noexcept;

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

struct FourWheelBodyStepInput {
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
    float gravity_mass_0x7c{};
    std::span<const float> packed_virtual_records;
    // Net mutations made by the duration-gated virtual +0x38 callbacks. Their
    // per-record behavior is not recovered, so the six aggregate deltas stay
    // an observation.
    std::array<float, 6> virtual_record_accumulator_delta{};
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
    NcarIntegratorModeForceResult mode_force{};
    NcarIntegratorWheelPairDifferentialResult wheel_pair_differentials{};
    NcarIntegratorCommonForceResult common_force{};
    NcarIntegratorVelocityForceResult velocity_forces{};
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
