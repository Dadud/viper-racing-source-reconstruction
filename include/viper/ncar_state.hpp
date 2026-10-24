#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "viper/registered_object_table.hpp"
#include "viper/model_resource.hpp"
#include "viper/timed_event_queue.hpp"

namespace viper {

// Numeric gates used by NCAR helpers. Their fields keep binary offsets because
// the serialized record's application-level schema is still being recovered.
struct NcarStateFields {
    float record_time_0x00{};
    float timer_0x50c{};
    int record_mode_0xec8{};
    std::uint32_t reset_gate_0x4fc{};
    bool initialized_0xf34{};
    float angular_x_0x234{};
    float angular_y_0x238{};
    float angular_z_0x23c{};
    float angular_w_0x240{};
    float angular_extra_0x244{};
    float angular_extra_0x248{};
    std::uint32_t field_0xcbc{};
    std::uint32_t field_0xd64{};
    float field_0xde0{};
    float field_0xde4{};
    float field_0xde8{};
    float field_0xdec{};
};

inline constexpr std::size_t ncar_sample_record_bytes = 0x3e;
inline constexpr std::size_t ncar_sample_window_bytes = 3 * ncar_sample_record_bytes;
// 0x427630 advances interpolation records by 0x3c bytes. This is a separate
// layout from the 0x3e-byte timed sample payload used by the history queue.
inline constexpr std::size_t ncar_interpolated_record_bytes = 0x3c;

using NcarResetResourceRecord = std::array<std::uint32_t, 8>;

struct NcarResetResourceGroupInput {
    std::size_t destination_record_count{};
    std::size_t source_record_count{};
    std::optional<std::uint32_t> descriptor_pointer_0x10;
    std::span<NcarResetResourceRecord> destination_records;
    std::span<const NcarResetResourceRecord> source_records;
};

struct NcarResetResourceGroupsResult {
    std::uint8_t groups_visited{};
    std::array<std::size_t, 5> records_copied{};
    std::array<std::size_t, 5> records_unprocessed{};
    std::array<bool, 5> descriptor_pointer_writes_attempted{};
    std::array<std::optional<std::uint32_t>, 5> descriptor_pointer_values{};
    bool inputs_complete{true};
};

struct NcarResetSphereLocalGeometry {
    float local_b0_after_body_shift{};
    float local_bc_after_clamp{};
    float local_98_after_body_shift{};
    float local_9c_after_body_shift{};
};

struct NcarResetSphereBoundsObservation {
    std::optional<bool> extents_pointer_present;
    std::size_t vertex_count{};
    std::span<const std::array<float, 8>> vertices;
    float car_record_0x58{};
    float car_record_0x5c{};
    // Runtime +0x128 is copied from CARF v6 word 0x66 by 0x45E4B0.
    float car_record_0x128{};
    // Preferred when extents were already measured from a parsed FNIM model.
    std::optional<ModelVertexBounds> precomputed_model_bounds;
};

struct NcarResetSphereBoundsPlan {
    std::array<float, 6> raw_bounds{}; // min/max X, min/max Y, min/max Z
    NcarResetSphereLocalGeometry local_geometry{};
    bool extents_pointer_unknown{};
    bool unusual_model_reported{};
    bool inputs_complete{};
};

[[nodiscard]] NcarResetSphereBoundsPlan derive_ncar_reset_sphere_geometry(
    const NcarResetSphereBoundsObservation& observation) noexcept;

[[nodiscard]] std::array<std::array<float, 3>, 12>
derive_ncar_reset_sphere_local_positions(
    const NcarResetSphereLocalGeometry& geometry) noexcept;

struct NcarResetBackendGroupObservation {
    std::optional<bool> group_object_present;
    std::size_t child_count_0x4c{12U};
    struct Child {
        bool pointer_present{};
        std::array<float, 3> local_position_0x20{};
        std::optional<std::array<float, 3>> constructor_position_0x64;
        std::optional<float> radius;
    };
    std::span<const Child> children;
    std::optional<NcarResetSphereLocalGeometry> constructor_geometry;
    std::optional<NcarResetSphereBoundsObservation> source_bounds;
    std::optional<bool> parent_transform_present;
    std::array<float, 12> parent_transform_0x00_to_0x2c{};
};

struct NcarResetBackendGroupPlan {
    std::uint32_t group_virtual_0x04_target{0x0042cce0U};
    std::uint32_t child_virtual_0x04_target{0x0042beb0U};
    bool group_virtual_0x04_attempted{};
    bool group_pointer_presence_unknown{};
    bool group_pointer_fault{};
    std::size_t child_count_0x4c{};
    std::size_t child_entries_visited{};
    std::size_t child_calls_attempted{};
    std::size_t child_calls_unobserved{};
    bool null_child_pointer_fault{};
    bool parent_transform_presence_unknown{};
    bool null_parent_transform_fault{};
    bool geometry_inputs_incomplete{};
    bool unusual_model_reported{};
    bool radius_inputs_incomplete{};
    float group_radius_0x50{};
    std::size_t constructor_child_tag_writes_0x18{};
    std::optional<std::uint32_t> constructor_child_tag_value_0x18;
    std::optional<std::uint32_t> group_tag_value_0x18;
    std::size_t group_child_tag_propagations{};
    bool child_allocations_assumed_success{};
    std::vector<std::array<float, 3>> child_world_positions_0x2c;
};

// Models the 0x42CCE0 child loop and its sphere transform callback 0x42BEB0.
[[nodiscard]] NcarResetBackendGroupPlan plan_ncar_reset_backend_group(
    const NcarResetBackendGroupObservation& observation);

struct NcarBackendChildRefreshEffect {
    bool virtual_0x14_attempted{};
    bool null_child_pointer_fault{};
    bool byte_0x38_cleared{};
    bool byte_0x88_cleared{};
    std::optional<std::array<float, 3>> restored_position_0x20;
    bool constructor_position_unknown{};
};

struct NcarBackendGroupRefreshPlan {
    std::uint32_t group_virtual_0x14_target{0x0042d0a0U};
    std::uint32_t child_virtual_0x14_target{0x0042cc10U};
    bool group_virtual_0x14_attempted{};
    bool group_pointer_presence_unknown{};
    bool group_pointer_fault{};
    std::size_t child_count_0x4c{};
    std::size_t child_entries_visited{};
    std::size_t child_calls_unobserved{};
    bool geometry_inputs_incomplete{};
    bool unusual_model_reported{};
    bool child_allocations_assumed_success{};
    std::vector<NcarBackendChildRefreshEffect> children;
};

// Projects the object's virtual +0x14 backend refresh path and child state writes.
[[nodiscard]] NcarBackendGroupRefreshPlan plan_ncar_backend_group_refresh(
    const NcarResetBackendGroupObservation& observation);

struct NcarObjectFieldWrite {
    std::uint32_t offset{};
    std::uint8_t width_bytes{};
    std::uint32_t value_bits{};
};

struct NcarObjectInitializationObservation {
    NcarResetBackendGroupObservation backend_group;
    std::array<float, 12> saved_transform_0xe6c{};
    float source_value_0xe9c{};
    float source_value_0xea0{};
    std::size_t child_count_0x1ec{};
    std::size_t child_flag_storage_count{};
    std::array<NcarResetResourceGroupInput, 5> resource_groups{};
    std::array<std::uint8_t, 5> resource_flags_0x15{};
};

struct NcarObjectInitializationPlan {
    std::vector<NcarObjectFieldWrite> writes_before_backend_refresh;
    NcarBackendGroupRefreshPlan backend_group_refresh;
    std::vector<NcarObjectFieldWrite> writes_before_helper_0x4331e0;
    bool helper_0x4331e0_called{};
    std::vector<NcarObjectFieldWrite> writes_from_helper_0x4331e0;
    std::vector<NcarObjectFieldWrite> writes_after_helper_0x4331e0;
    bool helper_0x4338c0_called{};
    std::uint8_t child_flag_value_0x98{1U};
    std::size_t child_flags_written{};
    std::size_t child_flags_unobserved{};
    std::vector<NcarObjectFieldWrite> wheel_reset_writes_0x441b90;
    std::vector<NcarObjectFieldWrite> reset_object_writes_0x433920;
    NcarResetResourceGroupsResult reset_resources;
    std::array<std::uint8_t, 5> reset_resource_flags_0x15{};
    NcarResetBackendGroupPlan reset_backend_group;
    std::uint32_t cleared_field_0xc0c{};
    NcarBackendGroupRefreshPlan final_backend_group_refresh;
};

// Projects the ordered direct writes and calls observed in object initializer 0x433AA0.
[[nodiscard]] NcarObjectInitializationPlan plan_ncar_object_initialization(
    const NcarObjectInitializationObservation& observation);

struct NcarVertexMapGroupObservation {
    std::size_t vertex_count{};
    std::span<const std::array<float, 8>> vertices;
};

struct NcarVertexMapPlan {
    std::array<std::vector<std::uint16_t>, 5> reference_indices;
    std::array<std::size_t, 5> vertices_mapped{};
    std::array<std::size_t, 5> vertices_unobserved{};
    std::array<bool, 5> group_inputs_complete{};
    bool base_group_identity_mapping{true};
    float nearest_distance_limit_squared{100000000.0F};
    bool allocations_assumed_success{true};
};

// Maps constructor groups 1..4 to the nearest vertex in group 0; group 0 maps to itself.
[[nodiscard]] NcarVertexMapPlan plan_ncar_vertex_maps(
    const std::array<NcarVertexMapGroupObservation, 5>& groups);

struct NcarBodyConstructorObservation {
    std::array<float, 4> source_values_0x08_to_0x14{};
    std::uint32_t source_record_word_0x00{};
    // Offset of this object's record in the serialized physics-task stream.
    // The loader reads the DWORD preceding each record and passes it unchanged.
    std::optional<std::uint32_t> serialized_object_stream_offset;
    bool fdiv_compatibility_enabled{};
    std::optional<std::uint32_t> compatibility_fill_bits;
};

// Adapts the bounded 0x1E4-byte car runtime record consumed by the shared
// 0x43E490 body initializer. DWORDs at +0x08..+0x14 are interpreted as the
// four constructor source floats; the stream offset remains an explicit caller
// observation because it depends on the complete physics-task serialization.
[[nodiscard]] std::optional<NcarBodyConstructorObservation>
make_ncar_body_constructor_observation_from_runtime_record(
    std::span<const std::byte> runtime_record,
    std::optional<std::uint32_t> serialized_object_stream_offset,
    bool fdiv_compatibility_enabled,
    std::optional<std::uint32_t> compatibility_fill_bits = std::nullopt) noexcept;

struct NcarBodyConstructorPlan {
    std::uint32_t vtable_address{0x004c85e0U};
    std::uint8_t cleared_byte_0x474{};
    std::uint32_t cleared_dword_0x1ec{};
    std::size_t initialized_subobjects_at_0x6c{};
    std::size_t initialized_subobjects_at_0x2b0{};
    std::vector<NcarObjectFieldWrite> inherited_base_writes_0x4375c0;
    std::vector<NcarObjectFieldWrite> subobject_writes_0x43f460;
    std::vector<NcarObjectFieldWrite> finalizer_writes_0x43e650;
    std::array<float, 4> scaled_dimensions_0x1f0_to_0x1fc{};
    std::array<float, 4> reciprocal_dimensions_0x200_to_0x20c{};
    std::array<std::optional<float>, 9> matrix_0x210{};
    std::array<std::optional<float>, 9> copied_matrix_0x264{};
    bool helper_0x4375c0_called{};
    bool helper_0x43e650_called{};
    bool fdiv_compatibility_enabled{};
    bool fdiv_compatibility_fill_unknown{};
};

// Projects the direct fields and fixed subobject loops in body initializer 0x43E490.
[[nodiscard]] NcarBodyConstructorPlan plan_ncar_body_constructor(
    const NcarBodyConstructorObservation& observation);

struct NcarConstructorSubobjectDefaultsPlan {
    std::vector<NcarObjectFieldWrite> writes;
    std::size_t update_curve_subobjects_initialized{};
    std::size_t selector_subobjects_initialized{};
    std::size_t linked_state_subobjects_initialized{};
    bool constructor_tail_source_unknown{};
};

// Projects the simple embedded-object defaults called directly by 0x42FF90.
[[nodiscard]] NcarConstructorSubobjectDefaultsPlan
plan_ncar_constructor_subobject_defaults(
    std::optional<std::uint32_t> source_record_word_0x65 = std::nullopt);

struct NcarWheelConstructorObservation {
    bool fdiv_compatibility_enabled{};
    std::optional<std::uint32_t> compatibility_fill_bits;
};

struct NcarWheelConstructorPlan {
    std::vector<NcarObjectFieldWrite> writes;
    std::vector<std::uint32_t> unknown_fill_offsets;
    bool fdiv_compatibility_enabled{};
};

inline constexpr std::size_t ncar_wheel_object_bytes = 0x1a4U;

// A byte-backed wheel object preserves which constructor bytes are known.
// Unknown/unwritten native storage must not be mistaken for initialized zero.
struct NcarWheelRuntimeObjectState {
    std::array<std::byte, ncar_wheel_object_bytes> bytes{};
    std::array<std::uint8_t, ncar_wheel_object_bytes> initialized_bytes{};
};

[[nodiscard]] NcarWheelConstructorPlan plan_ncar_wheel_constructor(
    std::uint32_t wheel_object_offset,
    const NcarWheelConstructorObservation& observation);

// Applies only fully resolved plan writes within the wheel's bounded object
// storage. The destination is unchanged on unknown fills or invalid writes.
[[nodiscard]] bool materialize_ncar_wheel_constructor(
    NcarWheelRuntimeObjectState& destination,
    std::uint32_t wheel_object_offset,
    const NcarWheelConstructorPlan& plan) noexcept;

struct NcarExternalFieldWrite {
    std::uint32_t object_address{};
    std::uint32_t offset{};
    std::uint8_t width_bytes{};
    std::uint32_t value_bits{};
};

struct NcarSoundConstructorObservation {
    // Results from 0x46C110, then 0x46C080 factories (road1, road2, scrape,
    // horn, start), and the 0x46C030 shift factory, in constructor order.
    std::array<std::optional<std::uint32_t>, 7> handles{};
    std::optional<std::uint32_t> owner_handle_0x510;
    std::optional<std::uint32_t> shift_gain_0x30_bits;
    std::optional<std::uint32_t> shift_pitch_0x08_bits;
};

struct NcarSoundConstructorPlan {
    std::vector<NcarObjectFieldWrite> car_writes;
    std::vector<NcarExternalFieldWrite> sound_writes;
    std::vector<std::uint32_t> unresolved_handle_slots;
    std::vector<std::uint32_t> unresolved_sound_objects;
};

// Projects the sound factory result stores and post-factory mutations at the
// end of 0x42FF90. Allocator, resource lookup, and filename resolution remain inputs.
[[nodiscard]] NcarSoundConstructorPlan plan_ncar_sound_constructor(
    const NcarSoundConstructorObservation& observation);

// Shared bounded projection of the five 0x433920 descriptor/resource passes:
// destination-counted 0x20-byte rows copy three DWORDs and clear byte +0x15.
[[nodiscard]] NcarResetResourceGroupsResult reset_ncar_resource_groups(
    const std::array<NcarResetResourceGroupInput, 5>& groups,
    std::array<std::uint8_t, 5>& resource_flags) noexcept;

struct NcarConstructorLayout {
    static constexpr std::size_t object_size = 0xf38;
    static constexpr std::size_t sample_window_offset = 0xec0;
    static constexpr std::size_t sample_window_bytes = 0xba;
    static constexpr std::uint32_t vtable_address = 0x004c8238U;
};

// Applies direct field initialization from 0x438E40. The sample-window
// address and allocated storage are supplied observations; allocation and
// global event/manager registration calls are not performed here.
[[nodiscard]] bool initialize_ncar_constructor_storage(
    std::span<std::byte> object,
    std::span<std::byte> sample_window,
    std::uint32_t sample_window_address) noexcept;

enum class NcarDestructorAction : std::uint8_t {
    install_ncar_vtable,
    destroy_global_delay_queue,
    free_global_delay_queue,
    clear_global_delay_queue,
    unregister_shared_manager_client,
    release_indexed_manager_entry,
    clear_indexed_manager_slot,
    release_sample_window,
    clear_sample_window_fields,
    call_base_car_destructor,
};

struct NcarDestructorObservation {
    bool event_processor_active{};
    bool global_delay_queue_present{};
    std::uint32_t manager_index_0x510{};
    bool indexed_manager_entry_present{};
};

struct NcarDestructorPlan {
    std::array<NcarDestructorAction, 10> actions{};
    std::size_t action_count{};
    std::uint32_t manager_index_0x510{};
};

// Models the ordered teardown in 0x439040; called resource/manager routines
// remain external effects represented by the action list.
[[nodiscard]] NcarDestructorPlan plan_ncar_destruction(
    const NcarDestructorObservation& observation) noexcept;

// Applies the direct vtable and sample-window pointer writes from 0x439040.
[[nodiscard]] bool apply_ncar_destructor_field_writes(
    std::span<std::byte> object) noexcept;

struct NcarSampleRecord {
    std::array<std::byte, ncar_sample_record_bytes> raw_bytes{};
    std::uint32_t time_bits_0x00{};
    std::uint32_t sample_time_bits_0x04{};
    // These remain bit patterns: some are floats, but preserving their exact
    // serialized representation avoids assuming host-side rounding semantics.
    std::uint32_t value_bits_0x08{};
    std::uint32_t signed_scalar_bits_0x0c{};
    std::uint32_t value_bits_0x10{};
    std::uint32_t small_integer_0x14{};
    std::uint32_t scaled_byte_value_bits_0x18{};
    std::array<std::uint32_t, 3> vector_bits_0x1c{};
    std::array<std::uint32_t, 4> rotation_bits_0x28{};
    std::uint8_t flag_0x38{};
    std::uint8_t flag_0x39{};
    std::array<std::uint8_t, 4> gate_bytes_0x3a{};
};

// Mutable NCAR body fields read by the timed sample snapshot at 0x43E050.
// The offsets are kept explicit until their gameplay meanings are recovered.
struct NcarSampleProducerObjectState {
    float value_0xd70{};
    std::uint32_t value_0xd64{};
    float value_0x500{};
    float value_0xde4{};
    std::uint8_t flag_0xe28{};
    std::uint32_t value_0xc34{};
    std::uint8_t flag_0xec0{};
    std::array<std::uint8_t, 4> wheel_damage_0x574{};
    std::array<float, 3> position_0x5c{};
    // Row-major orientation matrix stored at object +0x38.
    std::array<float, 9> orientation_0x38{};
};

struct NcarSampleProducerCaptureResult {
    bool captured{};
    bool orientation_normalized{};
    bool fdiv_compatibility_enabled{};
};

// Captures the direct state writes of 0x43E050. The orientation matrix and
// +0xEC0 flag are mutated in the same order as the native callback. A
// degenerate basis is rejected before changing either object or record state.
[[nodiscard]] NcarSampleProducerCaptureResult capture_ncar_sample_record(
    NcarSampleRecord& destination,
    NcarSampleProducerObjectState& body,
    float current_time,
    bool fdiv_compatibility_enabled) noexcept;

struct NcarInterpolatedWheelObservation {
    std::array<std::uint8_t, 5> first_record{};
    std::array<std::uint8_t, 5> second_record{};
    float interpolation_fraction{};
    std::array<float, 3> body_velocity_0x234{};
};

struct NcarInterpolatedWheelPlan {
    float field_0xe8{};
    float angle_0xac{};
    float field_0x124{};
    float field_0x11c{};
    std::uint8_t flags_0x120{};
    std::uint8_t flag_0x20{};
    std::array<float, 3> copied_body_velocity_0xbc{};
    bool effect_helper_0x443f60_called{true};
    bool body_to_wheel_helper_0x437ba0_called{true};
    bool x87_extended_precision_approximated{true};
};

struct NcarInterpolatedObjectObservation {
    std::array<std::uint8_t, ncar_interpolated_record_bytes> first_record{};
    std::array<std::uint8_t, ncar_interpolated_record_bytes> second_record{};
    float interpolation_fraction{};
    bool object_reset_gate_0x48c_nonzero{};
    bool object_flag_0xbe4_nonzero{};
    float object_scalar_0xdcc{};
    bool fdiv_compatibility_enabled{};
    NcarResetBackendGroupObservation reset_backend_group{};
};

struct NcarInterpolatedObjectPlan {
    float field_0xd70{};
    float field_0x500{};
    float field_0x50c{};
    float setter_0xde0{};
    float setter_0xde4{};
    std::array<float, 3> copied_velocity_0xdf0{};
    float field_0x630{};
    float field_0x7d4{};
    float field_0x978{};
    float field_0xb1c{};
    std::uint32_t cleared_field_0xe40{};
    std::uint8_t cleared_field_0xe49{};
    std::uint32_t cleared_field_0x470{};
    std::uint16_t combined_flags_0x04{};
    bool field_0xe28{};
    std::int8_t field_0xd68{};
    std::int8_t field_0xd64{};
    std::array<float, 3> body_velocity_0x234{};
    std::array<float, 12> transform_0x38_to_0x64{};
    float speed_magnitude_0xddc{};
    bool reset_helper_0x433920_called{};
    std::uint8_t reset_object_byte_0x48c{};
    std::uint8_t reset_object_byte_0x48d{};
    std::array<std::uint32_t, 8> reset_object_dwords_0x48e{};
    std::array<std::uint8_t, 4> reset_wheel_damage_bytes_0x574{};
    std::uint8_t reset_resource_group_passes{};
    bool reset_backend_virtual_call_attempted{};
    NcarResetBackendGroupPlan reset_backend_group{};
    bool rotation_endpoint_is_second_record{true};
    std::array<NcarInterpolatedWheelPlan, 4> wheel_updates{};
    bool x87_extended_precision_approximated{true};
};

struct NcarInterpolatedUpdateObservation {
    NcarInterpolatedObjectObservation object{};
    std::uint32_t saved_object_flags_0x04{};
};

struct NcarInterpolatedUpdatePlan {
    NcarInterpolatedObjectPlan object_update{};
    std::uint32_t restored_object_flags_0x04{};
    float interpolated_speed_0xed0{};
    bool x87_extended_precision_approximated{true};
};

struct NcarInterpolatedRecordSelectionObservation {
    std::span<const std::byte> records;
    // Captures the orphan callsite at 0x427DA3: EBX is the object, whose +0xEF4
    // pointer may point inside records and whose +0xEF8 supplies the limit.
    std::size_t record_pointer_origin_index{};
    std::uint32_t global_time_bits_0x508774{};
    std::uint32_t object_value_bits_0xedc{};
    std::uint32_t object_value_bits_0xee0{};
    // DAT_00508A58 in 0x426670/0x426940.
    std::int32_t global_count_0x508a58{};
    // 0x427630 calls 0x427B10 only when the next slot is below this value.
    std::int32_t object_limit_0xef8{};
};

struct NcarInterpolatedRecordSelectionPlan {
    bool selection_parameters_valid{};
    bool record_window_valid{};
    bool interpolation_callback_0x427b10_reached{};
    std::int32_t frame_step_param_3{};
    std::int32_t first_slot_index{};
    std::int32_t second_slot_index{};
    float interpolation_fraction{};
    std::array<std::uint8_t, ncar_interpolated_record_bytes> first_record{};
    std::array<std::uint8_t, ncar_interpolated_record_bytes> second_record{};
};

inline constexpr std::size_t ncar_registered_object_count =
    registered_object_table_count;
using NcarRegisteredObjectTable = RegisteredObjectTable;

struct NcarObjectRegistrationObservation {
    NcarRegisteredObjectTable table{};
    std::int32_t index{};
    std::uint32_t object_pointer{};
};

struct NcarObjectRegistrationPlan {
    NcarRegisteredObjectTable table{};
    bool pre_registration_noop_0x415090_called{true};
    bool post_registration_noop_0x4150a0_called{true};
    bool index_in_table{};
    bool out_of_table_write_unmodeled{};
    bool table_write_applied{};
};

struct NcarObjectLookupObservation {
    NcarRegisteredObjectTable table{};
    std::int32_t index{};
};

struct NcarObjectLookupPlan {
    bool no_op_helper_0x420960_called{true};
    bool index_in_table{};
    bool out_of_table_read_unmodeled{};
    std::optional<std::uint32_t> object_pointer;
};

// Models the 16-entry object-pointer registry initialized by 0x420480, updated
// by 0x4208F0, and read by 0x420930. The native helpers do not bounds-check.
[[nodiscard]] NcarObjectRegistrationPlan register_ncar_object(
    const NcarObjectRegistrationObservation& observation) noexcept;
[[nodiscard]] NcarObjectLookupPlan lookup_ncar_object(
    const NcarObjectLookupObservation& observation) noexcept;

// Runtime observations used by the outer object dispatcher around 0x427630.
// Dynamic callback-table, object-registry, and transform-payload contents are
// supplied explicitly; they are populated at runtime and are not in the PE.
struct NcarInterpolatedDispatchObservation {
    NcarInterpolatedRecordSelectionObservation selection{};
    NcarInterpolatedUpdateObservation update{};
    std::uint32_t object_flags_0x04{};
    std::int32_t object_gate_0xed4{};
    std::int32_t object_frame_gate_0xed8{};
    std::uint32_t object_transform_index_0xec8{};
    std::uint32_t object_callback_index_0x510{};
    std::uint16_t object_counter_0x10{};
    // Resolution of the indexed target pointer passed to 0x41D1C0.
    bool indexed_target_pointer_nonnull{};
    std::optional<std::uint32_t> indexed_target_value_0x2c;
    std::optional<NcarRegisteredObjectTable> registered_object_table_0x507d70;
    // Runtime transform returned by 0x420930 for the +0xEC8 index.
    std::optional<std::array<float, 12>> registered_transform_0x38;
    // AL from the global +0x40 virtual callback; unknown means dispatch is
    // ambiguous and no downstream branch is synthesized.
    std::optional<bool> global_virtual_return_nonzero;
};

struct NcarInterpolatedDispatchPlan {
    std::uint32_t cleared_object_field_0xef0{};
    std::uint32_t object_flags_after_gate_0x04{};
    std::uint32_t indexed_target_table_byte_offset{};
    bool indexed_target_callback_0x41d1c0_called{true};
    bool indexed_target_stores_valid{};
    std::uint32_t indexed_target_write_0x04{};
    std::uint32_t indexed_target_write_0x08{};
    bool indexed_target_null_dereference{};
    bool registered_transform_lookup_0x420930_called{};
    bool registered_object_table_observation_available{};
    bool registered_object_table_observation_missing{};
    NcarObjectLookupPlan registered_object_lookup{};
    bool registered_object_out_of_table_access_unmodeled{};
    bool registered_transform_null_dereference{};
    bool registered_transform_copy_attempted{};
    bool registered_transform_copy_valid{};
    std::array<float, 12> copied_transform_0x38{};
    std::optional<std::uint32_t> object_flags_after_virtual_callback_0x04;
    bool virtual_callback_branch_known{};
    bool interpolation_time_gate_passed{};
    bool selector_0x427630_called{};
    NcarInterpolatedRecordSelectionPlan selection{};
    std::optional<NcarInterpolatedUpdatePlan> interpolated_update;
    bool tail_virtual_callback_0x38_called{};
    std::uint16_t incremented_object_counter_0x10{};
};

// Models the 0x427DA3 caller operands and indexed ring selection in 0x427630,
// including signed frame-step arithmetic and the 0x3c-byte record stride.
// Invalid spans are rejected before copying; native inputs are assumed valid.
[[nodiscard]] NcarInterpolatedRecordSelectionPlan
select_ncar_interpolated_records(
    const NcarInterpolatedRecordSelectionObservation& observation) noexcept;

// Projects the observed dispatch gates and side effects surrounding
// 0x427630. Runtime-registered transform data and unresolved pointers remain
// explicit observations rather than fabricated static values.
[[nodiscard]] NcarInterpolatedDispatchPlan dispatch_ncar_interpolated_object(
    const NcarInterpolatedDispatchObservation& observation) noexcept;

// Projects one five-byte wheel sample update in 0x444240. The follow-up
// effect and basis helpers are reported as calls; their additional writes are
// not synthesized here.
[[nodiscard]] NcarInterpolatedWheelPlan update_ncar_interpolated_wheel(
    const NcarInterpolatedWheelObservation& observation) noexcept;

// Projects scalar, flag, transform, velocity, wheel-record, and tail-byte
// effects from 0x432420. Lower-level wheel basis/contact helpers remain separate.
[[nodiscard]] NcarInterpolatedObjectPlan update_ncar_interpolated_object(
    const NcarInterpolatedObjectObservation& observation) noexcept;

// Composes the direct caller 0x427B10 around the object update.
[[nodiscard]] NcarInterpolatedUpdatePlan update_ncar_interpolated_state(
    const NcarInterpolatedUpdateObservation& observation) noexcept;

// Bytes in the target object at +0x574 + n*0x1A4, written by the
// mode-3 branch of the 0x439200 callback.
using NcarCallbackGateBytes = std::array<std::uint8_t, 4>;

struct NcarRotationDecode {
    std::array<float, 4> quaternion{};
    bool zero_vector_branch{};
    bool fdiv_compatibility_enabled{};
};

struct NcarRotationEncode {
    std::array<std::uint16_t, 3> rotation{};
    bool zero_sine_branch{};
    bool fdiv_compatibility_enabled{};
};

struct NcarPackedPayload24 {
    std::array<std::byte, 0x18> bytes{};
    // Bits written as zero here were not assigned by 0x49E210: bit 7 of byte
    // 0x16 and all of byte 0x17. Their runtime stack values are unspecified.
    std::array<std::byte, 0x18> unspecified_bit_mask{};
    bool fdiv_compatibility_enabled{};
};

struct NcarPackedPayloadApplyResult {
    std::uint32_t expanded_tick{};
    NcarRotationDecode rotation{};
};

// Reconstructs the 16-bit tick expansion in 0x49E1D0. The candidate is
// selected from the current tick's high word, then adjusted to the nearest
// representation within the signed half-range.
[[nodiscard]] std::uint32_t expand_ncar_wrapped_tick(
    std::uint16_t wrapped_tick,
    std::uint32_t current_tick) noexcept;

// Converts the expanded tick using 0x420A90's epoch globals. tick_delta uses
// the binary's signed 32-bit wrapped subtraction before the float conversion.
[[nodiscard]] float ncar_seconds_from_tick(
    std::uint32_t expanded_tick,
    std::uint32_t epoch_tick_origin,
    float epoch_seconds_origin) noexcept;

// Decodes the three unsigned 16-bit rotation values consumed by 0x49E090.
// Long-double intermediates approximate x87; the compatibility path flag is
// reported, though the Pentium FDIV correction itself is not emulated.
[[nodiscard]] NcarRotationDecode decode_ncar_rotation16(
    const std::array<std::uint16_t, 3>& rotation,
    bool fdiv_compatibility_enabled) noexcept;

// Encodes quaternion components using the acos/sin axis extraction and
// [-20,20] quantizer in 0x49E370. Finite quaternion-like inputs are expected.
[[nodiscard]] NcarRotationEncode encode_ncar_rotation16(
    const std::array<float, 4>& quaternion,
    bool fdiv_compatibility_enabled) noexcept;

// Reconstructs the meaningful bytes written by 0x49E210 from the 62-byte
// record projection. tick_origin and seconds_origin are DAT_00508194 and
// DAT_00508190. Unassigned scratch bits are zero-filled and identified in
// unspecified_bit_mask.
[[nodiscard]] NcarPackedPayload24 encode_ncar_packed_payload24(
    const NcarSampleRecord& record,
    std::uint32_t tick_origin,
    float seconds_origin,
    bool fdiv_compatibility_enabled) noexcept;

// Applies the fields written by 0x49DF60 to an existing destination record.
// Its +0x00 time and unknown/unwritten bytes are preserved as the binary does.
[[nodiscard]] NcarPackedPayloadApplyResult decode_ncar_packed_payload24(
    std::span<const std::byte, 0x18> payload,
    std::uint32_t current_tick,
    std::uint32_t tick_origin,
    float seconds_origin,
    NcarSampleRecord& destination,
    bool fdiv_compatibility_enabled) noexcept;

struct NcarSampleWindow {
    std::array<NcarSampleRecord, 3> records{};
};

struct NcarSampleBuffer {
    std::array<std::byte, ncar_sample_window_bytes> bytes{};
    int record_mode_0xec8{};
};

struct NcarSampleUpdate {
    bool accepted{};
    bool run_post_accept_helpers{};
    int mode_after{};
};

struct NcarCallbackQueueState {
    float last_payload_time_0xf30{};
};

struct NcarCallbackQueueObservation {
    float current_time{};
    bool event_processor_active_0x49be40{};
    std::array<std::byte, ncar_sample_record_bytes> payload_0x3e{};
    std::int32_t queue_clock{};
    std::int32_t queue_clock_offset{};
};

struct NcarCallbackQueueResult {
    bool enqueue_branch_reached{};
    bool enqueue_succeeded{};
    bool dequeue_attempted{};
    TimedEventPollResult dequeue{};
    bool copied_payload_has_sample_size{};
    NcarSampleUpdate sample_update{};
};

struct NcarSampleDerivative {
    std::array<float, 3> values{};
};

struct NcarHistoryDerivative {
    std::array<float, 3> values{};
    bool normalized_to_9_81{};
};

struct NcarInterpolationRamp {
    float value_0xf24{};
    float rate_0xf28{};
};

struct NcarSampleDynamicsState {
    NcarSampleDerivative current_derivative_0xecc{};
    std::array<float, 3> previous_derivative_0xed8{};
    std::array<float, 3> older_derivative_0xee4{};
    NcarHistoryDerivative acceleration_0xef0{};
    NcarInterpolationRamp interpolation_ramp{};
};

struct NcarSampleDynamicsResult {
    NcarSampleDerivative current_derivative{};
    NcarHistoryDerivative acceleration{};
    bool fdiv_compatibility_enabled{};
};

struct NcarForceMomentAccumulator {
    std::array<float, 3> force_0x24c{};
    std::array<float, 3> moment_0x258{};
};

// Models the shared 0x43E6F0 accumulator called by the NCAR integrator.
// A near-zero vector is ignored; otherwise force and (point-center) x force
// are added to the object's +0x24C and +0x258 vector fields.
[[nodiscard]] bool accumulate_ncar_force_moment(
    NcarForceMomentAccumulator& accumulator,
    const std::array<float, 3>& vector,
    const std::array<float, 3>& application_point,
    const std::array<float, 3>& center) noexcept;

struct NcarPointTransformFields {
    std::array<float, 3> linear_0x234{};
    std::array<float, 3> angular_0x240{};
    std::array<float, 3> center_0x5c{};
};

// Models 0x43E9B0: linear velocity at an application point, including the
// angular-velocity cross product relative to the body's +0x5C center.
[[nodiscard]] std::array<float, 3> transform_ncar_vector_at_point(
    const NcarPointTransformFields& body,
    const std::array<float, 3>& application_point) noexcept;

// Models 0x43E7F0's unconditional vector addition into moment +0x258.
void accumulate_ncar_moment_vector(
    NcarForceMomentAccumulator& accumulator,
    const std::array<float, 3>& vector) noexcept;

struct NcarPointImpulseAccumulator {
    float duration_0x2ac{};
    std::array<float, 3> impulse_time_0x294{};
    std::array<float, 3> moment_time_0x2a0{};
};

// Models 0x43E830's duration-scaled impulse and point-moment accumulators.
void accumulate_ncar_point_impulse(
    NcarPointImpulseAccumulator& accumulator,
    const std::array<float, 3>& impulse,
    const std::array<float, 3>& application_point,
    const std::array<float, 3>& center,
    float duration) noexcept;

struct NcarIntegratorBookkeepingState {
    std::int32_t counter_0xd78{};
    float sampled_time_0xd70{};
    float sum_0xeb0{};
    std::uint32_t copied_value_0xd64{};
    std::uint32_t copied_value_0xd68{};
};

struct NcarIntegratorBookkeepingInput {
    float time_callback_result{};
    float addend_0x5f8{};
    float addend_0x79c{};
    std::uint32_t source_0xcbc{};
    std::uint32_t source_0xcb4{};
};

struct NcarIntegratorBookkeepingResult {
    bool counter_reset_to_zero{};
    bool substep_helper_0x4323e0_reached{};
    float substep_helper_argument{};
};

// Models the direct bookkeeping stores in 0x431150 after its four repeated
// 0x442530 updates. The conditional helper call remains an effect flag.
[[nodiscard]] NcarIntegratorBookkeepingResult advance_ncar_integrator_bookkeeping(
    NcarIntegratorBookkeepingState& state,
    const NcarIntegratorBookkeepingInput& input) noexcept;

struct NcarIntegratorChildControl {
    std::uint32_t value_at_child_0x5c{};
    std::uint32_t value_at_child_0x64{};
    float selected_value_0x18{};
    std::int32_t previous_index_0x1c{};
    std::int32_t selection_index_0x24{};
    float negative_index_value_0x2c{};
    float ramp_value_0x60{};
    std::uint8_t enabled_0x58{};
    std::int32_t animation_state_0x68{};
};

struct NcarIntegratorChildControlResult {
    bool animation_control_0x43fae0_reached{};
    std::uint8_t animation_control_byte{};
};

// Applies the two direct child-object setters at 0x43FF70/0x43FF80 in order,
// then reports the 0x43FAE0 controller call; its state machine is modeled below.
[[nodiscard]] NcarIntegratorChildControlResult setup_ncar_integrator_child_control(
    NcarIntegratorChildControl& child,
    std::uint32_t value_0xdec,
    std::uint32_t value_0xde0,
    std::uint8_t mode_byte_0x534) noexcept;

struct NcarAnimationControllerObservation {
    std::uint8_t mode_byte{};
    bool child_override_flag_0x41{};
    float selected_value_for_index{};
    float callback_value_0x4c{};
    float callback_value_0x50{};
    float child_value_0x34{};
    float child_value_0x3c{};
    bool fdiv_compatibility_enabled{};
};

struct NcarAnimationControllerTargets {
    std::uint32_t value_target_0x2c{};
    float ramp_target_0x00{};
};

struct NcarAnimationControllerResult {
    bool wrote_value_target_0x2c{};
    std::uint32_t child_value_callback_argument{};
    bool wrote_ramp_target_0x00{};
    float child_ramp_setter_value{};
    bool fdiv_compatibility_path_reached{};
};

// Reconstructs the state transitions and direct stores in 0x43FAE0. Its two
// setter targets are supplied separately; their direct writes are applied.
[[nodiscard]] NcarAnimationControllerResult advance_ncar_animation_controller(
    NcarIntegratorChildControl& child,
    NcarAnimationControllerTargets& targets,
    const NcarAnimationControllerObservation& observation) noexcept;

struct NcarLinkedTargetFields {
    std::array<std::uint32_t, 12> copied_state_0x38{};
    std::array<float, 3> subtractor_0x44{};
    std::array<float, 3> multiplier_0x50{};
    std::array<float, 3> accumulator_0x5c{};
    std::array<float, 3> vector_0x234{};
    std::array<float, 3> cleared_vector_0x240{};
    float last_update_time_0x478{};
};

struct NcarLinkedTargetUpdate {
    bool copied_state{};
    bool updated_vector_fields{};
};

struct NcarLinkedCallbackResult {
    bool called_target_update{};
    NcarLinkedTargetUpdate target_update{};
};

struct NcarSampleLinkedTargetObservation {
    std::span<const std::byte, ncar_sample_record_bytes> incoming_sample;
    float current_time{};
    std::array<std::uint32_t, 12> source_state_0x38{};
    std::array<float, 3> source_vector_0x234{};
    bool target_lookup_succeeded{};
    bool type_name_is_plane{};
};

struct NcarSampleLinkedTargetResult {
    NcarSampleUpdate sample_update{};
    bool linked_target_gate_passed{};
    NcarLinkedTargetUpdate target_update{};
};

struct NcarTimedInterpolation {
    std::array<float, 4> current_0xefc{};
    std::array<float, 4> target_0xf0c{};
    std::array<float, 3> output_0x240{};
    float interpolation_start_0xf1c{};
};

struct NcarRotationBlendState {
    std::array<float, 9> matrix_0x38{};
    float interpolation_weight_0xefc{};
    float interpolation_fraction_0xf24{};
    float interpolation_step_0xf28{};
    // Four observed DWORDs at +0x19C in slots +0x554+n*0x1A4.
    std::array<std::uint32_t, 4> slot_value_0x19c{};
};

struct NcarRotationBlendObservation {
    // Slot 0 is newest; slots 1 and 2 are progressively older samples.
    std::array<float, 3> sample_times{};
    std::array<std::array<float, 4>, 3> sample_quaternions{};
    float now{};
    // Supplies the value of global DAT_004E9754 for the helper path.
    bool fdiv_compatibility_enabled{};
};

struct NcarRotationBlendResult {
    std::array<float, 2> sample_fractions{};
    std::array<std::array<float, 4>, 2> pair_quaternions{};
    std::array<float, 4> interpolated_sample_quaternion{};
    std::array<float, 4> object_quaternion_before{};
    std::array<float, 4> object_quaternion_after{};
    std::array<float, 9> output_matrix{};
    float slot_weight{};
    float object_blend_weight{};
    bool object_matrix_positive_trace_branch{};
    std::int8_t object_matrix_diagonal_branch_axis{-1};
    bool first_pair_source_negated{};
    bool second_pair_source_negated{};
    std::array<bool, 2> pair_near_parallel_copy{};
    bool sample_pair_source_negated{};
    bool sample_pair_near_parallel_copy{};
    bool object_blend_source_negated{};
    bool object_blend_near_parallel_copy{};
    bool fdiv_compatibility_branch{};
};

// Reconstructs the observed interpolation, slerp, and matrix writes in
// 0x439DB0. Division/trigonometric compatibility is reported, not emulated.
[[nodiscard]] NcarRotationBlendResult advance_ncar_rotation_blend(
    NcarRotationBlendState& state,
    const NcarRotationBlendObservation& observation) noexcept;

struct NcarSampleCallbackSlotState {
    std::uint32_t value_0x13c{};
    std::uint32_t value_0x124{};
    std::uint8_t byte_0x168{};
    std::uint8_t byte_0x020{};
    std::uint32_t value_0x0e8{};
    std::uint32_t value_0x0dc{};
    std::uint32_t value_0x0a4{};
    std::uint32_t value_0x10c{};
    std::uint32_t value_0x16c{};
    std::uint32_t value_0x158{};
};

struct NcarSampleCallbackRenderChildState {
    std::array<std::uint32_t, 3> current_position_bits_0x64{};
    std::array<std::uint32_t, 3> cached_position_bits_0x20{};
    std::uint8_t dirty_flag_0x88{};
};

struct NcarSampleCallbackTargetState {
    std::array<float, 9> orientation_0x38{};
    std::array<float, 3> position_0x5c{};
    std::array<float, 6> velocity_0x234{};
    std::uint32_t zeroed_field_0xd70{};
    float timer_0x50c{};
    float callback_time_0xe44{};
    bool flag_0xe3c{};
    bool flag_0xe3d{};
    std::uint32_t embedded_value_0xcbc{};
    std::uint32_t copied_value_0xd64{};
    std::uint32_t field_0xc0c{};
    std::uint32_t child_count_0x1ec{};
    std::vector<std::uint8_t> child_flags_0x98;
    std::uint8_t object_byte_0x48c{};
    std::uint8_t object_byte_0x48d{};
    std::array<std::uint32_t, 8> object_dwords_0x48e{};
    std::array<std::uint8_t, 4> callback_slot_flags_0x574{};
    std::array<NcarSampleCallbackSlotState, 4> callback_slots_0x554{};
    std::uint8_t manager_index_0x510{};
    std::array<NcarResetResourceGroupInput, 5> resource_groups{};
    std::array<std::uint32_t, 5> resource_descriptor_0x10{};
    std::array<std::uint8_t, 5> resource_flag_0x15{};
    bool render_group_pointer_present{true};
    std::vector<NcarSampleCallbackRenderChildState> render_children;
};

struct NcarSampleCallbackVirtualEffect {
    bool reached{};
    bool fdiv_compatibility_enabled{};
    bool leading_child_virtual_0x14_reached{};
    std::size_t render_group_virtual_0x14_passes{2U};
    std::size_t render_child_virtual_0x14_calls{};
    bool invalid_render_group_pointer_dereference_reached{};
    bool embedded_zero_setter_reached{};
    bool embedded_value_copied{};
    bool child_flags_update_reached{};
    std::size_t child_flags_written{};
    bool child_flag_span_covers_count{};
    bool callback_slot_reset_helper_calls_reached{};
    std::uint32_t callback_slot_reset_helper_call_count{4U};
    bool callback_slot_clear_reached{};
    std::uint32_t callback_slot_clear_count{4U};
    std::uint32_t callback_slot_state_reset_count{4U};
    std::uint32_t child_resource_pair_pass_count{5U};
    bool child_resource_update_reached{};
    std::uint8_t resource_groups_visited{};
    std::array<std::size_t, 5> resource_records_copied{};
    std::array<std::size_t, 5> resource_records_unprocessed{};
    bool resource_group_inputs_complete{true};
    bool child_virtual_0x04_reached{};
    bool trailing_child_virtual_0x14_reached{};
    bool event_queue_append_reached{};
    std::uint8_t event_id{7U};
    std::array<std::uint8_t, 2> event_payload{};
    std::uint32_t event_payload_size{2U};
};

struct NcarTimedCallbackState {
    bool gate_0xf20{};
    float prior_time_0xf2c{};
    bool sample_flag_0x39{};
    bool fdiv_compatibility_enabled{};
    NcarSampleCallbackTargetState virtual_target{};
};

struct NcarTimedCallbackResult {
    bool main_chain_reached{};
    bool interpolation_0x43a190_reached{};
    bool prediction_0x439a10_reached{};
    bool final_update_0x439db0_reached{};
    bool sample_callback_0x439d30_reached{};
    bool sample_flag_cleared{};
    bool sample_virtual_method_0x40_reached{};
    std::array<float, 3> sample_vector_0x1c{};
    std::array<float, 2> callback_components{};
    bool callback_time_written{};
    bool gate_0xf20_set{};
    NcarSampleCallbackVirtualEffect virtual_effect{};
};

struct NcarSampleCallbackEffect {
    bool virtual_method_0x40_reached{};
    std::array<float, 3> sample_vector_0x1c{};
    std::array<float, 2> callback_components{};
    bool callback_time_written{};
    bool gate_0xf20_set{};
    NcarSampleCallbackVirtualEffect virtual_effect{};
};

// Applies 0x439D30 and the NCAR vtable +0x40 target at 0x433620. Resource
// helper internals and the secondary object's virtual methods remain reported
// as call effects. The sample-window +0x39 flag is cleared by the caller.
[[nodiscard]] NcarSampleCallbackEffect apply_ncar_sample_callback(
    NcarTimedCallbackState& state,
    const NcarSampleRecord& sample,
    float now) noexcept;

// Models both independent gates in 0x43A130 and its direct flag clear.
// Also models 0x439D30's sample-derived callback arguments and direct latch
// writes. The virtual +0x40 method's state effect remains external.
[[nodiscard]] NcarTimedCallbackResult advance_ncar_timed_callback(
    NcarTimedCallbackState& state,
    const NcarSampleRecord& sample,
    float now) noexcept;

struct NcarPositionPredictionState {
    std::array<float, 3> position_0x5c{};
    std::array<float, 3> velocity_0x234{};
    std::array<float, 3> acceleration_0xef0{};
    std::array<float, 3> blend_weights_0xf00{};
    float value_0xefc{};
    float value_0xf0c{};
};

struct NcarPositionPredictionResult {
    std::array<float, 3> predicted_position{};
    std::array<float, 3> sample_velocity{};
    std::array<float, 3> position_error{};
    float squared_position_error{};
    bool reset_gate_passed{};
    bool reset_callback_0x439d30_reached{};
    NcarSampleCallbackEffect reset_callback_effect{};
    bool position_blend_applied{};
    bool velocity_blend_applied{};
    bool fdiv_compatibility_enabled{};
};

struct NcarTimedFrameState {
    NcarTimedCallbackState callback{};
    NcarTimedInterpolation interpolation{};
    NcarPositionPredictionState position{};
    NcarRotationBlendState rotation{};
};

struct NcarTimedFrameResult {
    bool main_chain_reached{};
    bool interpolation_0x43a190_reached{};
    NcarPositionPredictionResult position{};
    bool rotation_0x439db0_reached{};
    NcarRotationBlendResult rotation{};
    bool sample_callback_0x439d30_reached{};
    NcarSampleCallbackEffect sample_callback_effect{};
    bool sample_flag_cleared{};
};

// Models 0x439A10's quadratic position extrapolation, three-sample derivative,
// reset threshold, and per-axis state blends. FDIV correction is reported only.
[[nodiscard]] NcarPositionPredictionResult advance_ncar_position_prediction(
    NcarPositionPredictionState& state,
    NcarTimedCallbackState& callback_state,
    const NcarSampleWindow& samples,
    float now,
    bool fdiv_compatibility_enabled) noexcept;

// Composes the 0x43A130 timed callback in observed order: 0x43A190,
// 0x439A10, 0x439DB0, then the independent +0x39 callback and clear.
[[nodiscard]] NcarTimedFrameResult advance_ncar_timed_frame(
    NcarTimedFrameState& state,
    const NcarSampleWindow& samples,
    float now,
    bool fdiv_compatibility_enabled) noexcept;

[[nodiscard]] bool should_update_ncar_linked_target(
    int record_mode,
    std::uint8_t current_gate_byte,
    std::uint8_t prior_gate_byte,
    bool target_present) noexcept;

// Decodes only fields directly read/written by the analyzed NCAR helpers.
// Float-like values are returned as raw little-endian IEEE bit patterns.
[[nodiscard]] NcarSampleWindow decode_ncar_sample_window(
    std::span<const std::byte, ncar_sample_window_bytes> bytes) noexcept;

// Reproduces the acceptance, overlapping three-slot shift, timestamp write,
// and +0xEC8 transition in 0x439490. The input record must not alias buffer.
[[nodiscard]] NcarSampleUpdate update_ncar_sample_buffer(
    NcarSampleBuffer& buffer,
    std::span<const std::byte, ncar_sample_record_bytes> incoming,
    float current_time) noexcept;

// Composes the accepted-sample path through 0x439110 to 0x43AD70. The target
// lookup result and source-object fields remain explicit observations.
[[nodiscard]] NcarSampleLinkedTargetResult update_ncar_sample_linked_target(
    NcarSampleBuffer& buffer,
    NcarLinkedTargetFields& target,
    const NcarSampleLinkedTargetObservation& observation) noexcept;

// Mirrors sample bytes +0x3A..+0x3D into normalized 0/1 bytes: zero maps to
// zero, and every nonzero input maps to one.
void apply_ncar_callback_gate_bytes(
    NcarCallbackGateBytes& object_bytes,
    const NcarSampleRecord& sample) noexcept;

// Models the producer/queue/consumer prefix of the NCAR frame callback at
// 0x439200. Payload creation is supplied as bytes because 0x4393B0 and its
// simulation-side producers remain separate effects. The sample buffer is
// updated only for a successfully copied, exact 0x3E-byte queue payload.
[[nodiscard]] NcarCallbackQueueResult advance_ncar_callback_queue(
    NcarCallbackQueueState& state,
    NcarSampleBuffer& sample_buffer,
    TimedEventQueue& event_queue,
    const NcarCallbackQueueObservation& observation);

// Models the first vector written by 0x439520: the derivative at sample 0 of
// the quadratic through the three raw sample vectors. Uses host long double
// intermediates where available; it does not emulate target x87 instruction
// rounding. The optional flag corresponds to global 0x4E9754 and applies the
// observed finite-normal CRT operand-scaling branch when its significand gate
// matches. Exceptional cases remain host math.
[[nodiscard]] NcarSampleDerivative derive_ncar_sample_vector(
    const NcarSampleWindow& samples,
    bool fdiv_compatibility_enabled = false) noexcept;

// Models the second triplet in 0x439520 from the current and two prior
// derivative vectors, including its squared-magnitude gate and 9.81 scaling.
// Host long double improves intermediate precision where it is wider than
// double. It also rounds the two reused time deltas through float stack
// temporaries as the target does. Remaining x87 ordering, exceptional CRT
// cases, and exact Pentium FDIV behavior are not fully emulated.
[[nodiscard]] NcarHistoryDerivative derive_ncar_history_derivative(
    const NcarSampleWindow& samples,
    const NcarSampleDerivative& current,
    const NcarSampleDerivative& previous,
    const NcarSampleDerivative& older,
    bool fdiv_compatibility_enabled = false) noexcept;

// Applies the final +0xF24 decrement and +0xF28 rate calculation in 0x439520.
// The optional flag corresponds to global 0x4E9754.
void advance_ncar_interpolation_ramp(NcarInterpolationRamp& ramp,
                                     float first_parameter,
                                     float second_parameter,
                                     bool fdiv_compatibility_enabled = false) noexcept;

// Applies the ordered history shift, derivative writes, gravity cap, and ramp
// update performed by 0x439520 for one accepted three-sample window.
[[nodiscard]] NcarSampleDynamicsResult advance_ncar_sample_dynamics(
    NcarSampleDynamicsState& state,
    const NcarSampleWindow& samples,
    bool fdiv_compatibility_enabled = false) noexcept;

// Models the numeric writes in 0x43AD70, reached through 0x439110. All field
// labels are offsets; type_name_is_plane is the observed string comparison.
[[nodiscard]] NcarLinkedTargetUpdate update_ncar_linked_target(
    NcarLinkedTargetFields& target,
    const std::array<std::uint32_t, 12>& state_0x38,
    const std::array<float, 3>& vector_0x234,
    float now,
    float sample_parameter,
    bool type_name_is_plane) noexcept;

// Models the gate/cache behavior in 0x434A80. In mode 11 the binary uses a
// global state block and a zero input vector; otherwise it uses the instance.
[[nodiscard]] NcarLinkedCallbackResult run_ncar_linked_callback(
    NcarLinkedTargetFields& target,
    std::uint8_t& cached_gate_byte,
    std::uint8_t gate_byte,
    bool linked_update_enabled,
    int global_mode,
    bool target_present,
    const std::array<std::uint32_t, 12>& instance_state_0x38,
    const std::array<float, 3>& instance_vector_0x234,
    const std::array<std::uint32_t, 12>& global_state,
    float now,
    bool type_name_is_plane) noexcept;

// Models the numeric portion of 0x43A190 using the first two sample times.
void advance_ncar_timed_interpolation(
    NcarTimedInterpolation& fields,
    float newest_sample_time,
    float previous_sample_time,
    float now) noexcept;

// Component-wise blend performed by 0x438D30: from*(1-weight)+to*weight.
[[nodiscard]] std::array<float, 3> blend_ncar_vector(
    const std::array<float, 3>& from,
    const std::array<float, 3>& to,
    const std::array<float, 3>& weights) noexcept;

// Models the time/input predicate in 0x4390C0. The time threshold is strict.
[[nodiscard]] bool ncar_elapsed_gate(float now, float record_time,
                                     int record_mode, bool input_is_one) noexcept;

// 0x438E10 is true only when now - timestamp is strictly greater than 0.5.
[[nodiscard]] bool ncar_half_second_elapsed(float now, float timestamp) noexcept;

// 0x439160 decrements +0x50C by 0.016 while it is above the observed
// bit-pattern threshold; otherwise it clears the field.
void decay_ncar_timer(NcarStateFields& fields) noexcept;

// Models 0x439190, including its 0x439160 timer update, one-time angular
// initialization, high-bit early return, and three zero-valued setter calls.
// Returns true for the branch that exits early when uint32(+0x238) exceeds
// 0x80000000; the three setter fields are untouched on that early return.
[[nodiscard]] bool initialize_ncar_rotation(NcarStateFields& fields) noexcept;

struct NcarCallbackMainObservation {
    float current_time{};
    bool input_time_gate_0x4390c0{};
    bool event_processor_active_0x49be40{};
    NcarSampleRecord current_sample{};
};

struct NcarCallbackMainResult {
    bool rotation_initializer_reached{};
    bool rotation_initializer_returned_early{};
    bool main_integrator_0x431150_reached{};
    bool integrator_prefix_reset_applied{};
    bool mode_3_branch_reached{};
    bool control_update_0x4330f0_reached{};
    bool mode_3_setter_values_written{};
    bool linked_callback_0x434a80_reached{};
    bool gate_bytes_written{};
    bool timer_set_to_one{};
    bool acceleration_reset_0x43a130_reached{};
    bool timer_decay_0x439160_reached{};
    bool exit_timer_overridden_to_0_8{};
};

struct NcarCallbackBookkeepingResult {
    NcarCallbackMainResult callback{};
    bool integrator_bookkeeping_reached{};
    NcarIntegratorBookkeepingResult integrator_bookkeeping{};
};

// Models the entry reset in 0x431150 when object dword +0x4FC is zero.
[[nodiscard]] bool apply_ncar_integrator_entry_reset(
    NcarStateFields& state) noexcept;

// Composes the input/mode branch and timer tail of 0x439200. It simulates the
// entry-reset prefix of 0x431150; the rest of that integrator and 0x43A130 are
// reported as reached effects, not simulated.
[[nodiscard]] NcarCallbackMainResult advance_ncar_callback_main(
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    const NcarCallbackMainObservation& observation) noexcept;

// Composes the same callback with 0x431150's post-wheel bookkeeping before
// the 0x439200 timer tail. The wheel-produced callback/addend values are inputs.
[[nodiscard]] NcarCallbackBookkeepingResult
advance_ncar_callback_main_with_bookkeeping(
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    NcarIntegratorBookkeepingState& bookkeeping_state,
    const NcarCallbackMainObservation& observation,
    const NcarIntegratorBookkeepingInput& bookkeeping_input) noexcept;

struct NcarFrameCallbackObservation {
    NcarCallbackQueueObservation queue{};
    NcarCallbackMainObservation main{};
    NcarIntegratorBookkeepingInput integrator_bookkeeping{};
};

struct NcarFrameCallbackResult {
    NcarCallbackQueueResult queue{};
    NcarCallbackBookkeepingResult main{};
};

// Composes the 0x439200 queue prefix, main input/mode dispatch, reached
// integrator bookkeeping and callback timer tail. Record lookup and wheel
// outputs remain observations.
[[nodiscard]] NcarFrameCallbackResult advance_ncar_frame_callback(
    NcarCallbackQueueState& queue_state,
    NcarSampleBuffer& sample_buffer,
    TimedEventQueue& event_queue,
    NcarStateFields& state,
    NcarCallbackGateBytes& object_gate_bytes,
    NcarIntegratorBookkeepingState& bookkeeping_state,
    const NcarFrameCallbackObservation& observation);

} // namespace viper
