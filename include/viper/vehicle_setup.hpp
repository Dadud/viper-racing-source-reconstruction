#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "viper/terrain_surface.hpp"
#include "viper/ncar_state.hpp"

namespace viper {

struct HumanCarSetupFile {
    static constexpr std::size_t active_setup_size = 0x8cU;
    static constexpr std::size_t file_size = 0xd4U;
    std::array<std::byte, active_setup_size> active_setup{};
    std::uint32_t version{};
    bool version_warning{};
};

// Parses the exact-size per-user .csu file read by 0x45EE40. The first 0x8C
// bytes are copied into the active setup; a non-1 version warns but succeeds.
[[nodiscard]] std::optional<HumanCarSetupFile> parse_human_car_setup_file(
    std::span<const std::byte> payload) noexcept;

struct TireDefinition {
    std::string label;
    std::array<float, 22> parameters{};
};

struct TireResponseObject {
    // The 0x435B50/0x435BF0 allocation is 0x40 bytes (16 DWORDs).
    // Keep destination slots numeric; their physical meanings are unproven.
    std::array<float, 16> words{};
};

// Reconstructs the 0x435BF0 tire-object stores from a parsed 120-byte TIRE
// record. The 0x2C slot is the caller-supplied value computed by 0x435B50.
[[nodiscard]] TireResponseObject initialize_tire_response_object(
    const TireDefinition& tire, float supplied_value_0x2c) noexcept;

struct TireQuadraticTerms {
    float quadratic{};
    float linear{};
};

struct TireLinearTerms {
    float slope{};
    float intercept{};
};

struct TireResponseCurveSet {
    TireQuadraticTerms quadratic;
    TireLinearTerms linear;
    float quadratic_scale_at_0x10{};
    float quadratic_scale_at_0x14{};
    float linear_scale_at_0x18{};
    float input_limit_at_0x38{};
    float input_slope_at_0x3c{};
};

struct TireSlipCurveEvaluation {
    float signed_input{};
    float clamped_input{};
    float normalized_output{};
};

struct TireCurveEvaluation {
    float quadratic{};
    float quadratic_scaled_at_0x10{};
    float quadratic_scaled_at_0x14{};
    float linear{};
    float linear_scaled_at_0x18{};
};

struct TireTrigonometricTerms {
    float coefficient_0x1c{};
    float coefficient_0x20{};
    float coefficient_0x24{};
    float coefficient_0x28{};
};

struct WheelTireDynamicsState {
    float filtered_component_0x170{};
    float filtered_component_0x174{};
    float instantaneous_component_0x180{};
    float response_gate_0x11c{};
    float suspension_response_0x178{};
    float spin_filter_0x17c{};
};

struct WheelTireDynamicsInput {
    float instantaneous_component_0x2c{};
    float instantaneous_component_0x30{};
    float secondary_slip_component_0xe4{};
    float smoothing_gate_value_0xec{};
    TireTrigonometricTerms tire_terms{};
    bool contact_active{};
    bool clear_filter_state{};
};

struct WheelTireDynamicsResult {
    float smoothing_weight{};
    float second_blend_weight{};
    float combined_slip_magnitude{};
    float trigonometric_response{};
};

struct WheelTireAccumulatorInput {
    float accumulator_0xa4{};
    float accumulator_0x10c{};
    float state_0xec{};
    float state_0xf0{};
    float response_gate_0x11c{};
    float sign_source_0x04{};
    float forward_response_local_0x38{};
    float lateral_response_local_0x2c{};
    float divisor_0x1c{};
    float divisor_factor_0x48{};
    float divisor_factor_0xc0{};
    float forward_accumulator_scale{};
    float signed_lateral_scale{};
    float decay_0x10c{};
    float forward_scale_0x10c{};
    bool contact_active_0x168{};
    bool fdiv_compatibility_enabled{};
};

struct WheelTireAccumulatorResult {
    float accumulator_0xa4{};
    float accumulator_0x10c{};
    float state_0xec{};
    float state_0xf0{};
    float sign_0x04{};
    bool contact_states_updated{};
    bool fdiv_compatibility_path{};
};

struct WheelTireAccumulatorState {
    float accumulator_0xa4{};
    float accumulator_0x10c{};
    float state_0xec{};
    float state_0xf0{};
};

// Applies tire-response state stores at 0x442530: 0x004438F3..0x0044399B.
[[nodiscard]] WheelTireAccumulatorResult advance_wheel_tire_accumulators(
    const WheelTireAccumulatorInput& input) noexcept;

struct WheelRateFinalizationInput {
    float rate_0x124{};
    float blend_local_0x28{};
    float wheel_scalar_0x38{};
    float response_local_0x90{};
    float rate_increment_scale{};
    float auxiliary_rate_scale{};
    std::uint8_t damage_flag_0x20{};
    std::uint8_t animation_flags_0x120{};
};

struct WheelRateFinalizationResult {
    float rate_0x124{};
    float auxiliary_rate_0x128{};
    std::uint8_t animation_flags_0x120{};
};

struct WheelSpinPreprojectionInput {
    float spin_rate_0x124{};
    float wheel_scalar_0x5c{};
    float value_0xa8{};
    float value_0x50{};
    float value_0x54{};
    float value_0x58{};
    float value_0xe0{};
    float response_value_0x178{};
    float previous_filter_0x17c{};
    float numerator_0xe4{};
    float state_0x174{};
    float state_0x170{};
    float high_speed_projection{};
    std::int32_t mode_0x15c{};
    std::int32_t selector_0x164{};
    std::uint8_t damage_flag_0x20{};
    bool use_precomputed_value_0xa8{};
    bool gate_0x160{};
};

struct WheelSpinPreprojectionResult {
    float spin_rate_0x124{};
    float auxiliary_0x12c{};
    float filter_0x17c{};
    float response_local_0x90{};
    float blend_local_0x28{};
};

// Reconstructs the early spin/filter block in 0x442530 before contact query.
[[nodiscard]] WheelSpinPreprojectionResult update_wheel_spin_preprojection(
    const WheelSpinPreprojectionInput& input) noexcept;

// Applies the final rate, auxiliary-rate, and damage-animation writes in 0x442530.
[[nodiscard]] WheelRateFinalizationResult finalize_wheel_rate_state(
    const WheelRateFinalizationInput& input) noexcept;

struct WheelAnimationTimerState {
    float field_0x12c{};
    float field_0x188{};
    float field_0x190{};
    float field_0x194{};
    float field_0x198{};
};

struct WheelAnimationTimerResult {
    float pending_0x198{};
    float field_0x194{};
    float field_0x198{};
};

// Models 0x443F10; current_time is the value returned by 0x4257B0.
[[nodiscard]] WheelAnimationTimerResult advance_wheel_animation_timer(
    WheelAnimationTimerState& state,
    float current_time,
    float parameter) noexcept;

struct FourWheelAnimationTimerResult {
    std::array<WheelAnimationTimerResult, 4> wheel_results{};
    bool call_0x440440_reached{true};
};

// Models the four 0x443F10 calls made by 0x4323E0 and records its trailing call.
[[nodiscard]] FourWheelAnimationTimerResult advance_four_wheel_animation_timers(
    std::array<WheelAnimationTimerState, 4>& wheel_states,
    float current_time,
    float parameter) noexcept;

struct VehicleAnimationChannelState {
    float rate{};
    float integration_scale{};
    float elapsed{};
    float pending{};
};

struct VehicleAnimationUpdateResult {
    VehicleAnimationChannelState channel_0x18{};
    VehicleAnimationChannelState channel_0x5c{};
};

// Models the two animation channels updated by 0x440440 on the vehicle block at +0xBFC.
[[nodiscard]] VehicleAnimationUpdateResult advance_vehicle_animation_channels(
    VehicleAnimationChannelState& channel_0x18,
    VehicleAnimationChannelState& channel_0x5c,
    float current_time,
    float parameter) noexcept;

struct VehicleAnimationFrameResult {
    FourWheelAnimationTimerResult wheels{};
    VehicleAnimationUpdateResult vehicle{};
};

// Models the complete 0x4323E0 ordering: four wheel records, then the vehicle block.
[[nodiscard]] VehicleAnimationFrameResult advance_vehicle_animation_frame(
    std::array<WheelAnimationTimerState, 4>& wheel_states,
    VehicleAnimationChannelState& channel_0x18,
    VehicleAnimationChannelState& channel_0x5c,
    float current_time,
    float parameter) noexcept;

// The post-suspension contact-feature dispatch in 0x442530 writes these
// animation bytes and two response scalars. Feature codes stay raw.
struct WheelContactFeatureProfileInput {
    std::int32_t feature_0x19c{};
    float response_gate_0x11c{};
    bool reset_contact{};
    bool contact_active{};
};

struct WheelContactFeatureProfile {
    std::uint8_t animation_flags_0x120{};
    std::uint8_t animation_mode_0x121{};
    float response_scale{1.0F};
    float secondary_response{};
    bool clear_fields_0xec_0xf0_0x11c{};
};

struct WheelContactFeatureState {
    std::uint8_t animation_flags_0x120{};
    std::uint8_t animation_mode_0x121{};
    float state_0xec{};
    float state_0xf0{};
    float response_gate_0x11c{};
};

[[nodiscard]] WheelContactFeatureProfile classify_wheel_contact_feature(
    const WheelContactFeatureProfileInput& input) noexcept;
void apply_wheel_contact_feature_profile(
    WheelContactFeatureState& state,
    const WheelContactFeatureProfile& profile) noexcept;

struct WheelAngleAccumulatorInput {
    float angle_0xac{};
    float rate_0x124{};
    float increment_scale{};
    float positive_wrap_threshold{};
    float wrap_delta{};
    float negative_wrap_threshold{};
};

// Models the direct +0xAC update and one-step wrap in 0x442530.
[[nodiscard]] float advance_wheel_angle_accumulator(
    const WheelAngleAccumulatorInput& input) noexcept;

struct WheelAnimationBasisState {
    float angle_0xd4{};
    float sine_0xb0{};
    float zero_0xb4{};
    float cosine_0xb8{};
    float offset_0xd8{};
    std::array<float, 3> basis_0x110_0x118{};
};

struct WheelAnimationBasisInput {
    float value_0x04{};
    float value_0x14{};
    float steering_angle_0xac{};
    float scalar_0x10c{};
    float depth_limit_0x44{};
    float contact_depth_0xe8{};
    float value_0x60{};
    float value_0x64{};
    float value_0x68{};
    float value_0x6c{};
    float value_0x70{};
    float value_0x74{};
    float value_0x78{};
    float value_0x7c{};
    std::array<float, 3> orientation_0x80_0x88{};
    float value_0xdc{};
    // Raw body basis values ordered by offsets +0x38,+0x3C,...,+0x58.
    std::array<float, 9> body_basis_0x38_to_0x58{};
    bool damage_flag_0x20{};
};

struct WheelPiecewiseTerms {
    float positive_slope{};
    float positive_intercept{};
    float nonpositive_slope{};
    float nonpositive_intercept{};
};

struct WheelKinematicRatios {
    float lateral_velocity_ratio{};
    float rolling_speed_mismatch_ratio{};
};

struct TireWheelResponseFactors {
    float trigonometric_response{}; // 0x435780 result (local_e4)
    float forward_ratio{};          // local_74
    float lateral_ratio{};          // local_9c
    float ratio_normalization{};    // local_e8
    float lateral_blend{};          // local_d4
    float lateral_direction_factor{}; // local_e0 before response assembly
    float lateral_correction{};     // local_70
    float low_speed_weight{};       // local_98
    float curve_input{};            // local_c4
    float forward_curve_value{};    // local_50
    float lateral_curve_value{};    // local_4c
};

struct TireWheelCurveFactorInput {
    TireResponseCurveSet curves{};
    // Three coordinate-ordered terms from (suspension response + wheel +0x100
    // response) projected on the observed contact-normal basis.
    std::array<float, 3> suspension_response_components{};
    std::array<float, 3> wheel_response_components{};
    std::array<float, 3> contact_normal_components{};
    float wheel_scalar_0x90{};
    float lateral_velocity{};
    float longitudinal_velocity{};
    float rolling_surface_speed{};
    // Optional raw wheel fields used by 0x442530 after its earlier spin update.
    float wheel_spin_rate_0x124{};
    float wheel_scalar_0x5c{};
    bool derive_rolling_surface_speed{};
    std::uint32_t slip_direction_sign_bits{};
    float slip_curve_input{};
    float tire_scale_0x98{};
    float wheel_scale_0x94{};
    float mode_scale_0xf0{};
    float reference_speed_scale_0x1c{1.0F};
    bool use_reference_speed_scale_0x1c{};
};

struct TireWheelResponseDerivation {
    TireWheelResponseFactors factors{};
    float dynamics_component_0x2c{};
    float dynamics_component_0x30{};
    float secondary_slip_component_0xe4{};
    bool dynamics_division_valid{};
};

struct TireWheelResponseComponents {
    float forward{};
    float lateral{};
};

struct PhysicsVector3 {
    float x{};
    float y{};
    float z{};
};

struct BodyPointVelocityInput {
    PhysicsVector3 body_center_0x5c{};
    PhysicsVector3 point_0x130{};
    PhysicsVector3 linear_velocity_0x234{};
    PhysicsVector3 angular_velocity_0x240{};
};

struct WheelMomentVectorInput {
    // Car matrix (+0x38..+0x58) consumed by FUN_00422F30 and its +0x44 axis.
    std::array<float, 9> body_basis_0x38_to_0x58{};
    // Wheel-local vector at +0xB0..+0xB8.
    PhysicsVector3 wheel_local_vector_0xb0{};
    float spin_rate_0x124{};
    float previous_spin_rate_0x13c{};
    std::array<float, 3> spin_filter_0x140_to_0x148{};
    float wheel_scale_0x34{};
};

struct WheelMomentVectorResult {
    PhysicsVector3 transformed_wheel_axis{};
    PhysicsVector3 wheel_axis_cross_car_axis{};
    PhysicsVector3 moment_vector_0x258{};
    float next_spin_rate_0x13c{};
};

// Evaluates the body's linear plus angular velocity at a world-space point,
// matching 0x43E9B0 as called by the wheel update.
[[nodiscard]] PhysicsVector3 derive_body_point_velocity_0x43e9b0(
    const BodyPointVelocityInput& input) noexcept;
[[nodiscard]] WheelMomentVectorResult derive_wheel_moment_vector_0x43e7f0(
    const WheelMomentVectorInput& input) noexcept;

struct BodyPointTransformInput {
    PhysicsVector3 local_point{};
    std::array<float, 9> basis_0x38_to_0x58{};
    PhysicsVector3 center_0x5c{};
};

// Models 0x437BA0's local-point to world-point transform using the car basis
// at +0x38..+0x58 and world center at +0x5C..+0x64.
[[nodiscard]] PhysicsVector3 transform_body_local_point(
    const BodyPointTransformInput& input) noexcept;

struct EmbeddedRecordContactInput {
    PhysicsVector3 record_world_point{};
    PhysicsVector3 terrain_point{};
    PhysicsVector3 terrain_normal{};
    PhysicsVector3 point_velocity{};
    float response_scalar_0x18{};
    float previous_penetration_0x1c{};
    std::int32_t terrain_feature{};
};

// Adapts the recovered terrain-resource query output into the embedded-record
// response inputs. Failed or invalid-plane queries have no contact response.
[[nodiscard]] std::optional<EmbeddedRecordContactInput>
make_embedded_record_contact_input(
    const TerrainSurfaceQueryResult& terrain_query,
    const PhysicsVector3& record_world_point,
    const PhysicsVector3& point_velocity,
    float response_scalar_0x18,
    float previous_penetration_0x1c) noexcept;

struct EmbeddedRecordContactResponse {
    float penetration{};
    float normal_response_limit{};
    float friction_magnitude_before_cap{};
    float friction_magnitude_after_cap{};
    bool special_feature_0xe{};
    bool penetration_increased{};
    bool large_force_effect_requested{};
    PhysicsVector3 spring_force{};
    PhysicsVector3 velocity_force{};
    PhysicsVector3 force_before_virtual_callback{};
};

struct EmbeddedRecordForceApplicationResult {
    float body_speed_squared{};
    float energy_scale{};
    float added_energy_0xe40{};
    bool energy_accumulator_updated{};
    bool force_moment_accumulator_updated{};
};

struct EmbeddedRecordAuxiliaryEffectPlan {
    std::uint32_t event_type{1U};
    std::uint32_t payload_identifier{0x4d2U};
    std::uint32_t payload_size{0x14U};
    PhysicsVector3 payload_vector{};
    float force_scale{};
    bool optional_effect_callback_requested{};
};

struct EmbeddedRecordAuxiliaryEffectState {
    PhysicsVector3 cached_vector_0x1c{};
    bool effect_target_present_0x0c{};
    float target_scale_0x08{};
    bool target_scale_changed_gate_0x2c{};
    bool target_enabled_0x29{};
    bool target_flag_0x2a{};
    bool target_flag_0x2b{};
};

struct EmbeddedRecordAuxiliaryEffectUpdate {
    bool target_present{};
    bool scale_changed{};
    bool cache_written{};
    bool target_flags_reset{};
};

struct EmbeddedRecordEffectEventAppendResult {
    bool enqueued{};
    bool storage_range_valid{};
    bool capacity_exceeded{};
    bool overflow_warning_emitted{};
    std::uint32_t used_bytes_after{};
    std::int32_t last_overflow_warning_tick_after{};
};

// Models 0x436030's confirmed 0x4D2 event payload and optional +0x0C callback.
// Vector meanings are kept neutral because the outer 0x4361E0 call mapping is
// unresolved; inputs are the exact vectors passed to this helper.
[[nodiscard]] EmbeddedRecordAuxiliaryEffectPlan plan_embedded_record_auxiliary_effect(
    const PhysicsVector3& first_vector,
    const PhysicsVector3& second_vector,
    bool effect_slot_0x0c_nonzero) noexcept;

// Applies 0x435FE0's state writes when the optional +0x0C target exists.
[[nodiscard]] EmbeddedRecordAuxiliaryEffectUpdate apply_embedded_record_auxiliary_effect(
    EmbeddedRecordAuxiliaryEffectState& state,
    const PhysicsVector3& first_vector,
    float force_scale) noexcept;

// Serializes the 0x4D2 helper payload through the observed 0x4273C0 byte
// stream. Buffer selection/base offset are caller observations from globals.
[[nodiscard]] EmbeddedRecordEffectEventAppendResult append_embedded_record_effect_event(
    std::span<std::byte> active_event_buffer,
    std::size_t buffer_base_offset,
    std::uint32_t used_bytes,
    std::int32_t now_tick,
    std::int32_t last_overflow_warning_tick,
    const EmbeddedRecordAuxiliaryEffectPlan& effect) noexcept;

// Implements the recovered PCAR/base +0x34 target (0x430560) and NCAR
// forwarding thunk (0x43A280): velocity-gated energy bookkeeping followed by
// the shared 0x43E6F0 force and moment accumulation.
[[nodiscard]] EmbeddedRecordForceApplicationResult apply_embedded_record_force_to_body(
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const PhysicsVector3& body_linear_velocity,
    const PhysicsVector3& response_force,
    const PhysicsVector3& application_point,
    const PhysicsVector3& body_center) noexcept;

struct EmbeddedRecordState {
    PhysicsVector3 world_point_cache_0x0c{};
    float response_scalar_0x18{};
    float previous_penetration_0x1c{};
    PhysicsVector3 local_point_0x20{};
    bool copy_body_center_gate_0x2c{};
};

struct EmbeddedRecordTailInput {
    PhysicsVector3 body_center_0x5c{};
    std::array<float, 9> body_basis_0x38_to_0x58{};
    bool terrain_contact_found{};
    float virtual_response_force_x{};
};

struct EmbeddedRecordUpdateInput {
    NcarPointTransformFields body_motion{};
    std::array<float, 9> body_basis_0x38_to_0x58{};
    // Query coordinates are supplied from the caller's recovered stack setup.
    TerrainSurfaceQueryResult terrain_query{};
    bool contact_point_override_enabled{};
    PhysicsVector3 contact_point_override{};
};

struct EmbeddedRecordUpdateResult {
    PhysicsVector3 world_point{};
    bool terrain_contact_found{};
    EmbeddedRecordContactResponse contact_response{};
    EmbeddedRecordForceApplicationResult body_application{};
};

struct EmbeddedRecordBatchUpdateResult {
    std::size_t requested_records{};
    std::size_t processed_records{};
    std::size_t unprocessed_records{};
    bool input_complete{};
    std::vector<std::size_t> record_offsets_0x00;
    std::vector<EmbeddedRecordUpdateResult> record_results;
};

// Applies 0x4473D0's post-contact writes: optional center copy, cached
// body-local-to-world point transform, and the hit/miss update of +0x1C.
[[nodiscard]] PhysicsVector3 finish_embedded_record_update(
    EmbeddedRecordState& state,
    const EmbeddedRecordTailInput& input) noexcept;

// Composes one observed 0x4473D0 record update from its local point through
// terrain contact, force preparation, the resolved PCAR/NCAR +0x34 body
// callback, and persistent hit/miss stores. Terrain query coordinates remain
// caller-owned because the original stack-argument mapping is unresolved.
[[nodiscard]] EmbeddedRecordUpdateResult update_embedded_record_contact(
    EmbeddedRecordState& record_state,
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const EmbeddedRecordUpdateInput& input) noexcept;

// Builds 0x4473D0's terrain segment from record +0x20 (Y + 10) to the
// transformed record point supplied from +0x00, queries parsed track terrain,
// then composes the same contact and state-update path.
[[nodiscard]] EmbeddedRecordUpdateResult update_embedded_record_contact_from_terrain(
    EmbeddedRecordState& record_state,
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const EmbeddedRecordUpdateInput& input,
    const TerrainSurfaceResourcePayload& terrain,
    const PhysicsVector3& record_local_point_0x00,
    bool compatibility_division_enabled) noexcept;

// Processes the counted embedded-record array visited by 0x43ECE0, in its
// observed 0x30-byte row order, applying each record's terrain/contact effects
// to the shared energy and force/moment accumulators.
[[nodiscard]] EmbeddedRecordBatchUpdateResult
update_embedded_record_contact_batch_from_terrain(
    std::span<EmbeddedRecordState> record_states,
    std::span<const PhysicsVector3> record_local_points_0x00,
    std::size_t record_count_0x7b,
    float& energy_accumulator_0xe40,
    NcarForceMomentAccumulator& force_moment_accumulator,
    const EmbeddedRecordUpdateInput& body_input,
    const TerrainSurfaceResourcePayload& terrain,
    bool compatibility_division_enabled);

// Reconstructs the spring and velocity-dependent force prepared by 0x4473D0
// before its car vtable +0x34 callback. Contact geometry and point velocity
// remain explicit inputs; the virtual callback's force mutation is not modeled.
[[nodiscard]] EmbeddedRecordContactResponse prepare_embedded_record_contact(
    const EmbeddedRecordContactInput& input) noexcept;

struct WheelUpdatePrefixInput {
    float rate_0x124{};
    float wheel_scalar_0x5c{};
    bool query_flag_nonzero_0x50878c{};
    PhysicsVector3 reference_point{};
    PhysicsVector3 wheel_position_0x14{};
    float squared_distance_threshold{};
    bool damage_flag_0x20{};
    float angle_0xac{};
    float angle_increment_scale{};
    float positive_angle_threshold{};
    float angle_wrap_delta{};
    float negative_angle_threshold{};
};

struct WheelUpdatePrefixResult {
    float value_0xa8{};
    float angle_0xac{};
    bool damage_flag_0x20{};
    bool distance_gate_ran{};
    bool damage_flag_set{};
};

// Models the observed entry prefix of 0x442530; global 0x50878C is an input.
[[nodiscard]] WheelUpdatePrefixResult advance_wheel_update_prefix(
    const WheelUpdatePrefixInput& input) noexcept;

struct WheelRateBlendInput {
    float rate_0x124{};
    float rate_magnitude_0x124{};
    float absolute_value_0xe0{};
    float signed_response_local{};
    float rate_gate_0x41100000{};
    float rate_coefficient{};
    float blend_coefficient{};
    float response_rate_coefficient{};
};

struct WheelRateBlendResult {
    float blend{};
    float signed_response_local{};
    float rate_0x124{};
    bool branch_taken{};
};

// Models the conditional rate/residual blend at 0x442530: 0x00442805..0x0044286D.
[[nodiscard]] WheelRateBlendResult adjust_wheel_rate_blend(
    const WheelRateBlendInput& input) noexcept;

struct WheelExcessWeightInput {
    bool response_path_reached{};
    float excess_0xc4{};
    float weight_0x2c{};
    float excess_coefficient{};
    float upper_limit{};
};

struct WheelExcessWeightResult {
    float weight_0x2c{};
    bool applied{};
};

// Models the positive-excess weight update at 0x442530: 0x004429AB..0x004429DF.
[[nodiscard]] WheelExcessWeightResult adjust_wheel_excess_weight(
    const WheelExcessWeightInput& input) noexcept;

struct WheelPointTransformInput {
    PhysicsVector3 wheel_position_0x04_0x0c{};
    PhysicsVector3 referenced_point_0x10{};
    // Twelve floats in binary offset order +0x38 through +0x64.
    std::array<float, 12> body_transform_0x38_to_0x64{};
};

// Projects the caller's wheel point into the body's affine frame (+0xC8..+0xD0).
[[nodiscard]] PhysicsVector3 transform_wheel_point_0xc8(
    const WheelPointTransformInput& input) noexcept;

struct WheelContactState {
    PhysicsVector3 contact_point_0x130{};
    std::int32_t contact_feature_0x19c{};
    std::uint8_t contact_active_0x168{};
};

struct WheelContactObservation {
    bool body_query_enabled{};
    bool query_succeeded{};
    bool reset_feature_hook_active{};
    // Origin for projecting terrain contact depth; the transformed caller path uses +0xC8.
    PhysicsVector3 contact_projection_origin{};
    PhysicsVector3 contact_direction{};
    PhysicsVector3 query_contact_point{};
    PhysicsVector3 query_contact_normal{};
    std::int32_t query_feature_output_0x19c{};
    float wheel_depth_limit_0x44{};
};

struct WheelContactResult {
    bool reached_query{};
    bool contact_gate_passed{};
    bool reset_feature_id{};
    bool reached_contact_callback_0x443f00{};
    bool active{};
    float direction_alignment{};
    float projected_depth{};
    float clamped_contact_depth{};
};

struct WheelSuspensionInput {
    WheelPiecewiseTerms piecewise_terms{};
    float contact_depth{};
    float previous_contact_depth_0xe8{};
    float wheel_depth_limit_0x44{};
    float spring_scale_0x3c{};
    float spring_base_0x40{};
    float suspension_base_0x158{};
    float suspension_threshold_0x4c{};
    bool subtract_damage_adjustment_0x20{};
};

struct WheelSuspensionResult {
    float scaled_travel{};
    float threshold{};
    float threshold_adjustment{};
    float piecewise_response{};
    float spring_adjustment{};
    float combined_response_0x178{};
};

struct WheelSuspensionSurfaceInput {
    float suspension_response{};
    float signed_motion_projection{};
    float coefficient_0xa0{};
    float coefficient_0x9c{};
    float environment_sign{};
    std::int32_t contact_feature_0x19c{};
    bool special_surface_scaling_disabled_0x1a0{};
};

struct WheelSuspensionSurfaceResult {
    float absolute_motion_projection{};
    float motion_weight{};
    float signed_surface_adjustment{};
    float feature_scaled_adjustment{};
    float adjusted_response{};
};

struct WheelSuspensionFinalizeInput {
    float adjusted_response{};
    float contact_depth{};
    float previous_contact_depth_0xe8{};
    float piecewise_response{};
    float suspension_base_0x158{};
    float spring_scale_0x3c{};
    PhysicsVector3 contact_direction{};
    bool contact_active{};
};

struct WheelSuspensionFinalizeResult {
    float applied_response{};
    float next_contact_depth_0xe8{};
    PhysicsVector3 applied_force{};
    bool reset_contact{};
};

struct WheelSuspensionPipelineInput {
    WheelSuspensionInput base{};
    WheelSuspensionSurfaceInput surface{};
    PhysicsVector3 contact_direction{};
    bool contact_active{};
};

struct WheelSuspensionPipelineResult {
    WheelSuspensionResult base{};
    WheelSuspensionSurfaceResult surface{};
    WheelSuspensionFinalizeResult finalized{};
};

struct WheelUpdatePipelineInput {
    WheelContactObservation contact{};
    WheelPointTransformInput point_transform{};
    TerrainSurfaceQueryInput terrain{};
    WheelSuspensionPipelineInput suspension{};
    WheelContactFeatureState feature_state{};
};

struct WheelUpdatePipelineResult {
    WheelContactResult contact{};
    WheelSuspensionPipelineResult suspension{};
    WheelContactFeatureProfile feature_profile{};
    WheelContactFeatureState feature_state{};
};

struct FourWheelContactUpdateInput {
    std::array<WheelContactState, 4> contact_states{};
    std::array<WheelUpdatePipelineInput, 4> wheel_inputs{};
};

struct FourWheelContactUpdateResult {
    std::array<WheelContactState, 4> contact_states{};
    std::array<WheelUpdatePipelineResult, 4> wheel_results{};
    std::array<std::size_t, 4> wheel_subobject_offsets{};
    std::size_t wheel_update_calls{};
};

struct WheelCollisionImpulseInput {
    PhysicsVector3 body_center{};
    PhysicsVector3 contact_point{};
    PhysicsVector3 body_velocity{};
    PhysicsVector3 contact_normal{};
    std::array<float, 9> angular_response_matrix{};
    float body_mass{};
    float contact_depth{};
    float contact_threshold_response_0x28{};
    bool wheel_damage_flag_0x20{};
    bool query_flag_nonzero_0x50878c{};
};

struct WheelCollisionImpulseResult {
    float normal_response{};
    PhysicsVector3 impulse{};
    float duration{};
    bool applied{};
    bool damage_hook_reached{};
    bool set_wheel_damage_flag{};
};

// Reconstructs the scalar spring/suspension block in the wheel loop at 0x442530.
[[nodiscard]] WheelSuspensionResult evaluate_wheel_suspension(
    const WheelSuspensionInput& input) noexcept;

// Models the terrain-feature-dependent response adjustment in 0x442530.
[[nodiscard]] WheelSuspensionSurfaceResult adjust_wheel_suspension_for_surface(
    const WheelSuspensionSurfaceInput& input) noexcept;

// Applies the inactive/negative-response relaxation, cap, and body force vector.
[[nodiscard]] WheelSuspensionFinalizeResult finalize_wheel_suspension(
    const WheelSuspensionFinalizeInput& input) noexcept;

// Chains the observed scalar, terrain-feature, and force-finalization stages.
[[nodiscard]] WheelSuspensionPipelineResult evaluate_wheel_suspension_pipeline(
    const WheelSuspensionPipelineInput& input) noexcept;

// Composes transformed contact, suspension finalization, and feature dispatch.
[[nodiscard]] WheelUpdatePipelineResult advance_wheel_update_pipeline(
    WheelContactState& contact_state,
    const WheelUpdatePipelineInput& input) noexcept;

// Composes the four 0x442530 contact/suspension projections in 0x431150's
// observed wheel order. It does not claim to reproduce the remaining wheel
// dynamics or body-accumulator side effects in the original callback.
[[nodiscard]] FourWheelContactUpdateResult advance_four_wheel_contact_updates(
    const FourWheelContactUpdateInput& input) noexcept;

// Reconstructs the normal collision response and conditional wheel impulse
// preparation in 0x442530; body accumulator mutation remains a separate call.
[[nodiscard]] WheelCollisionImpulseResult prepare_wheel_collision_impulse(
    const WheelCollisionImpulseInput& input) noexcept;

// Models the query/result branch and direct contact stores in 0x442530.
// The ray/ground query and global hook are supplied as observations.
[[nodiscard]] WheelContactResult update_wheel_contact(
    WheelContactState& state,
    const WheelContactObservation& observation) noexcept;

// Runs the shared terrain lookup before the reconstructed contact state machine.
[[nodiscard]] WheelContactResult update_wheel_contact_from_terrain(
    WheelContactState& state,
    const WheelContactObservation& observation,
    const TerrainSurfaceQueryInput& terrain) noexcept;

// Chains point construction, the body's 3x4 transform, terrain sampling, and contact stores.
[[nodiscard]] WheelContactResult update_wheel_contact_from_transformed_point(
    WheelContactState& state,
    const WheelContactObservation& observation,
    const WheelPointTransformInput& point_input,
    const TerrainSurfaceQueryInput& terrain) noexcept;

struct WheelTireResponseInput {
    TireWheelResponseFactors factors{};
    PhysicsVector3 forward_basis{};
    PhysicsVector3 contact_normal{};
    PhysicsVector3 projection_basis{};
    float response_scale{};
};

struct WheelTireResponseResult {
    TireWheelResponseComponents components{};
    PhysicsVector3 lateral_basis{};
    PhysicsVector3 uncorrected_vector{};
    PhysicsVector3 corrected_vector{};
};

struct WheelTireBasisUpdateInput {
    PhysicsVector3 response_vector_after_scaling{};
    PhysicsVector3 basis_0x110_0x118{};
    PhysicsVector3 scalar_projection_basis_0x58_0x60{};
    float response_scalar_0x7c{};
};

struct WheelTireResponseVectorInput {
    TireWheelResponseFactors factors{};
    PhysicsVector3 forward_basis{};
    PhysicsVector3 contact_normal{};
    float response_scale{};
};

// Combines recovered response factors, forms the contact-normal lateral basis,
// and applies 0x442530's 1e8 squared-length normalization cap before +0xF4.
// The five tire-curve factors are evaluated from curves and call-site scales.
[[nodiscard]] PhysicsVector3 derive_wheel_tire_response_vector(
    const WheelTireResponseVectorInput& input) noexcept;

struct WheelTireBasisUpdateResult {
    PhysicsVector3 response_vector_0xf4_0xfc{};
    PhysicsVector3 corrected_vector_0x100_0x108{};
    float response_scalar_0x7c{};
};

// Projects and stores the tire vector and scalar in the 0x442530 tail.
[[nodiscard]] WheelTireBasisUpdateResult apply_wheel_tire_basis_update(
    const WheelTireBasisUpdateInput& input) noexcept;

inline constexpr float kBodyIntegrationScale = 0.016F;

struct BodyLinearStep {
    PhysicsVector3 velocity{};
    PhysicsVector3 position{};
    bool velocity_capped{};
};

struct BodyAngularVelocityStep {
    PhysicsVector3 angular_velocity{};
    bool angular_velocity_capped{};
};

struct BodyOrientationStep {
    std::array<float, 9> orientation_matrix{};
    std::array<float, 9> delta_rotation_matrix{};
    float rotation_angle{};
    bool identity_rotation{};
};

struct BodyIntegrationTailState {
    std::array<float, 6> running_accumulators_0x93_to_0x98{};
    std::uint16_t update_counter_0x10{};
    std::uint32_t pending_virtual_record_count_0x11c{};
};

struct BodyBasisSynchronization {
    std::array<float, 9> local_matrix{};
    std::array<float, 9> intermediate_matrix{};
    std::array<float, 9> basis_matrix{};
};

struct BodyIntegrationKinematicsInput {
    PhysicsVector3 position{};
    PhysicsVector3 linear_velocity{};
    PhysicsVector3 accumulated_linear_force{};
    float inverse_mass{};
    PhysicsVector3 angular_velocity{};
    PhysicsVector3 accumulated_angular_moment{};
    std::array<float, 9> orientation_matrix_0x0e{};
    std::array<float, 9> angular_transform_matrix_0x99{};
    std::array<float, 9> body_matrix_0x210{};
};

struct BodyIntegrationKinematicsResult {
    BodyLinearStep linear{};
    BodyAngularVelocityStep angular{};
    BodyOrientationStep orientation{};
    BodyBasisSynchronization basis{};
};

struct BodyIntegrationVirtualRecordCall {
    PhysicsVector3 normalized_first_vector{};
    PhysicsVector3 second_vector{};
    float scalar{};
};

struct BodyIntegrationVirtualRecordCalls {
    std::vector<BodyIntegrationVirtualRecordCall> calls;
    std::size_t truncated_records{};
};

struct BodyIntegrationForceCollectionInput {
    float gravity_mass_0x7c{};
    // Aggregate writes made by the preceding embedded-record callbacks. Their
    // per-record virtual behavior remains caller supplied.
    std::array<float, 6> embedded_record_accumulator_delta{};
    std::span<const float> packed_virtual_records;
};

struct BodyIntegrationForceCollectionResult {
    PhysicsVector3 accumulated_linear_force{};
    PhysicsVector3 accumulated_angular_moment{};
    bool point_impulse_consumed{};
    BodyIntegrationVirtualRecordCalls virtual_record_calls;
};

// Maps the four numeric car variants in the world `obj_car` parser at
// 0x45CAE0 to the FourCC written into its scene-object construction record.
[[nodiscard]] std::optional<std::uint32_t> world_car_object_tag(
    std::uint32_t record_variant) noexcept;

// Identity fields written by 0x45CAE0 into the 0x1E4-byte scene-construction
// record: the variant FourCC at +0 and zero-based car-record ordinal at +0x194.
struct WorldCarSceneRecordIdentity {
    std::uint32_t object_tag{};
    std::int32_t ordinal{};
};

[[nodiscard]] std::optional<WorldCarSceneRecordIdentity>
make_world_car_scene_record_identity(
    std::uint32_t record_variant,
    std::int32_t car_record_ordinal) noexcept;

// Reproduces the linear velocity increment in 0x43ECE0. The binary stores
// the reciprocal of its mass-like field at body offset +0x200.
[[nodiscard]] PhysicsVector3 advance_body_linear_velocity(
    const PhysicsVector3& velocity,
    const PhysicsVector3& accumulated_force,
    float inverse_mass) noexcept;

// Applies the 0x43ECE0 linear velocity cap and position step after force
// accumulation. The cap is 300 units when squared speed exceeds 90000.
[[nodiscard]] BodyLinearStep advance_body_linear_step(
    const PhysicsVector3& position,
    const PhysicsVector3& velocity,
    const PhysicsVector3& accumulated_force,
    float inverse_mass) noexcept;

// Models 0x422F30's row-vector transform using the matrix at body +0x264
// (DWORD index 0x99).
[[nodiscard]] PhysicsVector3 transform_body_angular_accumulator(
    const PhysicsVector3& accumulator,
    const std::array<float, 9>& matrix_0x99) noexcept;
// Applies the transformed accumulator and 0x43ECE0 angular-speed cap.
[[nodiscard]] BodyAngularVelocityStep advance_body_angular_velocity(
    const PhysicsVector3& angular_velocity,
    const PhysicsVector3& transformed_accumulator) noexcept;

// Applies the 0x43ECE0 rotation-vector step to its 3x3 orientation matrix.
[[nodiscard]] BodyOrientationStep advance_body_orientation_matrix(
    const std::array<float, 9>& orientation_matrix,
    const PhysicsVector3& angular_velocity) noexcept;

// Reconstructs the local nine-float matrix assembled by 0x43ECE0 after the
// orientation update, including the instruction-observed stack reorder.
[[nodiscard]] std::array<float, 9> build_body_integrator_local_basis_matrix(
    const std::array<float, 9>& updated_orientation_matrix) noexcept;

// Applies the two observed 0x402DD0 multiplies around the body matrices at
// +0x210 and +0x264. local_matrix is preserved for inspection.
[[nodiscard]] BodyBasisSynchronization synchronize_body_basis_matrices(
    const std::array<float, 9>& local_matrix,
    const std::array<float, 9>& matrix_0x210,
    const std::array<float, 9>& orientation_matrix) noexcept;

// Models the final six accumulator clears and 16-bit counter increment in
// 0x43ECE0.
void finish_body_integration_tail(BodyIntegrationTailState& state) noexcept;

// Consumes the 0x43E830 duration-scaled impulse and point-moment sums at the
// start of 0x43ECE0. Values are divided by accumulated duration, scaled by
// 62.5, added to running linear/angular accumulators, and their six source
// sums are cleared only when duration passes the float-epsilon gate. Duration
// itself is cleared unconditionally.
[[nodiscard]] bool consume_body_point_impulse_accumulator(
    BodyIntegrationTailState& tail_state,
    NcarPointImpulseAccumulator& impulse_state) noexcept;

// Decodes the seven-float records at body DWORD index 0xAC passed through the
// vtable +0x38 callback by 0x43ECE0. The first three values are normalized by
// the same duration as the point-impulse accumulators; records are gated by
// that duration's epsilon test. Incomplete packed records are reported as
// truncated rather than read past the supplied span.
[[nodiscard]] BodyIntegrationVirtualRecordCalls
materialize_body_integration_virtual_record_calls(
    float duration,
    std::span<const float> packed_records,
    std::size_t record_count);

// Composes the observable pre-kinematics accumulator phase of 0x43ECE0:
// gravity, caller-projected embedded-record effects, point-impulse transfer,
// then normalized +0x38 callback arguments. Clears the duration/count fields;
// virtual callback effects are not applied here.
[[nodiscard]] BodyIntegrationForceCollectionResult
prepare_body_integration_force_collection(
    BodyIntegrationTailState& tail_state,
    NcarPointImpulseAccumulator& impulse_state,
    const BodyIntegrationForceCollectionInput& input);

// Composes the ordered linear, angular, orientation, basis-sync, and tail
// stages of 0x43ECE0. Earlier force collection and virtual record effects stay
// caller inputs; the local basis matrix is derived from the updated orientation.
[[nodiscard]] BodyIntegrationKinematicsResult
advance_body_integration_kinematics(
    BodyIntegrationTailState& tail_state,
    const BodyIntegrationKinematicsInput& input) noexcept;

// Decodes the fixed-size 120-byte payload copied by the original tire loader
// at 0x435B50. Parameter semantics remain address-based and undocumented.
[[nodiscard]] std::optional<TireDefinition> parse_tire_resource(
    std::span<const std::byte> payload);

// Evaluators recovered from the tire-object methods at 0x435670 and 0x435750.
[[nodiscard]] float evaluate_tire_quadratic(
    const TireQuadraticTerms& terms, float input) noexcept;
[[nodiscard]] float evaluate_tire_linear(
    const TireLinearTerms& terms, float input) noexcept;

// Rebuilds the two tire-object coefficient pairs from the source TIRE fields.
[[nodiscard]] TireResponseCurveSet derive_tire_response_curves(
    const TireDefinition& tire) noexcept;
[[nodiscard]] TireCurveEvaluation evaluate_tire_response_curves(
    const TireResponseCurveSet& curves, float input) noexcept;
// Models 0x4356D0's signed-input clamp and 1 + input*(+0x3C/+0x38).
// direction_sign_bits is the caller's synthesized status word, not an input float.
[[nodiscard]] TireSlipCurveEvaluation evaluate_tire_slip_curve(
    const TireResponseCurveSet& curves,
    std::uint32_t direction_sign_bits,
    float input) noexcept;

// Rebuilds the four runtime terms from TIRE fields 5-8 and models 0x435780.
[[nodiscard]] TireTrigonometricTerms derive_tire_trigonometric_terms(
    const TireDefinition& tire) noexcept;
[[nodiscard]] float evaluate_tire_trigonometric_response(
    const TireTrigonometricTerms& terms, float input) noexcept;
[[nodiscard]] WheelTireDynamicsResult advance_wheel_tire_dynamics(
    WheelTireDynamicsState& state,
    const WheelTireDynamicsInput& input) noexcept;
// Reconstructs wheel angle, sine/cosine, offset, and body-basis writes in 0x443F60.
[[nodiscard]] WheelAnimationBasisState advance_wheel_animation_basis(
    const WheelAnimationBasisInput& input) noexcept;

// Models the four-coefficient piecewise response used at 0x4405B0.
[[nodiscard]] float evaluate_wheel_piecewise_response(
    const WheelPiecewiseTerms& terms, float input) noexcept;

// Reference speed is already clamped by the wheel update before these ratios.
[[nodiscard]] WheelKinematicRatios derive_wheel_kinematic_ratios(
    float lateral_velocity,
    float longitudinal_velocity,
    float rolling_surface_speed,
    float clamped_reference_speed) noexcept;

// Combines the factors into the two response scalars at 0x442530 before
// projecting them onto wheel-oriented basis vectors.
[[nodiscard]] TireWheelResponseComponents combine_tire_wheel_response(
    const TireWheelResponseFactors& factors) noexcept;
[[nodiscard]] TireWheelResponseFactors derive_tire_wheel_response_factors(
    TireWheelResponseFactors factors,
    const TireWheelCurveFactorInput& input) noexcept;
[[nodiscard]] TireWheelResponseDerivation derive_tire_wheel_response(
    TireWheelResponseFactors factors,
    const TireWheelCurveFactorInput& input) noexcept;

// Assembles the unadjusted response vector formed at 0x442530 before the
// routine applies its additional basis projection.
[[nodiscard]] PhysicsVector3 assemble_tire_response_vector(
    const PhysicsVector3& forward_basis,
    const PhysicsVector3& lateral_basis,
    float forward_response,
    float lateral_response,
    float response_scale) noexcept;

// Applies the additional basis projection performed before 0x43E6F0.
[[nodiscard]] PhysicsVector3 correct_tire_response_projection(
    const PhysicsVector3& response_vector,
    const PhysicsVector3& correction_basis) noexcept;

// Composes the recovered response scalars, contact-normal cross product,
// vector assembly, and body-basis projection from 0x442530. The supplied
// factors contain values produced by the earlier wheel/contact stages.
[[nodiscard]] WheelTireResponseResult compose_wheel_tire_response(
    const WheelTireResponseInput& input) noexcept;

// Reconstructs the normal contact response calculation in 0x42F100.
// angular_response_matrix uses the column-major indexing read by 0x422F30.
[[nodiscard]] float solve_normal_contact_impulse(
    const PhysicsVector3& body_center,
    const PhysicsVector3& contact_point,
    const PhysicsVector3& body_velocity,
    const PhysicsVector3& contact_normal,
    float body_mass,
    const std::array<float, 9>& angular_response_matrix,
    float restitution) noexcept;

// Conversions observed in the wheel/tire setup routine at 0x441C10.
[[nodiscard]] float inches_to_meters(float value) noexcept;
[[nodiscard]] float degrees_to_radians(float value) noexcept;

// Reproduces the x86 binary's ordered IEEE-754 bit-pattern bounds used by
// 0x45C5D0 for its per-car wheel-lock field.
[[nodiscard]] bool valid_wheel_lock_bits(std::uint32_t value_bits) noexcept;
[[nodiscard]] bool valid_wheel_lock(float value) noexcept;

inline constexpr std::uint32_t kCarSetupRecordStride = 200;
inline constexpr std::uint32_t kWheelLockFieldOffset = 0xBC;
inline constexpr std::uint32_t kWheelLockMinimumBits = 0x3C23D70A;
inline constexpr std::uint32_t kWheelLockMaximumBits = 0x3F8147AE;

} // namespace viper
