#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "viper/registered_object_table.hpp"

namespace viper {

struct PhysicsTaskGlobalStore {
    std::uint32_t address{};
    std::uint32_t value{};
};

// Literal global DWORD stores observed in 0x420480, in source order,
// including the two final initialization flags. Dynamic call results are not
// represented here.
[[nodiscard]] std::vector<PhysicsTaskGlobalStore>
physics_task_initialization_global_stores();

struct PhysicsTaskEntityObservation {
    bool nonnull{};
    bool virtual_0x2c_result{};
    std::uint32_t object_offset_0x34{};
};

enum class PhysicsTaskDispatchEventKind : std::uint8_t {
    virtual_0x2c,
    virtual_0x08,
    time_slice_helper_0x427080,
    auxiliary_pair_helper_0x432300,
    alternate_time_helper_0x426a10,
    virtual_0x0c,
};

struct PhysicsTaskDispatchEvent {
    PhysicsTaskDispatchEventKind kind{};
    std::size_t object_index{};
    std::size_t other_object_index{};
};

// The auxiliary presence map corresponds to the 16 DWORDs beginning at
// 0x507D70. Pair helper call order is the nested binary loop's row-major order.
[[nodiscard]] std::vector<PhysicsTaskDispatchEvent>
physics_task_fixed_step_dispatch_order(
    std::span<const PhysicsTaskEntityObservation> objects,
    const std::array<bool, 16>& auxiliary_objects,
    bool alternate_time_path);

enum class PhysicsTimeRingTransitionStatus : std::uint8_t {
    complete,
    invalid_slot_index,
};

struct PhysicsTimeRingTransitionObservation {
    std::int32_t current_slot{};
    std::int32_t next_slot{};
    std::span<const std::uint32_t> slot_offsets;
    std::uint32_t current_cursor{};
    std::uint32_t chunk_size{};
    std::uint32_t storage_base{};
    std::int32_t high_water_mark{};
    std::uint32_t generation{};
    std::uint32_t overflow_generation{};
};

struct PhysicsTimeRingTransitionPlan {
    PhysicsTimeRingTransitionStatus status{
        PhysicsTimeRingTransitionStatus::complete};
    std::vector<std::uint32_t> slot_offsets;
    std::uint32_t cursor_after{};
    std::int32_t high_water_mark_after{};
    std::uint32_t generation_after{};
    std::uint32_t overflow_generation_after{};
    std::uint32_t cleanup_byte_address{};
    bool active_flag_0x508a34_set{true};
    bool position_update_flag_0x508a54_set{};
    bool cleanup_byte_written{};
    bool overflow_detected{};
};

// Models the ring-offset transition in 0x4272C0, including the marker byte
// write delegated to 0x427050. All ring state and backing offsets are supplied.
[[nodiscard]] PhysicsTimeRingTransitionPlan plan_physics_time_ring_transition(
    const PhysicsTimeRingTransitionObservation& observation);

enum class PhysicsTimeSliceStatus : std::uint8_t {
    complete,
    division_by_zero,
    invalid_slot_index,
    missing_slot_offset,
    missing_capture_source,
    null_object_dereference,
};

struct PhysicsTimeSliceCallback {
    std::size_t object_index{};
    std::uint32_t virtual_0x20_argument{};
};

struct PhysicsTimeCaptureObservation {
    std::uint32_t source_word{};
    std::uint8_t source_byte{};
    std::array<float, 3> source_components{};
    bool fdiv_compatibility_enabled{};
};

struct PhysicsTimeCapturePlan {
    std::array<std::uint8_t, 11> record_bytes{};
    std::array<std::int16_t, 3> quantized_components{};
    bool fdiv_compatibility_path_used{};
    bool x87_extended_precision_approximated{true};
};

// Models the 11-byte record written by 0x427150 from globals at 0x507DB0.
[[nodiscard]] PhysicsTimeCapturePlan plan_physics_time_capture(
    const PhysicsTimeCaptureObservation& observation) noexcept;

using PhysicsTimeRecordBytes = std::array<std::uint8_t, 11>;

struct PhysicsTimeInterpolatedSample {
    std::uint32_t source_word{};
    std::uint8_t source_byte{};
    std::array<float, 3> components{};
};

struct PhysicsTimeAlternateObjectCall {
    std::size_t object_index{};
    std::uint32_t current_record_argument{};
    std::uint32_t next_record_argument{};
    float interpolation_fraction{};
};

struct PhysicsTimeCommandObservation {
    std::uint8_t command_byte{};
    bool handler_present{};
    std::optional<std::uint32_t> handler_consumed_bytes;
};

enum class PhysicsTimeCommandPayloadStatus : std::uint8_t {
    complete,
    unknown_command,
    incomplete_payload,
    registered_object_index_out_of_range,
    null_registered_object,
};

struct PhysicsTimeCommandPayloadObservation {
    std::uint8_t command_id{};
    std::span<const std::uint8_t> payload;
    std::int32_t secondary_context{};
    RegisteredObjectTable registered_object_addresses_0x507d70{};
    std::uint32_t event_buffer_size{};
    std::uint32_t record_ring_cursor{};
    bool record_ring_slot_nested_target_nonnull{};
    std::optional<std::uint32_t> record_ring_target_scalar_before_bits_0x08;
};

struct PhysicsTimeCommandPayloadPlan {
    PhysicsTimeCommandPayloadStatus status{
        PhysicsTimeCommandPayloadStatus::unknown_command};
    std::uint32_t consumed_payload_bytes{};
    bool event_buffer_append_attempted{};
    bool event_buffer_append_succeeded{};
    std::uint32_t event_buffer_size_after{};
    std::uint8_t event_buffer_record_type{};
    std::array<std::uint8_t, 28> event_buffer_payload{};
    bool event_buffer_payload_known{};
    std::optional<std::int32_t> registered_object_index;
    std::uint32_t registered_object_address{};
    bool unguarded_registered_object_call_would_fault{};
    bool reset_callback_0x433920_called{};
    bool virtual_0x3c_called{};
    bool command5_vectors_decoded{};
    // Stack argument 1: payload bytes 0..2 converted to a float vector.
    std::array<float, 3> command5_byte_components{};
    // Stack argument 0: words at payload offsets 3, 5, and 7 converted to a
    // float vector.
    std::array<float, 3> command5_word_components{};
    bool id4_validation_panic_called{};
    std::uint32_t record_ring_index_used{};
    std::uint32_t record_ring_cursor_after{};
    bool record_ring_index_out_of_range{};
    bool record_ring_update_callback_called{};
    std::array<std::uint32_t, 3> record_vector_words{};
    std::uint32_t record_scalar_bits{};
    bool record_ring_entry_vector_write_applied{};
    std::optional<bool> record_ring_target_scalar_changed;
    std::optional<std::uint32_t> record_ring_target_scalar_after_bits_0x08;
    std::optional<bool> record_ring_target_change_flag_0x2c_set;
    bool record_ring_target_flag_0x29_set{};
    bool record_ring_target_flag_0x2b_cleared{};
    bool record_ring_target_flag_0x2a_cleared{};
};

// Decodes fixed-width command payload effects recovered from raw handler
// instruction flow. External object and ring storage remain observations.
[[nodiscard]] PhysicsTimeCommandPayloadPlan plan_physics_time_command_payload(
    const PhysicsTimeCommandPayloadObservation& observation) noexcept;

enum class PhysicsAuxiliaryPairStatus : std::uint8_t {
    updated,
    target_object_null_dereference,
    inactive_target,
    outside_radius,
};

struct PhysicsAuxiliaryPairObservation {
    bool target_object_nonnull{true};
    bool target_virtual_0x30_result{};
    std::array<float, 3> receiver_position_0x5c{};
    std::array<float, 3> target_position_0xe08{};
    float target_radius_0xe14{};
    std::array<float, 3> target_force_0x234{};
    std::array<float, 3> receiver_accumulator_0xdfc{};
    bool fdiv_compatibility_enabled{};
};

struct PhysicsAuxiliaryPairPlan {
    PhysicsAuxiliaryPairStatus status{
        PhysicsAuxiliaryPairStatus::outside_radius};
    bool target_virtual_0x30_called{};
    bool proximity_gate_passed{};
    std::array<float, 3> delta{};
    float distance_squared{};
    float radius_squared{};
    long double proximity_factor{};
    std::array<float, 3> receiver_accumulator_after{};
    bool fdiv_compatibility_path_used{};
    bool x87_extended_precision_approximated{true};
};

// Projects the ordered pair effect in 0x432300. The callback and field data
// belong to the target (second) object; output accumulates on the receiver.
[[nodiscard]] PhysicsAuxiliaryPairPlan plan_physics_auxiliary_pair(
    const PhysicsAuxiliaryPairObservation& observation) noexcept;

inline constexpr std::size_t physics_auxiliary_object_count =
    registered_object_table_count;
using PhysicsAuxiliaryObjectTable = RegisteredObjectTable;

struct PhysicsAuxiliaryPairCallObservation {
    std::size_t receiver_index{};
    std::size_t target_index{};
    PhysicsAuxiliaryPairObservation pair{};
};

struct PhysicsAuxiliaryPassObservation {
    bool alternate_time_path{};
    PhysicsAuxiliaryObjectTable object_pointers_0x507d70{};
    std::array<std::optional<std::array<float, 3>>,
        physics_auxiliary_object_count> receiver_accumulators_0xdfc{};
    std::span<const PhysicsAuxiliaryPairCallObservation> pair_calls;
};

struct PhysicsAuxiliaryPairDispatchStep {
    std::size_t receiver_index{};
    std::size_t target_index{};
    bool observation_missing{};
    bool observation_duplicated{};
    bool receiver_accumulator_known{};
    std::optional<PhysicsAuxiliaryPairPlan> pair_effect;
};

struct PhysicsAuxiliaryPassPlan {
    std::array<std::optional<std::array<float, 3>>,
        physics_auxiliary_object_count> receiver_accumulators_after{};
    std::vector<PhysicsAuxiliaryPairDispatchStep> ordered_pair_calls;
    std::uint32_t expected_pair_calls{};
    std::uint32_t pair_observations_matched{};
    std::uint32_t missing_pair_observations{};
    std::uint32_t duplicate_pair_observations{};
    std::uint32_t unused_pair_observations{};
    bool alternate_time_path_delegated{};
    bool accumulator_dependency_unknown{};
    bool target_dereference_fault{};
    bool returned_normally{};
};

// Composes the normal pairwise pass in 0x421050 in its exact nested table
// order. Runtime fields and virtual returns are matched by the receiver/target
// table indices; accumulator writes flow forward across each receiver's calls.
[[nodiscard]] PhysicsAuxiliaryPassPlan plan_physics_auxiliary_pass(
    const PhysicsAuxiliaryPassObservation& observation);

struct PhysicsCommandHandlerRegistration {
    std::uint32_t command_id{};
    std::uint32_t handler_address{};
};

struct PhysicsCommandHandlerRegistryObservation {
    std::array<std::uint32_t, 8> initial_handlers{};
    std::span<const PhysicsCommandHandlerRegistration> registrations;
    std::span<const std::uint32_t> unregistrations;
};

struct PhysicsCommandHandlerRegistryPlan {
    std::array<std::uint32_t, 8> handlers{};
    std::uint32_t registrations_applied{};
    std::uint32_t existing_value_one_observations{};
    std::uint32_t removals_applied{};
    std::uint32_t missing_removal_reporters_called{};
    std::vector<std::uint32_t> unregistered_command_ids;
    bool out_of_range_registration_write{};
};

// Projects registration in 0x427470 and first-match removal in 0x4274A0.
// Out-of-range writes are reported without reproducing unsafe global corruption.
[[nodiscard]] PhysicsCommandHandlerRegistryPlan
plan_physics_command_handler_registry(
    const PhysicsCommandHandlerRegistryObservation& observation);

[[nodiscard]] std::array<std::uint32_t, 8>
physics_command_handlers_registered_by_track_initializers() noexcept;

enum class PhysicsAlternateTimeStatus : std::uint8_t {
    disabled,
    complete,
    division_by_zero,
    invalid_slot_index,
    missing_slot_offset,
    missing_record,
    command_observation_exhausted,
};

struct PhysicsAlternateTimeObservation {
    bool path_enabled{};
    std::int32_t simulation_counter{};
    std::int32_t slot_count{};
    std::int32_t previous_slot{};
    std::span<const std::uint32_t> slot_offsets;
    std::uint32_t storage_base{};
    std::optional<PhysicsTimeRecordBytes> current_record;
    std::optional<PhysicsTimeRecordBytes> next_record;
    std::span<const PhysicsTaskEntityObservation> objects;
    std::span<const PhysicsTimeCommandObservation> command_stream;
};

struct PhysicsAlternateTimePlan {
    PhysicsAlternateTimeStatus status{PhysicsAlternateTimeStatus::disabled};
    std::int32_t selected_slot{};
    std::int32_t next_slot{};
    std::int32_t previous_slot_after{};
    float interpolation_fraction{};
    std::optional<PhysicsTimeInterpolatedSample> interpolated_sample;
    std::vector<PhysicsTimeAlternateObjectCall> object_calls;
    std::uint32_t command_handlers_called{};
    std::uint32_t command_bytes_advanced{};
    std::uint32_t active_object_gates_called{};
    bool record_command_parser_reached{};
    bool parser_input_exhausted{};
    bool previous_slot_global_written{};
    bool sync_flag_0x508760_set{};
    bool x87_extended_precision_approximated{true};
};

// Models the data/ring and per-object portions of 0x426A10. Indirect command
// handler presence and consumed lengths are explicit bounded observations.
[[nodiscard]] PhysicsAlternateTimePlan plan_physics_alternate_time_path(
    const PhysicsAlternateTimeObservation& observation);

struct PhysicsTimeSliceObservation {
    std::int32_t simulation_counter{};
    std::int32_t slot_count{};
    std::int32_t current_slot{};
    std::uint32_t storage_base{};
    std::span<const std::uint32_t> slot_offsets;
    // 0x4272C0 may rewrite this slot when the selected index changes.
    std::optional<std::uint32_t> updated_selected_slot_offset;
    std::optional<PhysicsTimeCaptureObservation> capture_source;
    std::span<const PhysicsTaskEntityObservation> objects;
};

struct PhysicsTimeSlicePlan {
    PhysicsTimeSliceStatus status{PhysicsTimeSliceStatus::complete};
    std::int32_t selected_slot{};
    std::uint32_t sync_helper_calls{};
    std::uint32_t captured_record_address{};
    bool transition_helper_0x4272c0_called{};
    bool capture_helper_0x427150_called{};
    std::optional<PhysicsTimeCapturePlan> captured_record;
    std::vector<PhysicsTimeSliceCallback> callbacks_0x20;
};

// Projects the ring-slot selection and transition-dependent callbacks in
// 0x427080. The mutated selected-slot offset after 0x4272C0 is an observation.
[[nodiscard]] PhysicsTimeSlicePlan plan_physics_time_slice(
    const PhysicsTimeSliceObservation& observation);

struct PhysicsTaskObjectFactoryDescriptor {
    std::uint32_t fourcc{};
    std::uint32_t allocation_bytes{};
    std::uint32_t constructor_address{};
};

enum class PhysicsTaskFactoryStatus {
    constructed,
    allocation_failed,
    constructor_returned_null,
    unknown_type,
};

struct PhysicsTaskFactoryPlan {
    PhysicsTaskFactoryStatus status{PhysicsTaskFactoryStatus::unknown_type};
    std::optional<PhysicsTaskObjectFactoryDescriptor> descriptor;
    bool allocation_attempted{};
    bool constructor_called{};
    bool panic_reporter_called{};
    bool continued_after_panic{};
    bool factory_call_returns{true};
    bool returns_object{};
};

struct PhysicsTaskRecordLoadObservation {
    // Negative values are the end-of-records result from 0x4C5E10.
    std::int32_t record_length{};
    // Portable bound: whether the bytes needed to read the record header and
    // declared payload are available to the reconstruction.
    bool payload_complete{true};
    std::uint32_t fourcc{};
    bool allocation_succeeded{};
    bool constructor_returned_object{};
    bool panic_reporter_returned{};
    bool object_virtual_0x2c_result{};
};

enum class PhysicsTaskRecordLoadStatus {
    end_of_records,
    incomplete_payload,
    factory_did_not_return,
    factory_returned_null,
    object_loaded,
};

struct PhysicsTaskRecordLoadPlan {
    PhysicsTaskRecordLoadStatus status{
        PhysicsTaskRecordLoadStatus::end_of_records};
    PhysicsTaskFactoryPlan factory{};
    bool payload_read{};
    bool virtual_0x2c_called{};
    bool post_construct_0x426f20_called{};
    bool output_slot_written{};
    bool returns_success{};
    // If set, 0x4C5EA0 would copy bytes outside the captured record span.
    bool binary_read_exceeds_payload{};
};

struct PhysicsTaskArrayPopulationPlan {
    std::uint32_t allocation_bytes{0x800U};
    std::uint32_t successful_records{};
    std::uint32_t reader_calls{};
    bool allocation_attempted{true};
    bool allocation_failed{};
    bool array_pointer_used_after_allocation_failure{};
    bool terminated_by_reader{};
    bool observations_exhausted{};
    bool writes_past_allocated_array{};
};

enum class PhysicsTrackServiceStatus {
    constructed,
    allocation_failed_then_virtual_call,
    constructor_returned_null_then_virtual_call,
    unknown_type_panic_did_not_return,
    unknown_type_null_virtual_call,
};

struct PhysicsTrackServicePlan {
    PhysicsTrackServiceStatus status{
        PhysicsTrackServiceStatus::unknown_type_panic_did_not_return};
    std::uint32_t selector{};
    std::uint32_t allocation_bytes{};
    std::uint32_t constructor_address{};
    std::uint32_t vtable_address{};
    std::uint32_t post_load_virtual_0x0c_target{};
    bool allocation_attempted{};
    bool constructor_called{};
    bool panic_reporter_called{};
    bool continued_after_panic{};
    bool virtual_0x14_called{};
    bool virtual_call_has_null_object{};
};

enum class PhysicsTrackPostLoadStatus {
    evaluated,
    missing_input,
    null_receiver_fault,
    callback_body_unrecovered,
};

struct PhysicsTrackPostLoadObservation {
    std::uint32_t selector{};
    bool receiver_nonnull{};
    std::optional<std::array<std::uint32_t, 3>> fields_0x44_0x48_0x4c;
};

struct PhysicsTrackObjectWrite {
    std::uint32_t byte_offset{};
    std::uint32_t value{};
};

struct PhysicsTrackPostLoadPlan {
    PhysicsTrackPostLoadStatus status{
        PhysicsTrackPostLoadStatus::callback_body_unrecovered};
    std::uint32_t callback_address{};
    std::optional<bool> callback_result;
    bool virtual_0x0c_called{};
    std::vector<PhysicsTrackObjectWrite> object_writes;
};

// Evaluates recovered post-load callbacks for selectors 0, 1, 3, and 5.
[[nodiscard]] PhysicsTrackPostLoadPlan plan_physics_track_post_load(
    const PhysicsTrackPostLoadObservation& observation) noexcept;

enum class PhysicsTaskTableInitStatus {
    completed,
    division_by_zero,
    sentinel_fill_dereferences_null,
};

struct PhysicsTaskTableInitObservation {
    std::uint32_t previous_stride{};
    std::int32_t maximum_entries{};
    bool primary_allocation_succeeded{};
    bool sentinel_allocation_succeeded{};
};

struct PhysicsTaskTableInitPlan {
    PhysicsTaskTableInitStatus status{PhysicsTaskTableInitStatus::completed};
    std::uint32_t primary_allocation_bytes{0x200000U};
    std::uint32_t entry_stride{};
    std::uint32_t entry_count{};
    std::uint32_t sentinel_allocation_bytes{};
    std::uint32_t sentinel_fill_iterations{};
    bool primary_allocation_attempted{true};
    bool primary_allocation_failed{};
    bool sentinel_allocation_attempted{};
    bool sentinel_allocation_failed{};
    bool primary_null_pointer_stored{};
    bool zero_length_fill_loop_present{};
    bool sentinel_fill_dereferences_null{};
};

// Models allocation sizing and sentinel initialization in 0x426EB0. Values
// sourced from globals and both allocation outcomes are explicit observations.
[[nodiscard]] PhysicsTaskTableInitPlan plan_physics_task_table_init(
    const PhysicsTaskTableInitObservation& observation) noexcept;

// Models the service-object selector and the unconditional final virtual call
// in 0x426000. Construction and panic outcomes are runtime observations.
[[nodiscard]] PhysicsTrackServicePlan plan_physics_track_service(
    std::uint32_t selector,
    bool allocation_succeeded,
    bool constructor_returned_object,
    bool panic_reporter_returned) noexcept;

// Each byte represents the low-byte success value returned by 0x4224D0 for
// successive calls. The first false value ends the loop; the reader is called
// once even when there are no successful records. The binary has no 512-slot
// capacity check for its 0x800-byte pointer array.
[[nodiscard]] PhysicsTaskArrayPopulationPlan plan_physics_task_array_population(
    std::span<const std::uint8_t> reader_success,
    bool array_allocation_succeeded) noexcept;

// Returns the constructor and allocation size selected by 0x422580 for an
// observed FourCC tag. Values are the raw DWORD encodings used by the binary.
[[nodiscard]] std::optional<PhysicsTaskObjectFactoryDescriptor>
physics_task_object_factory_descriptor(std::uint32_t fourcc) noexcept;

// Models one 0x422580 factory dispatch. Allocation and constructor return are
// runtime observations; an unknown tag calls 0x411000 before returning null.
[[nodiscard]] PhysicsTaskFactoryPlan plan_physics_task_object_factory(
    std::uint32_t fourcc,
    bool allocation_succeeded,
    bool constructor_returned_object,
    bool panic_reporter_returned) noexcept;

// Models one 0x4224D0 record read, factory call, virtual +0x2C gate,
// optional 0x426F20 call, and successful output-slot write.
[[nodiscard]] PhysicsTaskRecordLoadPlan plan_physics_task_record_load(
    const PhysicsTaskRecordLoadObservation& observation) noexcept;

} // namespace viper
