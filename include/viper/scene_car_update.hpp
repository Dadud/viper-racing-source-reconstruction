#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

// Numeric states written at scene-object offset +0x83c by 0x465800.
// The original UI/renderer meanings of these values are not established.
enum class SceneCarState : int {
    state_0 = 0,
    state_1 = 1,
    state_2 = 2,
};

inline constexpr std::size_t scene_car_pose_float_count = 99;
using SceneCarPose = std::array<float, scene_car_pose_float_count>;

// Reproduces the state selection in 0x465800 using the values returned by
// 0x45c1f0 (selected car ordinal) and 0x45c200 (selection state).
[[nodiscard]] SceneCarState scene_car_state_for_update(
    int selected_car_ordinal,
    int scene_car_ordinal,
    int selection_state) noexcept;

// 0x463530 dispatches the scene object's vtable +0x24 update when the incoming
// 16-bit field differs from its cached dword, or when 0x425830 forces refresh.
[[nodiscard]] bool should_dispatch_scene_car_update(
    std::uint16_t incoming_field,
    std::uint32_t cached_field_0x38,
    bool force_update_0x425830) noexcept;

// Ordered operations in the outer scene-car dispatcher at 0x45BC20.
enum class SceneCarDispatcherOperationKind : std::uint8_t {
    timer_begin_0x415090,
    initialize_local_record_0x45dae0,
    acquire_update_block_0x425880,
    copy_shared_globals,
    parse_update_records_0x462d90,
    notify_scene_car_0x463530,
    update_car_string_record_0x45e010,
    timer_end_0x4150a0,
};

struct SceneCarDispatcherObjectInput {
    std::uint16_t incoming_field{};
    std::uint32_t cached_field_0x38{};
    bool force_update_0x425830{};
};

inline constexpr std::size_t scene_car_dispatch_shared_word_count = 12;
inline constexpr std::size_t scene_car_dispatch_local_record_count = 16;
inline constexpr std::size_t scene_car_dispatch_record_word_count = 14;
using SceneCarDispatchSharedWords =
    std::array<std::uint32_t, scene_car_dispatch_shared_word_count>;
using SceneCarDispatchRecordWords =
    std::array<std::uint32_t, scene_car_dispatch_record_word_count>;

struct SceneCarDispatcherOperation {
    SceneCarDispatcherOperationKind kind{};
    std::size_t index{};
    bool callback_dispatched{};
    SceneCarDispatchSharedWords shared_words{};
    std::array<std::uint32_t, 3> scalar_globals{};
    std::uint8_t byte_global{};
    SceneCarDispatchRecordWords record_words{};
    std::uint16_t incoming_field{};
    std::uint32_t resulting_cached_field{};
};

struct SceneCarDispatcherInput {
    // Result fields copied by 0x45BC20 from the 0x425880 stack block.
    SceneCarDispatchSharedWords shared_words{};
    // Values destined for globals 0x4DC1A0, 0x53A4B4, and 0x53A4B8, in order.
    std::array<std::uint32_t, 3> scalar_globals{};
    std::uint8_t byte_global{};
    // Global 0x539428 controls the notifier loop.
    std::int32_t scene_object_count{};
    std::span<const SceneCarDispatcherObjectInput> scene_objects{};
    // 0x53940C controls this loop; each local input is copied as 14 dwords.
    std::int32_t car_record_count{};
    std::span<const SceneCarDispatchRecordWords> car_records{};
};

struct SceneCarDispatcherPlan {
    std::vector<SceneCarDispatcherOperation> operations;
    SceneCarDispatchSharedWords resulting_shared_globals{};
    std::array<std::uint32_t, 3> resulting_scalar_globals{};
    std::uint8_t resulting_byte_global{};
    std::size_t scene_objects_examined{};
    std::size_t scene_callbacks_dispatched{};
    std::size_t car_records_examined{};
    bool scene_object_inputs_exhausted{};
    bool car_record_inputs_exhausted{};
    bool local_car_record_overread_possible{};
};

// Projects the full call order in 0x45BC20. Binary-owned state acquired by
// 0x425880 and indirect callback effects remain explicit inputs/operations.
[[nodiscard]] SceneCarDispatcherPlan plan_scene_car_dispatcher(
    const SceneCarDispatcherInput& input);

inline constexpr std::size_t scene_car_byte_handler_entry_capacity = 16;
struct SceneCarByteHandlerEntry {
    std::uint32_t callback_address{};
    std::uint8_t tag{};
    std::array<std::uint8_t, 3> trailing_bytes{};
};
static_assert(sizeof(SceneCarByteHandlerEntry) == 8);

struct SceneCarByteHandlerRegistry {
    std::vector<SceneCarByteHandlerEntry> entries;
    bool capacity_overrun_possible{};
};

struct SceneCarByteHandlerRegistration {
    std::uint32_t callback_address{};
    std::uint8_t tag{};
};

[[nodiscard]] std::array<SceneCarByteHandlerRegistration, 2>
scene_car_handlers_registered_by_scene_context() noexcept;
[[nodiscard]] SceneCarByteHandlerRegistration
scene_car_handler_registered_by_effect_context() noexcept;
// The three discovered native callbacks return fixed payload sizes on every
// observed branch; unknown handlers require a supplied callback observation.
[[nodiscard]] std::optional<std::int32_t>
scene_car_byte_handler_payload_bytes(std::uint32_t callback_address) noexcept;
void reset_scene_car_byte_handler_registry(SceneCarByteHandlerRegistry& registry) noexcept;
void register_scene_car_byte_handler(
    SceneCarByteHandlerRegistry& registry,
    SceneCarByteHandlerEntry handler);

struct SceneCarByteHandlerRemovalResult {
    bool removed{};
    bool reporter_called{};
};
[[nodiscard]] SceneCarByteHandlerRemovalResult remove_scene_car_byte_handler(
    SceneCarByteHandlerRegistry& registry,
    std::uint32_t callback_address) noexcept;

enum class SceneCarByteParserStatus : std::uint8_t {
    complete,
    stopped_on_unregistered_tag,
    stopped_by_handler_sentinel,
    payload_input_exhausted,
    callback_result_input_exhausted,
    invalid_negative_cursor,
};

struct SceneCarByteCallbackOperation {
    std::size_t payload_offset{};
    std::size_t handler_index{};
    std::uint32_t callback_address{};
    std::uint8_t tag{};
    std::int32_t returned_payload_bytes{};
    bool payload_length_recovered{};
};

struct SceneCarByteParserInput {
    std::int32_t declared_payload_bytes{};
    std::span<const std::uint8_t> payload{};
    const SceneCarByteHandlerRegistry* registry{};
    // Results are consumed only for matching handlers, in table order.
    std::span<const std::int32_t> callback_results{};
};

struct SceneCarByteParserPlan {
    SceneCarByteParserStatus status{SceneCarByteParserStatus::complete};
    std::int32_t cursor_after{};
    std::size_t callback_results_consumed{};
    std::vector<SceneCarByteCallbackOperation> callbacks;
};

// Models 0x462D90's ordered tag scan. Handler reads and writes are external;
// callback return values supply each observed payload length or -1 sentinel.
[[nodiscard]] SceneCarByteParserPlan plan_scene_car_byte_parser(
    const SceneCarByteParserInput& input);

inline constexpr std::size_t scene_car_effect_tag_zero_pool_size = 32;
enum class SceneCarEffectTagZeroStatus : std::uint8_t {
    duplicate_suppressed,
    generator_dispatched,
    pool_observation_exhausted,
    null_pool_entry_fault,
    selected_slot_observation_missing,
    selected_slot_null_fault,
};

struct SceneCarEffectTagZeroInput {
    // Value at each allocated effect object +0x04; null models a null pointer
    // in the 32-slot global array (whose shipped scan has no null check).
    std::span<const std::optional<std::uint32_t>> existing_effect_keys{};
    std::uint32_t payload_key_0x18{};
    std::int32_t cursor_0x540bf0{};
    std::optional<bool> selected_slot_nonnull;
};

struct SceneCarEffectTagZeroPlan {
    SceneCarEffectTagZeroStatus status{SceneCarEffectTagZeroStatus::pool_observation_exhausted};
    std::array<bool, scene_car_effect_tag_zero_pool_size> key_matches{};
    std::size_t entries_compared{};
    std::size_t selected_slot{};
    std::int32_t cursor_after{};
    bool cursor_written{};
    bool generator_0x468af0_called{};
    std::optional<std::int32_t> returned_payload_bytes;
};

// Projects tag 0 handler 0x4692A0's duplicate scan and 32-slot effect cursor.
// Particle generation by 0x468AF0 remains its own model boundary.
[[nodiscard]] SceneCarEffectTagZeroPlan plan_scene_car_effect_tag_zero_handler(
    const SceneCarEffectTagZeroInput& input) noexcept;

struct SceneCarDistanceThreshold {
    float distance{};
    std::array<std::uint32_t, 2> opaque{};
};
static_assert(sizeof(SceneCarDistanceThreshold) == 0x0C);

struct SceneCarDispatchIndicesInput {
    std::array<float, 3> object_position{};
    std::array<float, 3> camera_position{};
    std::span<const SceneCarDistanceThreshold> distance_thresholds_0x288{};
    std::int32_t global_index_adjustment{};
};

struct SceneCarDispatchIndices {
    float distance_from_camera{};
    std::int32_t index_0x510{};
    std::int32_t index_0x514{};
};

// Models the threshold scan and index adjustment in the scene vtable +8
// wrapper at 0x463EB0; vtable slot +0x28 is the observed false-return stub.
// Threshold records have a 0x0C-byte stride.
[[nodiscard]] SceneCarDispatchIndices select_scene_car_dispatch_indices(
    const SceneCarDispatchIndicesInput& input) noexcept;

// 0x465800 copies 99 consecutive floats from its input to scene-object +0x3c.
void copy_scene_car_pose(const SceneCarPose& source, SceneCarPose& destination) noexcept;

struct SceneCarWheelTransformInput {
    std::array<float, 9> basis{}; // scene object +0x04 through +0x24
    std::array<float, 3> translation{}; // scene object +0x28 through +0x30
    float front_vertical_offset_0x7ac{};
    float rear_vertical_offset_0x7b0{};
    std::array<float, 3> wheel_local_position{};
    std::array<float, 3> previous_wheel_position{};
    std::size_t wheel_index{};
};

struct SceneCarWheelTransformOutput {
    std::array<float, 9> basis{};
    std::array<float, 3> position{};
    std::array<float, 3> position_delta{};
    std::array<float, 12> mirrored_transform{};
};

// Models the numeric transform in the four-entry loop at 0x465800. The
// subsequent matrix multiplications alias their output with an input, and
// 0x402DD0 returns without writing for those aliases.
[[nodiscard]] SceneCarWheelTransformOutput transform_scene_car_wheel(
    const SceneCarWheelTransformInput& input) noexcept;

inline constexpr std::size_t scene_wheel_state_word_count = 16;
using SceneWheelStateWords = std::array<std::uint16_t, scene_wheel_state_word_count>;

struct SceneWheelStateDelta {
    SceneWheelStateWords rising_edges{};
    std::size_t rising_bit_count{};
    bool has_falling_edge{};
    bool has_rising_edge{};
};

enum class SceneWheelCallbackOperationKind : std::uint8_t {
    resource_event_0x447e50,
    resource_refresh_0x447ee0,
};

struct SceneWheelCallbackOperation {
    SceneWheelCallbackOperationKind kind{};
    std::int32_t first_id{};
    std::int32_t second_id{};
    // For 0x447E50: left, top, right, bottom.
    std::array<float, 4> rectangle{};
};

inline constexpr std::size_t scene_wheel_callback_operation_capacity = 259;
struct SceneWheelCallbackPlan {
    std::array<SceneWheelCallbackOperation,
        scene_wheel_callback_operation_capacity> operations{};
    std::size_t operation_count{};
};

// Reproduces the sixteen-word comparison in 0x465800. The words are kept
// opaque: their individual bit meanings have not been identified. A state
// reset clears the saved words before comparison, as the original does.
[[nodiscard]] SceneWheelStateDelta compare_scene_wheel_state(
    const SceneWheelStateWords& previous,
    const SceneWheelStateWords& incoming,
    bool source_active,
    bool state_reset) noexcept;

// Projects the ordered 0x447E50/0x447EE0 calls in 0x465800. IDs are the raw
// scene-object fields +0x308, +0x30C, and +0x310. Grid dimensions use the
// observed float at 0x4C940C; bit labels remain intentionally opaque.
[[nodiscard]] SceneWheelCallbackPlan plan_scene_wheel_callback_operations(
    const SceneWheelStateDelta& delta,
    std::int32_t id_0x308,
    std::int32_t id_0x30c,
    std::int32_t id_0x310,
    float grid_scale_0x4c940c) noexcept;

struct SceneWheelAuxiliaryState {
    std::array<float, 3> start_0x00{};
    std::array<float, 3> end_0x0c{};
    bool resource_present_0x18{};
    float elapsed_0x20{};
    std::uint32_t allocation_size_0x24{};
    bool transition_active_0x28{};
    // Three position triplets at resource offsets 0x00, 0x0C, 0x18, 0x24.
    std::array<float, 12> resource_positions{};
    std::uint32_t resource_lifecycle_state_0x30{};
    std::size_t resource_slot_index{};
    bool resource_slot_known{};
};

struct SceneWheelResourceSlot {
    std::array<float, 12> positions_0x00{};
    std::uint32_t lifecycle_state_0x30{};
    std::uint32_t allocation_size_0x34{};
};
static_assert(sizeof(SceneWheelResourceSlot) == 0x38);

// 0 is invisible/free. 1 is protected from reuse and survives manager reset.
// 2 is retired but remains drawable until reused or cleared by reset.
inline constexpr std::uint32_t scene_wheel_resource_slot_free = 0U;
inline constexpr std::uint32_t scene_wheel_resource_slot_live = 1U;
inline constexpr std::uint32_t scene_wheel_resource_slot_retired = 2U;

struct SceneWheelResourceAllocatorState {
    std::span<SceneWheelResourceSlot> slots{};
    std::int32_t next_slot_index_0x0c{};
    std::int32_t texture_handle_0x10{};
};

// Reproduces the slot-count choice in scene-car construction (0x463920).
// A zero feature mode disables manager creation. Otherwise one or four wheel
// groups are selected by mode, with 16 slots each or 40 for the selected PCAR.
[[nodiscard]] std::uint32_t scene_wheel_resource_pool_slot_count(
    std::uint32_t feature_mode_0x45bac0,
    bool selected_pcar_ordinal_0x45c230_matches_car) noexcept;

// Projects the slot-array result and resource handle returned to the manager
// constructor at 0x4622F0. Slot positions and +0x34 remain untouched.
void initialize_scene_wheel_resource_allocator(
    SceneWheelResourceAllocatorState& allocator,
    std::span<SceneWheelResourceSlot> allocated_slots,
    std::int32_t skid_texture_handle) noexcept;

// Reproduces 0x462380: reset the cursor and clear every slot except state 1.
void reset_scene_wheel_resource_allocator(
    SceneWheelResourceAllocatorState& allocator) noexcept;

enum class SceneWheelResourceManagerDestroyAction : std::uint8_t {
    release_texture_0x47d50,
    free_slot_array_0x414420,
    destroy_base_0x63370,
};

struct SceneWheelResourceManagerDestroyPlan {
    std::array<SceneWheelResourceManagerDestroyAction, 3> actions{};
    std::int32_t texture_handle{};
    std::size_t slot_count{};
};

// Projects the ordered cleanup calls in 0x4623B0 and clears the local manager
// view; external resource release and heap free remain action descriptors.
[[nodiscard]] SceneWheelResourceManagerDestroyPlan destroy_scene_wheel_resource_allocator(
    SceneWheelResourceAllocatorState& allocator) noexcept;

enum class SceneWheelResourceFrameOperationKind : std::uint8_t {
    call_0x513a0,
    call_0x51610,
    bind_texture_0x48e30,
    draw_indexed_0x448ed0,
    end_batch_0x448ef0,
};

struct SceneWheelResourceFrameOperation {
    SceneWheelResourceFrameOperationKind kind{};
    std::uint32_t argument{};
    std::size_t slot_index{};
    std::uint32_t slot_state{};
    // Four eight-dword vertex records, retained as raw binary32 words.
    std::array<std::array<std::uint32_t, 8>, 4> vertex_words{};
    std::array<std::uint16_t, 6> index_order{};
    std::uint32_t primitive_type{};
    std::uint32_t vertex_type{};
    std::uint32_t vertex_count{};
    std::uint32_t index_count{};
    std::uint32_t flags{};
};

struct SceneWheelResourceFramePlan {
    bool reset_path{};
    std::vector<SceneWheelResourceFrameOperation> operations;
};

// Projects the ordered manager frame block beginning at 0x4623F0. The four
// stack words at vertex offset +0x0C are not written in the block and remain
// explicit inputs; renderer/device effects are only described as operations.
[[nodiscard]] SceneWheelResourceFramePlan plan_scene_wheel_resource_frame(
    SceneWheelResourceAllocatorState& allocator,
    bool frame_gate_0x45c380,
    const std::array<std::uint32_t, 4>& vertex_word_0x0c_observations);

struct SceneWheelResourceAllocation {
    bool allocated{};
    std::size_t slot_index{};
};

// Models the counted circular slot scan in 0x462590. The cursor ranges from
// zero through count; the one-past-end value wraps on the next allocation.
[[nodiscard]] SceneWheelResourceAllocation allocate_scene_wheel_resource(
    SceneWheelResourceAllocatorState& allocator,
    std::uint32_t allocation_size) noexcept;

struct SceneWheelAuxiliaryUpdateInput {
    std::array<float, 3> target_position{}; // param_1
    std::array<float, 3> orientation_vector{}; // param_2
    bool allocation_returns_nonnull{}; // fallback observation when allocator is null
    SceneWheelResourceAllocatorState* allocator{}; // when supplied, models 0x462590 directly
    bool fdiv_compatibility_enabled{}; // global 0x4E9754
};

struct SceneWheelAuxiliaryUpdateResult {
    bool allocation_attempted{};
    bool resource_released{};
    bool normalized_cross_product_used{};
};

enum class SceneWheelAuxiliaryCall : std::uint8_t {
    none,
    update_0x462600,
    release_0x462940,
    release_0x462960,
};

struct SceneWheelAuxiliaryDispatchInput {
    bool world_gate_0x45bac0{};
    std::int32_t scene_car_ordinal_0x1c8{};
    bool effect_gate_0x45c380{};
    bool suppress_gate_0x425830{};
    bool wheel_record_flag_0x34_bit0{}; // Set updates; clear retires an existing effect.
    SceneWheelAuxiliaryUpdateInput update{};
};

struct SceneWheelAuxiliaryDispatchResult {
    SceneWheelAuxiliaryCall call{};
    SceneWheelAuxiliaryUpdateResult update{};
    bool resource_released{};
};

// Models the state and four resource-position triplets updated by 0x462600.
// Pool selection is modeled when allocator is supplied; otherwise allocation
// success is an explicit observation. x87/Pentium-FDIV rounding is approximate.
[[nodiscard]] SceneWheelAuxiliaryUpdateResult update_scene_wheel_auxiliary(
    SceneWheelAuxiliaryState& state,
    const SceneWheelAuxiliaryUpdateInput& input) noexcept;

// Reproduces the ordered outer gates in 0x465800 and applies the resource
// release performed by 0x462940/0x462960 when either sibling is reached.
[[nodiscard]] SceneWheelAuxiliaryDispatchResult dispatch_scene_wheel_auxiliary(
    SceneWheelAuxiliaryState& state,
    const SceneWheelAuxiliaryDispatchInput& input) noexcept;

} // namespace viper
