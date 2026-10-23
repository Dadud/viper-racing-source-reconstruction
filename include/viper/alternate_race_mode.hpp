#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

struct AlternateRaceLoopState {
    std::uint32_t state_three_observations{};
    bool exited{};
    bool test_flag_set{};
};

struct AlternateRaceLoopIteration {
    bool wait_500ms{};
    bool render_frame{};
    bool pump_messages_and_poll_input{};
    bool advance_control_state{};
    bool exited_now{};
    bool set_test_flag_now{};
};

enum class AlternateTestPromptPhase {
    not_started,
    waiting_for_render_context,
    waiting_for_input,
    complete,
};

struct AlternateTestPromptState {
    AlternateTestPromptPhase phase{AlternateTestPromptPhase::not_started};
};

struct AlternateTestPromptIteration {
    bool call_413ce0{};
    bool try_render_context{};
    bool pump_messages{};
    bool draw_prompt{};
    bool call_447ce0{};
    bool call_447c50{};
    bool consume_input_event{};
    bool call_41f4c0{};
};

struct AlternateTestPromptTextOperation {
    std::uint32_t draw_helper{0x0044D710U};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t text_address{};
    std::uint32_t color_source{0x004F0B14U};
};

struct AlternateTestPromptTextPlan {
    bool reached{};
    std::array<AlternateTestPromptTextOperation, 3> operations{};
};

[[nodiscard]] AlternateTestPromptTextPlan plan_alternate_test_prompt_text(
    bool render_context_available) noexcept;

struct AlternateThreeFrameState {
    std::uint8_t successful_frames{};
};

struct AlternateThreeFrameIteration {
    bool acquisition_attempted{};
    bool acquired{};
    bool completed{};
    std::uint32_t draw_text_address{0x00507C5CU};
    std::vector<std::uint32_t> ordered_helpers;
};

// Advances one observed iteration of 0x41F4C0's tight three-success loop.
[[nodiscard]] AlternateThreeFrameIteration advance_alternate_three_frame_helper(
    AlternateThreeFrameState& state,
    bool render_context_available);

struct AlternateModeThreeCandidate {
    bool callback_is_eligible{};
    bool callback_succeeded{};
};

struct AlternateModeObjectFields {
    std::array<std::uint32_t, 3> required_values_0x44_0x48_0x4c{};
    std::array<std::uint32_t, 4> reset_values_0x54_0x5c_0x60_0x64{};
};

// Models the polling/exit portion of 0x41AB00. Pass an event only when the
// event-available predicate succeeded; unrelated key values are ignored.
[[nodiscard]] AlternateRaceLoopIteration advance_alternate_race_loop(
    AlternateRaceLoopState& state,
    int mode_value,
    bool render_context_available,
    bool control_callback_requests_update,
    std::optional<std::uint16_t> key_event) noexcept;

// Models the optional post-loop branch in 0x41AB00. Inputs represent the
// recovered setting byte, render-context acquisition, and input availability.
[[nodiscard]] AlternateTestPromptIteration advance_alternate_test_prompt(
    AlternateTestPromptState& state,
    bool test_prompt_enabled,
    bool render_context_available,
    bool input_event_available) noexcept;

struct AlternateTestPromptWorkflowState {
    AlternateTestPromptState prompt{};
    AlternateThreeFrameState three_frame{};
    bool three_frame_helper_started{};
    bool complete{};
};

struct AlternateTestPromptWorkflowObservation {
    bool test_prompt_enabled{};
    bool prompt_render_context_available{};
    bool input_event_available{};
    bool three_frame_render_context_available{};
};

struct AlternateTestPromptWorkflowPlan {
    AlternateTestPromptIteration prompt{};
    AlternateTestPromptTextPlan prompt_text{};
    std::optional<AlternateThreeFrameIteration> three_frame;
    bool complete{};
};

// Composes the optional 0x41AB00 prompt, its text calls, and 0x41F4C0's
// repeated context/draw loop from one observed workflow step.
[[nodiscard]] AlternateTestPromptWorkflowPlan advance_alternate_test_prompt_workflow(
    AlternateTestPromptWorkflowState& state,
    const AlternateTestPromptWorkflowObservation& observation);

// Models the state-3 store at 0x43CE62. A candidate is eligible when its
// per-object byte at +0x6A is zero and the matching shared byte is nonzero.
[[nodiscard]] bool should_switch_to_alternate_mode_three(
    int current_mode,
    bool outer_gate_426e50,
    bool mode_gate_49be40,
    bool result_index_available,
    bool timer_latch_0x7d8,
    bool stored_time_0x7d4_is_before_now,
    std::span<const AlternateModeThreeCandidate> candidates) noexcept;

// Virtual methods shared by numeric object types 1 and 3 in vtables
// 0x4C7810 and 0x4C7880.
[[nodiscard]] bool alternate_mode_object_is_ready(
    const AlternateModeObjectFields& fields) noexcept;
void reset_alternate_mode_object(AlternateModeObjectFields& fields,
                                 std::int32_t& global_mode) noexcept;
void store_alternate_mode_three(std::int32_t& global_mode) noexcept;

struct AlternateModeInterfaceState {
    std::array<float, 3> position_0x38{};
    std::uint32_t transition_count_0x54{};
    float state_time_0x58{};
    float elapsed_since_state_0x5c{};
    float vehicle_speed_0x60{};
    std::uint32_t state_0x64{};
};

struct AlternateModeRegionObservation {
    float value_0x6c{};
    float value_0x74{};
    float value_0x78{};
    float value_0x80{};
};

struct AlternateModeDisplayTargetObservation {
    std::uint32_t address_token{};
    bool active_flag_0x0c{};
};

struct AlternateModeDisplayQueueState {
    std::array<std::uint32_t, 32> address_tokens_0xd8{};
    std::uint32_t count_0x158{};
};

// Reproduces the low-byte predicate in 0x43A880 for two X/Z points and the
// four raw region values consumed at +0x6C/+0x74/+0x78/+0x80.
[[nodiscard]] bool alternate_mode_region_query(
    const AlternateModeRegionObservation& region,
    const std::array<float, 3>& first_point,
    const std::array<float, 3>& second_point) noexcept;

struct AlternateModeInterfaceUpdateInput {
    std::array<float, 9> vehicle_orientation_0x38{};
    std::array<float, 3> vehicle_position_0x5c{};
    std::array<float, 3> position_offsets_0xd7c{};
    std::array<float, 3> reference_point_0x2c{};
    float other_object_value_0x48_0x64{};
    float vehicle_adjustment_0x64{};
    float current_time{};
    std::array<float, 3> vehicle_velocity_0x234{};
    std::uint32_t vehicle_ordinal_0x510{};
    // Return values from the state-specific 0x43A880 and vtable callbacks.
    bool state_zero_query_succeeded{};
    bool state_one_query_succeeded{};
    bool state_two_query_succeeded{};
    std::optional<AlternateModeRegionObservation> state_zero_region;
    std::optional<AlternateModeRegionObservation> state_one_region;
    std::optional<AlternateModeRegionObservation> state_two_region;
    // Result of the 0x46C0D0 string/resource lookup for a reached message.
    std::optional<AlternateModeDisplayTargetObservation> display_target;
};

struct AlternateModeInterfaceUpdateResult {
    std::array<float, 3> position_0x38{};
    bool vehicle_adjustment_changed{};
    float vehicle_adjustment_0x64{};
    bool message_0x4d5b8c{};
    bool message_0x4d5b98{};
    bool message_0x4d5ba4{};
    bool virtual_slot_0x64_called{};
    bool virtual_slot_0x68_called{};
    bool result_helpers_called{};
    bool invalid_state_panic_0x411000{};
    bool display_target_flag_stores{};
    bool display_queue_lock_entered{};
    bool display_queue_lock_left{};
    bool display_queue_entry_added{};
    bool display_queue_full_panic_0x411000{};
    std::uint32_t display_target_address_token{};
    std::uint32_t result_helper_vehicle_ordinal{};
};

// Models the shared vtable +0x38 callback installed by factory 0x426000 for
// numeric object types 1 and 3. Its external query, display, and virtual
// callback results stay explicit inputs.
[[nodiscard]] AlternateModeInterfaceUpdateResult
update_alternate_mode_interface(
    AlternateModeInterfaceState& state,
    const AlternateModeInterfaceUpdateInput& input,
    AlternateModeDisplayQueueState* display_queue = nullptr) noexcept;

// Reproduces the lap-index window selected by 0x41AB00. Each availability
// entry stands for a matching 0x48-byte record with selector +0x41 == 0 and
// lap-index byte +0x43; a missing record stops the visible list.
[[nodiscard]] std::vector<std::int32_t> alternate_visible_lap_indices(
    std::int32_t lap_count,
    std::span<const std::uint8_t> lap_record_available);

enum class AlternateRaceTextKind : std::uint8_t {
    entrant_name_field_0x05,
    notes_caption,
    entrant_name_field_0x12,
    results_caption,
    track_direction_line,
    elapsed_time_line,
    lap_time_line,
};

struct AlternateRaceTextOperation {
    AlternateRaceTextKind kind{};
    std::uint32_t draw_helper{};
    std::int32_t x{};
    std::int32_t y{};
    std::uint32_t source_address{};
    std::uint32_t color_source{};
    std::int32_t lap_index{};
    // Formatting/lookups executed before this draw, retained in native order.
    std::array<std::uint32_t, 2> prerequisite_helpers{};
    std::uint8_t prerequisite_helper_count{};
    std::uint32_t format_address{};
};

struct AlternateRaceTextObservation {
    bool render_context_available{};
    std::int32_t surface_width_0x509a4c{};
    std::int32_t font_line_height_0x4d770{};
    std::int32_t lap_count_from_vtable_0x48{};
    bool track_is_reversed_0xcd1{};
    std::span<const std::uint8_t> lap_record_available{};
};

struct AlternateRaceTextPlan {
    bool reached{};
    std::int32_t line_step{};
    std::vector<AlternateRaceTextOperation> operations;
};

// Projects the ordered 0x41AB00 scoreboard text calls for one acquired frame.
// Table contents, time values, and the vtable lap count remain observations.
[[nodiscard]] AlternateRaceTextPlan plan_alternate_race_text(
    const AlternateRaceTextObservation& observation);

struct AlternateRaceFrameObservation {
    int mode_value{};
    bool render_context_available{};
    bool control_callback_requests_update{};
    std::optional<std::uint16_t> key_event;
    AlternateRaceTextObservation text{};
};

struct AlternateRaceFramePlan {
    AlternateRaceLoopIteration loop{};
    AlternateRaceTextPlan text{};
};

// Connects one observed 0x41AB00 loop pass to its ordered scoreboard draws.
[[nodiscard]] AlternateRaceFramePlan advance_alternate_race_frame(
    AlternateRaceLoopState& state,
    const AlternateRaceFrameObservation& observation);

} // namespace viper
