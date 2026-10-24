#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

inline constexpr std::size_t scene_car_effect_tag_one_pool_size = 32;
enum class SceneCarEffectTagOneStatus : std::uint8_t {
    context_gate_closed,
    active_slot_retained,
    generated,
    random_observation_exhausted,
    slot_observation_missing,
    cursor_out_of_range,
};

struct SceneCarEffectTagOneInput {
    // FUN_0045BAE0 requires both globals to be nonzero before emitting.
    bool scene_effects_enabled_0x509b74{};
    bool effect_context_enabled_0x53a4c4{};
    std::int32_t cursor_0x540b18{};
    std::optional<float> selected_slot_time_0x18{};
    std::array<float, 3> payload_velocity{}; // handler payload +0x00
    std::array<float, 3> payload_origin{}; // handler payload +0x0C
    float now_0x45bb70{};
    // Raw FUN_004193F0(1000) observations, in x/z/y perturbation order.
    std::span<const std::int32_t> random_samples{};
};

struct SceneCarEffectTagOnePlan {
    SceneCarEffectTagOneStatus status{SceneCarEffectTagOneStatus::context_gate_closed};
    static constexpr std::int32_t returned_payload_bytes = 0x18;
    std::int32_t cursor_after{};
    std::size_t selected_slot{};
    std::size_t random_samples_consumed{};
    std::array<float, 3> stored_velocity{};
    std::array<float, 3> stored_origin{};
    float stored_time{};
    bool cursor_written{};
    bool record_written{};
    bool x87_rounding_approximated{true};
};

// Projects the tag 1 callback at 0x468450. The 32-slot pool uses a strict
// half-second occupancy window; RNG observations bound the velocity jitter.
[[nodiscard]] SceneCarEffectTagOnePlan plan_scene_car_effect_tag_one_handler(
    const SceneCarEffectTagOneInput& input) noexcept;

inline constexpr std::size_t scene_car_effect_tag_two_context_count = 16;
enum class SceneCarEffectTagTwoStatus : std::uint8_t {
    context_gate_closed,
    waiting_for_timestamp_window,
    generated,
    random_observation_exhausted,
    timestamp_observation_missing,
    cursor_out_of_range,
};

struct SceneCarEffectTagTwoInput {
    bool scene_effects_enabled_0x509b74{};
    bool effect_context_enabled_0x53a4c4{};
    std::int32_t context_cursor_0x114{};
    std::optional<float> previous_emit_time_0x118{};
    // Existing manager vector passed to 0x461AC0 before the new payload origin
    // is copied; it is not necessarily the current payload origin.
    std::optional<std::array<float, 3>> child_initializer_position_0xc4{};
    std::array<float, 3> payload_velocity{}; // handler payload +0x00
    std::array<float, 3> payload_origin{}; // handler payload +0x0C
    float payload_scale_0x1c{};
    float now_0x45bb70{};
    // Raw FUN_004193F0(1000) observations: x, z, y for each of 16 points.
    std::span<const std::int32_t> random_samples{};
};

struct SceneCarEffectTagTwoPlan {
    SceneCarEffectTagTwoStatus status{SceneCarEffectTagTwoStatus::context_gate_closed};
    static constexpr std::int32_t returned_payload_bytes = 0x24;
    std::int32_t context_cursor_after{};
    std::size_t selected_context{};
    std::size_t random_samples_consumed{};
    std::size_t complete_points_written{};
    std::array<float, 3> stored_origin{};
    std::optional<std::array<float, 3>> child_initializer_position{};
    std::array<float, 3> child_initializer_velocity{};
    std::uint8_t child_initializer_mode{};
    float stored_emit_time{};
    float clamped_payload_scale{};
    float vertical_scale{};
    std::array<std::array<float, 3>, 16> trail_points{};
    std::array<std::uint8_t, 16> point_component_write_masks{}; // x=1, y=2, z=4
    std::optional<std::size_t> pending_vertical_point{};
    float pending_vertical_value{};
    bool initializer_0x61ac0_called{};
    bool context_cursor_written{};
    bool origin_and_time_written{};
    bool x87_rounding_approximated{true};
};

// Projects tag 2 callback 0x468290's context ring, half-second emission gate,
// and 16 randomized trail points. The 0x61AC0 initializer remains an external
// effect; its selected context and invocation are reported explicitly.
[[nodiscard]] SceneCarEffectTagTwoPlan plan_scene_car_effect_tag_two_handler(
    const SceneCarEffectTagTwoInput& input) noexcept;

enum class SceneCarEffectChildInitStatus : std::uint8_t {
    initialized,
    random_observation_exhausted,
};

struct SceneCarEffectChildInitInput {
    std::array<float, 3> position{}; // 0x461AC0 param_1
    std::array<float, 3> velocity{}; // 0x461AC0 param_2
    std::uint8_t mode{}; // 0x461AC0 param_3
    std::uint32_t timer_counter_0x508774{};
    std::span<const std::int32_t> random_samples{}; // raw FUN_004193F0(0xFFFF)
    std::int32_t registration_id_0x34{};
    std::uint32_t registration_data_0x38{};
};

struct SceneCarEffectChildInitPlan {
    SceneCarEffectChildInitStatus status{
        SceneCarEffectChildInitStatus::random_observation_exhausted};
    std::size_t random_samples_consumed{};
    std::array<float, 3> stored_position{};
    std::array<float, 3> stored_velocity{};
    std::uint8_t stored_mode{};
    float lifetime_0x50{};
    float expires_at_0x4c{};
    float timer_0x54{};
    std::int32_t registration_id_0x34{};
    std::uint32_t registration_data_0x38{};
    float registration_scale{1.0F};
    bool active_0x3c_written{};
    bool position_written{};
    bool lifetime_fields_written{};
    bool velocity_fields_written{};
    bool registration_0x461650_called{};
    bool x87_rounding_approximated{true};
};

// Projects 0x461AC0's per-context particle fields. The 0x461650 quad is modeled
// separately; the common renderer's queue/device mutations remain external.
[[nodiscard]] SceneCarEffectChildInitPlan plan_scene_car_effect_child_initializer(
    const SceneCarEffectChildInitInput& input) noexcept;

enum class SceneCarBillboardQuadStatus : std::uint8_t {
    generated,
    frame_conversion_observation_missing,
    frame_out_of_range,
};

struct SceneCarBillboardVertex {
    float x{};
    float y{};
    float u{};
    float v{};
};

struct SceneCarBillboardQuadInput {
    std::uint32_t opaque_argument_0x38{};
    std::int32_t opaque_argument_0x34{};
    float frame_and_size{1.0F};
    // The source uses __ftol on its x87 value. Supply that conversion result
    // explicitly when the caller's FPU rounding state is not known.
    std::optional<std::int32_t> converted_frame_index{1};
    std::uint8_t mode{};
};

struct SceneCarBillboardQuadPlan {
    SceneCarBillboardQuadStatus status{
        SceneCarBillboardQuadStatus::frame_conversion_observation_missing};
    std::array<SceneCarBillboardVertex, 4> vertices{};
    std::array<std::uint32_t, 4> vertex_record_offsets{};
    std::uint32_t opaque_argument_0x38{};
    std::int32_t opaque_argument_0x34{};
    std::int32_t converted_frame_index{};
    float half_extent{};
    std::uint8_t submission_flags{7};
    bool vertex_data_written{};
    bool shared_quad_submission_called{};
    bool x87_rounding_approximated{true};
};

// Projects 0x461650's quad vertex/UV writes. The common submission through
// 0x44F750/0x44F9C0 is reported but its renderer-side mutations remain external.
[[nodiscard]] SceneCarBillboardQuadPlan plan_scene_car_billboard_quad(
    const SceneCarBillboardQuadInput& input) noexcept;

struct SceneRendererDescriptor {
    std::string name_0x00;
    std::int16_t center_0x18{};
    std::int16_t field_0x1a{};
    std::int16_t first_vertex_0x1c{};
    std::int16_t end_vertex_0x1e{};
};

struct SceneRendererResourceEntry {
    std::string lookup_name_0x00;
    std::string canonical_name_0x10;
    std::int32_t resource_id_0x20{};
    bool handle_cell_present_0x24{};
    std::int32_t handle_cell_value{};
};

struct SceneRendererBindingNode {
    std::string name;
    std::int32_t resource_id{};
    std::int32_t handle{-1};
    std::uint32_t vertex_pointer_0x18{};
    std::int32_t vertex_component_count_0x1c{};
    std::int32_t texture_offset_0x20{};
    std::int32_t texture_component_count_0x24{};
};

enum class SceneRendererRefreshActionKind : std::uint8_t {
    rebase_vertex,
    allocate_binding_node,
    load_resource,
    release_resource,
    update_handle_cell,
    free_binding_node,
};

struct SceneRendererRefreshAction {
    SceneRendererRefreshActionKind kind{};
    std::size_t descriptor_index{};
    std::size_t item_index{};
    std::size_t resource_table_index{};
    std::int32_t value{};
    std::string name;
};

enum class SceneRendererRefreshStatus : std::uint8_t {
    complete,
    descriptor_observation_exhausted,
    vertex_source_observation_exhausted,
    vertex_destination_observation_exhausted,
    allocation_observation_exhausted,
    binding_node_allocation_failed,
    resource_load_observation_exhausted,
};

struct SceneRendererRefreshInput {
    std::uint8_t flags{}; // 0x44F9C0 param_4
    std::uint8_t stored_byte_0x15{}; // param_3
    std::uint32_t descriptor_block_identity{}; // pointer identity written at +0x10
    std::int32_t descriptor_count{}; // descriptor block +0x08
    std::span<const SceneRendererDescriptor> descriptors{}; // +0x0C, stride 0x20
    std::span<const std::array<std::int16_t, 4>> source_vertices{}; // +0x14, stride 8
    std::uint32_t destination_vertex_buffer_address{}; // renderer state +0x24
    std::size_t destination_vertex_capacity{}; // available 3-short output rows
    std::int32_t texture_data_base_0x4d9fec{};
    std::span<const SceneRendererResourceEntry> resources{}; // renderer state +0x18/+0x1C
    std::span<const SceneRendererBindingNode> existing_binding_nodes{}; // list at state +0x28
    std::span<const std::uint8_t> node_allocation_results{}; // 0=allocation failed
    std::span<const std::int32_t> resource_load_results{}; // FUN_00447D10 returns
};

struct SceneRendererRebasedVertex {
    std::size_t vertex_index{};
    std::array<std::int16_t, 3> position{};
};

struct SceneRendererRefreshPlan {
    SceneRendererRefreshStatus status{SceneRendererRefreshStatus::complete};
    std::uint32_t stored_descriptor_block_identity{};
    std::uint8_t stored_byte_0x15{};
    std::size_t descriptor_rows_processed{};
    std::size_t source_vertices_rebased{};
    std::size_t resource_loads_consumed{};
    std::size_t node_allocations_consumed{};
    std::vector<SceneRendererRebasedVertex> rebased_vertices;
    std::vector<SceneRendererBindingNode> binding_nodes_after;
    std::vector<std::int32_t> resource_handle_cells_after;
    std::vector<SceneRendererRefreshAction> actions;
};

// Projects 0x44F9C0 flags 1/2/4: centered short-vertex rebasing, case-
// insensitive resource-list reuse/update/cleanup, and unconditional header
// stores. Allocation, resource-load returns and memory spans are observations.
[[nodiscard]] SceneRendererRefreshPlan plan_scene_renderer_refresh(
    const SceneRendererRefreshInput& input);

enum class SceneCarEffectGeneratorStatus : std::uint8_t {
    inactive_horizontal_motion,
    generated,
    random_observation_exhausted,
};

struct SceneCarEffectParticleGeneratorInput {
    std::array<float, 3> velocity{}; // handler payload +0x00
    std::array<float, 3> origin{}; // handler payload +0x0C
    std::uint32_t effect_key{};
    // FUN_00468E80 draws three values per generated cloud particle, x/y/z.
    std::span<const std::int32_t> random_percent_samples{};
    float now_0x45bb70{};
    bool fdiv_compatibility_enabled{};
};

struct SceneCarEffectParticleGeneratorPlan {
    SceneCarEffectGeneratorStatus status{SceneCarEffectGeneratorStatus::inactive_horizontal_motion};
    std::uint32_t stored_effect_key{};
    std::array<float, 3> stored_origin{};
    std::array<float, 3> scaled_velocity{};
    std::array<std::array<float, 3>, 15> control_points{};
    std::array<std::uint32_t, 15> control_point_flags{};
    std::array<std::array<float, 3>, 15> cloud_particles{};
    std::size_t random_samples_consumed{};
    std::size_t cloud_particles_written_count{};
    float duration_0x370{};
    float time_0x36c{};
    std::uint32_t mode_word_0x378{};
    bool control_points_written{};
    bool lifetime_fields_written{};
    bool x87_rounding_approximated{true};
};

// Projects 0x468AF0's 15 control points, 15 randomized cloud positions,
// horizontal-motion gate, and lifetime fields. RNG draws and current time
// remain explicit observations; x87 intermediate rounding is approximate.
[[nodiscard]] SceneCarEffectParticleGeneratorPlan
plan_scene_car_effect_particle_generator(
    const SceneCarEffectParticleGeneratorInput& input) noexcept;

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
    bind_texture_0x448e30,
    draw_indexed_0x448ed0,
    end_batch_0x448ef0,
};

enum class SceneWheelTextureBindingActionKind : std::uint8_t {
    ensure_resource,
    check_surface,
    set_render_state,
    synchronize_render_state,
    report_device_error,
};

struct SceneSharedRenderStateSyncObservation {
    // Raw 24-byte records at DAT_0050D74C (desired) and DAT_0050D760 (cache).
    // Integer fields at +0x04 and +0x10 are little-endian x86 values.
    std::array<std::uint8_t, 24> desired_state{};
    std::array<std::uint8_t, 24> cached_state{};
    bool synchronization_gate_0x44e400{};
    bool alternate_blend_factor_0x509b7d{};
    bool alpha_reference_gate_0x509b7c{};
    bool color_write_gate_0x509b7b{};
};

struct SceneSharedRenderStateSyncCall {
    std::uint32_t wrapper_address{};
    std::array<std::int32_t, 3> arguments{};
    std::uint8_t argument_count{};
    bool wrapper_side_effect_gate_passed{true};
};

struct SceneSharedRenderStateSyncPlan {
    bool synchronization_gate_open{};
    bool cache_snapshot_updated{};
    bool dirty_flag_cleared{};
    std::optional<std::int32_t> unsupported_mode_value;
    std::array<std::uint8_t, 24> cached_state_after{};
    std::vector<SceneSharedRenderStateSyncCall> calls;
};

[[nodiscard]] SceneSharedRenderStateSyncPlan plan_scene_shared_render_state_sync(
    const SceneSharedRenderStateSyncObservation& observation);

struct SceneWheelTextureBindingAction {
    SceneWheelTextureBindingActionKind kind{};
    std::uint32_t address{};
    std::int32_t argument_0{};
    std::int32_t argument_1{};
    std::int32_t result_value{};
};

enum class SceneWheelTextureBindingStatus : std::uint8_t {
    complete,
    resource_access_observation_missing,
    surface_observation_missing,
};

struct SceneWheelTextureBindingObservation {
    std::int32_t requested_handle{}; // 0x4525F0 param_1
    std::int32_t cached_handle{}; // DAT_004DA17C
    bool cache_dirty{}; // DAT_004DA180
    bool force_texture_enabled{}; // DAT_004DA178
    bool texture_enabled_before{}; // DAT_00509B3E
    std::optional<bool> resource_access_ready; // 0x454030
    std::optional<bool> surface_available; // 0x4540E0
    std::int32_t set_render_state_hresult{}; // 0x458BC0, used for handle -1
};

struct SceneWheelTextureBindingPlan {
    SceneWheelTextureBindingStatus status{SceneWheelTextureBindingStatus::complete};
    std::int32_t cached_handle_after{};
    bool cache_dirty_after{};
    std::optional<bool> texture_enabled_after;
    bool render_state_synchronization_called{};
    std::optional<SceneSharedRenderStateSyncPlan> synchronized_states;
    std::vector<SceneWheelTextureBindingAction> actions;
};

[[nodiscard]] SceneWheelTextureBindingPlan plan_scene_wheel_texture_binding(
    const SceneWheelTextureBindingObservation& observation,
    const std::optional<SceneSharedRenderStateSyncObservation>& render_state_sync = std::nullopt);

struct SceneRenderStateSnapshotRefreshPlan {
    std::uint32_t state_pointer_0x50d74c{};
    bool cache_invalidation_applied{};
    std::array<std::uint8_t, 24> cached_state_after{};
};

[[nodiscard]] SceneRenderStateSnapshotRefreshPlan plan_scene_render_state_snapshot_refresh(
    const std::array<std::uint8_t, 24>& state_record,
    const std::array<std::uint8_t, 24>& cached_state,
    bool dirty_flag_0x50d778);

struct SceneRenderCallbackSelectionObservation {
    bool state_0x50d119{};
    bool alternate_0x50d2dc{};
    bool alternate_0x50d118{};
};

struct SceneRenderCallbackSelectionPlan {
    std::uint32_t callback_slot_0x4db744{};
    std::uint32_t callback_slot_0x4db748{};
};

[[nodiscard]] SceneRenderCallbackSelectionPlan plan_scene_render_callback_selection(
    bool state_0x50d0a8,
    const SceneRenderCallbackSelectionObservation& observation);

using SceneRenderRowRecord = std::array<std::uint32_t, 8>;

enum class SceneRenderScanlineConversionStatus : std::uint8_t {
    complete,
    unsupported_callback,
    unsupported_descriptor_mode,
    negative_row_pointer,
    source_observation_exhausted,
    destination_observation_exhausted,
    depth_fade_result_observation_exhausted,
};

struct SceneRenderScanlineConversionObservation {
    std::int16_t first_row{}; // descriptor +0x18, signed
    std::int16_t end_row{}; // descriptor +0x1A, exclusive and signed
    std::uint8_t descriptor_mode{}; // descriptor +0x10, used by 0x455E00
    std::uint8_t alpha_byte_0x50d110{}; // used by 0x455D90
    std::uint32_t color_or_mask_0x50d114{}; // used by 0x455E00
    // Output byte of the x87 distance-fade branch, indexed by absolute row.
    std::span<const std::uint8_t> depth_fade_alpha_bytes;
    std::span<const SceneRenderRowRecord> source_rows;
    std::span<const SceneRenderRowRecord> destination_before;
};

struct SceneRenderScanlineWrite {
    std::size_t row_index{};
    std::uint8_t byte_offset{};
    std::uint32_t value{};
};

struct SceneRenderScanlineConversionPlan {
    SceneRenderScanlineConversionStatus status{SceneRenderScanlineConversionStatus::complete};
    std::uint32_t callback_address{};
    std::size_t rows_processed{};
    std::size_t row_counter_increments{}; // DAT_0050D304
    std::vector<SceneRenderRowRecord> destination_after;
    std::vector<SceneRenderScanlineWrite> writes;
};

// Models observed row converters selected by 0x455C80. Writes remain
// bounded by supplied source/destination rows; destination word +0x0C is
// deliberately preserved because the native loop leaves it untouched.
[[nodiscard]] SceneRenderScanlineConversionPlan plan_scene_render_scanline_conversion(
    std::uint32_t callback_address,
    const SceneRenderScanlineConversionObservation& observation);

struct SceneRenderPanicForwarderPlan {
    std::uint32_t callback_address{};
    std::uint32_t panic_reporter_address{}; // 0x411000
    std::uint32_t format_pointer{};
    std::string_view format_text;
};

// Resolves callback-table stubs that forward a static message pointer to the
// shared panic reporter. This does not model when the indirect callback runs.
[[nodiscard]] std::optional<SceneRenderPanicForwarderPlan>
plan_scene_render_panic_forwarder(std::uint32_t callback_address) noexcept;

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
    // The callback forwards through 0x448ED0 to 0x452700, whose device
    // dispatch uses vtable offset +0x78 and ignores the native return value.
    std::uint32_t forwarding_wrapper{};
    std::uint32_t device_wrapper{};
    std::uint32_t device_vtable_offset{};
    bool device_return_ignored{};
    std::uint32_t texture_transform_state{};
    std::array<std::uint32_t, 16> texture_transform_matrix_words{};
    std::optional<std::uint32_t> state_mask_before;
    std::optional<std::uint32_t> state_mask_after;
    std::optional<std::uint32_t> state_mask_index;
    std::optional<bool> texture_stage_enabled_before;
    std::optional<bool> texture_stage_enabled_after;
    // Set when 0x457A20(&DAT_00509B30) follows a 0x100-byte toggle.
    bool state_snapshot_refresh_called{};
    // Value passed to 0x455A60 when the toggle changes; otherwise absent.
    std::optional<bool> state_toggle_notification_value;
    std::optional<SceneRenderStateSnapshotRefreshPlan> state_snapshot_refresh;
    std::optional<SceneRenderCallbackSelectionPlan> callback_selection;
};

struct SceneWheelResourceStateMaskObservation {
    std::uint32_t state_index_0x4da120{};
    std::uint32_t mask_at_0x509f50_plus_stride{};
    std::array<std::uint8_t, 24> state_record_0x509b30{};
    std::array<std::uint8_t, 24> cached_render_state_0x50d760{};
    bool dirty_flag_0x50d778{};
    std::optional<SceneRenderCallbackSelectionObservation> callback_selection_context;
};

struct SceneWheelResourceFramePlan {
    bool reset_path{};
    std::optional<SceneWheelTextureBindingPlan> texture_binding;
    std::vector<SceneWheelResourceFrameOperation> operations;
};

// Projects the ordered manager frame block beginning at 0x4623F0. The four
// stack words at vertex offset +0x0C are not written in the block and remain
// explicit inputs; renderer/device effects are only described as operations.
[[nodiscard]] SceneWheelResourceFramePlan plan_scene_wheel_resource_frame(
    SceneWheelResourceAllocatorState& allocator,
    bool frame_gate_0x45c380,
    const std::array<std::uint32_t, 4>& vertex_word_0x0c_observations,
    const std::optional<SceneWheelTextureBindingObservation>& texture_binding = std::nullopt,
    const std::optional<SceneSharedRenderStateSyncObservation>& render_state_sync = std::nullopt,
    const std::optional<SceneWheelResourceStateMaskObservation>& state_mask = std::nullopt);

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
