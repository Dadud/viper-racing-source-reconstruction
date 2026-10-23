#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "viper/graphics_startup.hpp"

namespace viper {

struct RaceLoopCallbackSlots {
    // First three DWORDs of the five-DWORD row at 0x4CF5C0 + state * 0x14.
    std::uint32_t slot_0x00{};
    std::uint32_t slot_0x04{};
    std::uint32_t slot_0x08{};
};

// Returns rows observed in the shipped image for DAT_004CF5B8 values 0..5.
// The addresses remain opaque; call sites in 0x4020E0 establish which slot
// receives the render context and where the other two slots are invoked.
[[nodiscard]] std::optional<RaceLoopCallbackSlots>
race_loop_callbacks_for_state(std::int32_t state) noexcept;

struct RaceOverlayTransition {
    std::int32_t next_state{};
    bool overlay_dirty_flag_set{};
    bool race_loop_reentered{};
};

// Models the 0x4020E0 key-0x31 branch and its 0x402960 setter call: state 0
// switches to state 4; every nonzero state switches to state 0. The setter's
// observed 0x4035D0/0x402060 callees do not write the 0x4EB20C dirty-page flag.
[[nodiscard]] RaceOverlayTransition toggle_race_overlay_from_key_0x31(
    std::int32_t current_state) noexcept;

struct RaceLoopPageTransition {
    std::int32_t next_page{};
    bool page_store_performed{};
};

// Models the numeric page writes to 0x4CF5B4 in the race-loop event switch.
// Page meanings remain address-based; event 0x17B reaches a guarded no-op.
[[nodiscard]] RaceLoopPageTransition transition_race_loop_page_for_event(
    std::int32_t current_page,
    std::uint16_t event) noexcept;

struct RaceLoopPlannedCall {
    std::uint32_t address{};
    bool state_indexed_callback{};
    bool receives_render_context{};
    std::array<std::uint32_t, 4> trailing_arguments{};
    std::uint8_t trailing_argument_count{};
};

struct RaceLoopActiveFrameObservation {
    std::int32_t callback_state{};
    // Result of the 0x425850 active-update predicate.
    bool active_update_branch{};
    // State 4 first calls 0x404810; only a false result reaches the
    // secondary 0x40C2F0 render check.
    bool mode4_render_helper_0x404810{};
    bool mode4_secondary_render_check_0x40c2f0{};
    bool render_context_acquired{};
    // A second 0x40C2F0 check follows 0x40AA30 on an acquired context.
    bool post_submit_context_check_0x40c2f0{};
};

struct RaceLoopActiveFramePlan {
    std::vector<RaceLoopPlannedCall> calls;
    bool active_update_branch{};
    bool render_gate_passed{};
    bool render_context_requested{};
    bool frame_submitted{};
    bool post_submit_context_callback_reached{};
    std::optional<SurfaceUnlockPlan> surface_unlock{};
};

struct EscapeMenuInitializationState {
    std::uint32_t initialized_mask_0x4eb21c{};
    std::int32_t saved_page_0x4eb184{};
};

struct EscapeMenuInitializationObservation {
    std::int32_t current_page_0x4cf5b4{};
    std::uint32_t resume_text_handle_0x4eb204{};
    std::uint32_t restart_text_handle_0x4eb18c{};
    std::uint32_t end_race_text_handle_0x4eb15c{};
    std::uint32_t disconnect_text_handle_0x4eb17c{};
    bool resume_text_needs_refresh_0x4eb200{};
    bool restart_text_needs_refresh_0x4eb188{};
    bool end_race_text_needs_refresh_0x4eb158{};
    bool disconnect_text_needs_refresh_0x4eb178{};
};

struct EscapeMenuButtonDescriptorInitialization {
    std::uint32_t address{};
    // Thirteen DWORDs from descriptor offsets 0x00 through 0x30.
    std::array<std::uint32_t, 13> words{};
    std::uint16_t written_word_mask{};
};

struct EscapeMenuInitializationPlan {
    std::vector<RaceLoopPlannedCall> calls;
    std::vector<EscapeMenuButtonDescriptorInitialization> button_descriptors;
    std::uint8_t localized_text_entries_initialized{};
    std::uint8_t button_groups_initialized{};
    bool saved_current_page{};
};

// Models 0x4022F0's once-only label setup, localization refresh calls, saved
// pre-menu page, and four button-table initialization guards and descriptors.
// Localized text handles and refresh outcomes remain caller observations.
[[nodiscard]] EscapeMenuInitializationPlan plan_escape_menu_initialization(
    EscapeMenuInitializationState& state,
    const EscapeMenuInitializationObservation& observation) noexcept;

struct EscapeMenuEventState {
    std::int32_t page_0x4cf5b4{};
    std::int32_t callback_state_0x4cf5b8{};
    std::uint8_t mode_toggle_0x4eb238{};
    std::uint8_t overlay_dirty_flag_0x4eb20c{};
};

struct EscapeMenuEventObservation {
    bool input_available_0x413d50{};
    std::uint16_t event_0x413d20{};
    bool render_cancel_gate_0x40c2f0{};
    bool state_callback_present{};
    std::uint32_t state_callback_address{};
    bool state_callback_handled{};
    bool multiplayer_mode_0x49be40{};
    bool multiplayer_disconnected_0x49be50{};
    std::int32_t mode_event_poll_result_0x40a160{};
    std::int32_t selected_player_ordinal_0x45c230{-1};
    std::int32_t selected_player_ordinal_after_requery_0x45c230{-1};
    bool selected_player_vtable_handled{};
    std::int32_t active_player_count_0x45c2c0{};
};

struct EscapeMenuEventPlan {
    std::vector<RaceLoopPlannedCall> calls;
    std::optional<RaceOverlayTransition> overlay_transition;
    std::optional<RaceLoopPageTransition> page_transition;
    bool event_available{};
    bool event_consumed{};
    bool overlay_dirty_flag_set{};
    bool startup_setup_byte_set{};
    bool exit_latch_set{};
    bool unresolved_selected_player_virtual_call{};
    std::optional<std::int32_t> selected_player_vtable_argument;
};

// Reconstructs the input and event routing in 0x4022F0. State callback return
// values, platform-mode predicates, and player-list queries are observations;
// 0x4018F0's consumed low-word key values are modeled directly.
[[nodiscard]] EscapeMenuEventPlan plan_escape_menu_event(
    EscapeMenuEventState& state,
    const EscapeMenuEventObservation& observation);

struct RaceLoopSimulationStepState {
    float previous_time_0x4d4df0{};
};

struct RaceLoopSimulationStepObservation {
    std::uint32_t synchronization_object_0x4d4ddc{};
    std::uint32_t timer_object_0x4d4de4{};
    float current_time_0x425790{};
};

struct RaceLoopSimulationStepPlan {
    float raw_elapsed{};
    float update_delta{};
    bool delta_was_clamped{};
    bool time_anchor_updated_before_timer_dispatch{};
    std::array<RaceLoopPlannedCall, 5> calls{};
    std::uint8_t call_count{};
};

// Models the ordered substep in 0x425920 nested inside the active race frame.
[[nodiscard]] RaceLoopSimulationStepPlan plan_race_loop_simulation_step(
    RaceLoopSimulationStepState& state,
    const RaceLoopSimulationStepObservation& observation) noexcept;

struct RaceLoopPeriodicUpdateState {
    float accumulator_0x509424{};
};

struct RaceLoopPeriodicUpdateObservation {
    float simulation_delta{};
    // Raw DWORD arguments read from globals 0x509384, 0x509388, and 0x5093A4.
    std::array<std::uint32_t, 3> periodic_callback_arguments_0x509384{};
};

// Binding records consumed by 0x446980 store a type DWORD followed by a
// selector DWORD. The keyboard/device observations here are snapshots of
// state already sampled elsewhere in the input loop.
struct RaceInputBinding {
    std::uint32_t kind{};
    std::uint32_t selector{};
};

struct RaceInputBindingObservation {
    // Four axes used by 0x446980, ordered by globals +0x509888, +0x50988C,
    // +0x509894, then +0x509890, matching type-2 selector groups.
    std::array<float, 4> controller_axes_0x509888{};
    // Two additional axes written by 0x416D30 at +0x509898/+0x50989C.
    std::array<float, 2> controller_axes_extra_0x509898{};
    // Button-state bytes written by 0x416D30; the binding reads selectors 0-7.
    std::array<std::uint8_t, 32> button_bytes_0x5098a0{};
    std::uint32_t pov_direction_0x5098c0{};
    std::array<float, 2> mouse_axes_0x509660{};
    std::array<std::uint8_t, 3> mouse_button_bytes_0x509668{};
    // Sampled result for each selector passed to 0x413170. Type-1 binding
    // selectors are read from one byte of the binding record.
    std::array<std::uint8_t, 256> device_button_states{};
};

struct RaceControllerInputRefreshObservation {
    // DirectInput GetDeviceState return code and consumed fields from its
    // 0x50-byte record, supplied as raw observations rather than polled here.
    std::uint32_t state_read_result{};
    // Source record fields at +0x00,+0x04,+0x08,+0x14,+0x0C,+0x10, in the
    // exact order written to controller globals +0x00 through +0x14.
    std::array<std::int32_t, 6> axis_words{};
    std::uint32_t pov_word_0x20{};
    std::array<std::uint8_t, 32> button_bytes_0x30{};
};

struct RaceControllerInputRefreshResult {
    bool state_written{};
    std::array<float, 6> axes{};
    std::uint32_t pov_direction_0x5098c0{};
    std::array<std::uint8_t, 32> button_bytes_0x5098a0{};
};

// Models the successful 0x416D30 GetDeviceState transformation and global
// writes. Device acquisition and the state-read result remain observations.
[[nodiscard]] RaceControllerInputRefreshResult refresh_race_controller_input(
    const RaceControllerInputRefreshObservation& observation) noexcept;

struct RaceMouseInputRefreshObservation {
    // Values returned by 0x414680 before the 0x444E10 normalization.
    std::int32_t raw_x{};
    std::int32_t raw_y{};
    // Raw dimensions read from globals 0x509A4C and 0x509A2C respectively.
    std::int32_t extent_x_0x509a4c{};
    std::int32_t extent_y_0x509a2c{};
    std::uint32_t button_mask{};
    bool fdiv_compatibility_enabled{};
};

struct RaceMouseInputRefreshResult {
    std::array<float, 2> axes_0x509660{};
    std::array<std::uint8_t, 3> button_bytes_0x509668{};
};

// Models 0x444E10's raw mouse-coordinate normalization and three button stores.
[[nodiscard]] RaceMouseInputRefreshResult refresh_race_mouse_input(
    const RaceMouseInputRefreshObservation& observation) noexcept;

// Evaluates one 0x446980 binding selector. Unsupported type/selector values
// and missing input data return zero exactly as observed at the switch exits.
[[nodiscard]] float evaluate_race_input_binding(
    const RaceInputBinding& binding,
    const RaceInputBindingObservation& observation) noexcept;

// 0x446F40 reports active above 0.2 (equality is inactive); its recovered
// unordered comparison flags also classify NaN as active.
[[nodiscard]] bool race_input_binding_is_active(
    const RaceInputBinding& binding,
    const RaceInputBindingObservation& observation) noexcept;

// Matches 0x446F90's record type predicate (types 1, 3, 4, and 6).
[[nodiscard]] bool race_input_binding_uses_ramped_path(
    const RaceInputBinding& binding) noexcept;

enum class RaceLoopBindingSlot : std::uint8_t {
    steering_negative_0x509338,
    steering_positive_0x5093c0,
    analog_0x509328,
    analog_0x509368,
    direct_value_0x5093b0,
    flag_0x509408,
    analog_value_0x509430,
    flag_0x5093d0,
    flag_0x509398,
    flag_0x5093e8,
    difference_value_0x509400,
    difference_value_0x509378,
    flag_0x5093e0,
    difference_value_0x5093d8,
    difference_value_0x509418,
    count,
};

inline constexpr std::size_t race_loop_binding_slot_count =
    static_cast<std::size_t>(RaceLoopBindingSlot::count);

struct RaceLoopBindingSamples {
    std::array<float, race_loop_binding_slot_count> values{};
    std::array<std::uint8_t, race_loop_binding_slot_count> active{};
    std::array<std::uint8_t, race_loop_binding_slot_count> uses_ramped_path{};
};

// Samples the binding records read by 0x43B290 from an input snapshot already
// refreshed by the caller, retaining the same slot order as the enum above.
[[nodiscard]] RaceLoopBindingSamples sample_race_loop_bindings(
    const std::array<RaceInputBinding, race_loop_binding_slot_count>& bindings,
    const RaceInputBindingObservation& observation) noexcept;

struct RaceLoopControlState {
    float value_0x5093b8{};
    float value_0x5093c8{};
    float difference_0x509344{};
    float difference_0x509340{};
    std::uint8_t flag_0x5093f0{};
    std::uint8_t flag_0x5093cc{};
    std::uint8_t flag_0x5093f8{};
    std::uint8_t flag_0x509360{};
    std::uint8_t flag_0x509420{};
    std::uint8_t transition_latch_0x509354{};
    std::int32_t counter_0x5093bc{};
};

struct RaceLoopControlObservation {
    float frame_delta{};
    float value_0x5093b0{};
    float value_0x509400{};
    float value_0x509378{};
    float value_0x5093d8{};
    float value_0x509418{};
    float value_0x509430{};
    std::int32_t counter_limit_0x50935c{};
    bool input_0x509408_active{};
    bool input_0x509430_uses_ramped_path{};
    bool input_0x509430_active{};
    bool input_0x5093d0_active{};
    bool input_0x509398_active{};
    bool input_0x5093e8_active{};
    bool input_0x5093e0_active{};
    bool input_0x509418_active{};
    bool input_0x5093d8_active{};
};

// Reconstructs the direct value-copy, difference, threshold, and latch/counter
// updates in the tail of 0x43B290. Input evaluations and the binding record
// type gate are supplied as observations from 0x446980/0x446F40/0x446F90.
void update_race_loop_control_state(
    RaceLoopControlState& state,
    const RaceLoopControlObservation& observation) noexcept;

struct RaceLoopSteeringState {
    float value_0x509358{};
};

struct RaceLoopSteeringObservation {
    float frame_delta{};
    float smoothing_0x509428{};
    float gain_0x509390{};
    float response_rate_0x5093a8{};
    float negative_binding_value_0x509338{};
    float positive_binding_value_0x5093c0{};
    bool input_0x5093a0_forces_response{};
    bool negative_binding_type_uses_ramped_path{};
    bool positive_binding_type_uses_ramped_path{};
    bool negative_binding_active{};
    bool positive_binding_active{};
};

// Reconstructs the smoothed steering-difference calculation and active-input
// decay/response path in 0x43B290. Binding values, type gates, and active tests
// are sampled observations; signed/unsigned float-word comparisons match the
// recovered x86 instructions.
void update_race_loop_steering(
    RaceLoopSteeringState& state,
    const RaceLoopSteeringObservation& observation) noexcept;

struct RaceLoopAnalogChannelState {
    float value{};
};

struct RaceLoopAnalogChannelObservation {
    float frame_delta{};
    float binding_value{};
    float binding_scale{};
    float binding_bias{};
    float ramp_rate{};
    bool force_ramped_path{};
    bool binding_type_uses_ramped_path{};
    bool binding_active{};
};

// Models either of 0x43B290's analog channels (0x5093F4 or 0x509320): direct
// scaled input when the binding is not ramped, otherwise frame-rate-based
// decay and rise using the channel-specific rate constants.
void update_race_loop_analog_channel(
    RaceLoopAnalogChannelState& state,
    const RaceLoopAnalogChannelObservation& observation) noexcept;

struct RaceLoopPeriodicUpdatePlan {
    bool periodic_tick{};
    std::array<RaceLoopPlannedCall, 2> calls{};
    std::uint8_t call_count{};
};

// Models 0x43B290's 1/18-second cadence gate and its observed helper dispatches.
[[nodiscard]] RaceLoopPeriodicUpdatePlan plan_race_loop_periodic_update_dispatch(
    RaceLoopPeriodicUpdateState& state,
    const RaceLoopPeriodicUpdateObservation& observation) noexcept;

struct RaceLoop43B290State {
    RaceLoopPeriodicUpdateState cadence{};
    RaceLoopSteeringState steering{};
    RaceLoopAnalogChannelState analog_0x5093f4{};
    RaceLoopAnalogChannelState analog_0x509320{};
    RaceLoopControlState controls{};
};

struct RaceLoop43B290Observation {
    float simulation_delta{};
    std::array<std::uint32_t, 3> periodic_callback_arguments_0x509384{};
    std::array<RaceInputBinding, race_loop_binding_slot_count> bindings{};
    // Input snapshots after the refresh boundary: index 0 when the periodic
    // tick is false, index 1 when it is true. Optional raw refresh observations
    // below can project controller/mouse results into the selected snapshot.
    std::array<RaceInputBindingObservation, 2> post_refresh_inputs{};
    // When present, models the raw mouse values consumed by this call's
    // unconditional 0x444E10 update; other devices stay in the snapshots.
    std::optional<RaceMouseInputRefreshObservation> mouse_refresh;
    // When present, models 0x416D30's periodic DirectInput state projection.
    std::optional<RaceControllerInputRefreshObservation> controller_refresh;
    bool force_ramped_path_0x5093a0{};
    float steering_smoothing_0x509428{};
    float steering_gain_0x509390{};
    float steering_response_rate_0x5093a8{};
    float analog_0x5093f4_scale_0x50934c{};
    float analog_0x5093f4_bias_0x509370{};
    float analog_0x5093f4_rate_0x509350{};
    float analog_0x509320_scale_0x509330{};
    float analog_0x509320_bias_0x509380{};
    float analog_0x509320_rate_0x509348{};
    std::int32_t counter_limit_0x50935c{};
};

struct RaceLoop43B290Plan {
    RaceLoopPeriodicUpdatePlan cadence_dispatch{};
    RaceInputBindingObservation sampled_inputs{};
    bool mouse_refresh_applied{};
    bool controller_refresh_applied{};
};

// Composes the observed full state-update sequence in 0x43B290. Binding
// records, post-refresh input snapshots, tuning globals, and force-feedback
// DWORDs are the remaining observations at helper boundaries.
[[nodiscard]] RaceLoop43B290Plan update_race_loop_43b290(
    RaceLoop43B290State& state,
    const RaceLoop43B290Observation& observation) noexcept;

struct RaceLoopSimulationFramePlan {
    RaceLoopSimulationStepPlan step{};
    RaceLoopPeriodicUpdatePlan periodic_update{};
};

// Composes 0x425920 timing with the known cadence boundary inside 0x43B290.
[[nodiscard]] RaceLoopSimulationFramePlan plan_race_loop_simulation_frame(
    RaceLoopSimulationStepState& step_state,
    RaceLoopPeriodicUpdateState& update_state,
    const RaceLoopSimulationStepObservation& step_observation,
    const std::array<std::uint32_t, 3>& periodic_callback_arguments) noexcept;

// Projects the ordered active-frame branch in 0x4020E0. Results from the
// renderer and mode-4 helper gates are observations; the call order and
// state-indexed callback slots are applied here.
[[nodiscard]] RaceLoopActiveFramePlan plan_race_loop_active_frame(
    const RaceLoopActiveFrameObservation& observation);

struct RaceLoopOuterState {
    std::int32_t startup_countdown_0x10{10};
    std::int32_t exit_poll_countdown_0x10{10};
    std::uint8_t exit_latch_0x4eb1e8{};
    std::int32_t page_0x4cf5b4{};
    std::uint32_t dispatched_page_0x508758{};
    std::uint8_t startup_setup_byte_0x4eb300{};
    std::uint8_t task_exit_byte_0x5087b4{};
    std::uint8_t initialization_latch_0x4e3220{};
    std::uint8_t physics_active_byte_0x508760{};
    std::uint8_t page_side_effect_gate_0x4eb20c{};
    EscapeMenuInitializationState escape_menu{};
    EscapeMenuEventState escape_menu_event{};
};

struct RaceLoopOuterObservation {
    // Return value sampled from the global accessor 0x40A160.
    std::int32_t poll_value_0x4d0c54{};
    // Value of the exit latch after the timeout handler / one-time setup
    // callbacks, when those callbacks are reached.
    bool exit_latch_after_timeout_handler{};
    bool exit_latch_after_startup_setup{};
    EscapeMenuInitializationObservation timeout_menu_initialization{};
    std::optional<EscapeMenuEventObservation> timeout_menu_event{};
    std::optional<std::int32_t> page_after_timeout_menu{};
    std::optional<std::int32_t> callback_state_after_timeout_menu{};
    bool page_side_effect_gate_0x4eb20c{};
    bool mode_0x49be40_active{};
    std::int32_t page_after_prelude_0x4cf5b4{};
    std::int32_t page_after_page11_handler_0x4cf5b4{};
    bool active_state_after_frame{};
    std::optional<SurfaceFlipObservation> surface_flip{};
    RaceLoopActiveFrameObservation frame{};
};

struct RaceLoopOuterIterationPlan {
    std::vector<RaceLoopPlannedCall> calls;
    RaceLoopActiveFramePlan frame{};
    EscapeMenuInitializationPlan timeout_menu_initialization{};
    std::optional<EscapeMenuEventPlan> timeout_menu_event{};
    bool page_store_to_12{};
    bool timeout_handler_reached{};
    bool startup_setup_reached{};
    bool exit_requested_at_page_gate{};
    bool returns_from_loop{};
    std::optional<SurfaceFlipPlan> surface_flip{};
};

// Models one ordered iteration of 0x4020E0 around the active-frame renderer.
// Results of opaque event, startup, and page callbacks remain observations.
[[nodiscard]] RaceLoopOuterIterationPlan plan_race_loop_outer_iteration(
    RaceLoopOuterState& state,
    const RaceLoopOuterObservation& observation);

struct RaceOverlayToggleFramePlan {
    RaceOverlayTransition transition{};
    RaceLoopActiveFramePlan following_frame{};
};

// Composes the accepted key-0x31 state transition with the next active-frame
// callback/render plan. Frame helper outcomes remain observed inputs.
[[nodiscard]] RaceOverlayToggleFramePlan plan_race_overlay_toggle_following_frame(
    std::int32_t current_state,
    const RaceLoopActiveFrameObservation& following_frame_observation);

enum class State1InputAction {
    none,
    select_help_page,
    select_info_page,
    select_metrics_page,
    select_save_page,
    toggle_overlay_flag,
    capture_camera_coordinates,
    increment_value_0x4d3df8,
    decrement_value_0x4d3df8,
    update_camera_controller,
    increment_value_0x4d3df4,
    decrement_value_0x4d3df4,
    restore_camera_coordinates,
    reset_camera_controller,
    translate_save_key,
};

struct State1InputState {
    std::int32_t page_0x4d3dd0{};
    std::uint8_t input_active_0x4f0b44{};
    std::uint8_t overlay_flag_0x4d3dd4{};
    float value_0x4d3df4{};
    float value_0x4d3df8{};
    std::uint32_t camera_coordinate_0x5081cc{};
    std::uint32_t camera_coordinate_0x5081d4{};
    std::uint32_t saved_coordinate_0x4f0b58{};
    std::uint32_t saved_coordinate_0x4f0b60{};
    float camera_controller_value_0x4f0b5c{};
};

struct State1InputObservation {
    std::uint16_t event{};
    float coordinate_increment_0x4c7560{};
    float controller_increment_0x4c7564{};
    std::array<std::uint32_t, 2> reset_source_coordinates_0x4f0b24{};
    // Result of the +0x25 gate on the object returned by 0x45C2C0 for the
    // 0x73 save-page event.
    bool save_page_gate_passed{};
    // Whether 0x4BDE40 maps the save-page event to character 0x67 ('g').
    bool save_key_maps_to_g{};
};

struct State1InputResult {
    State1InputAction action{State1InputAction::none};
    bool callback_return_value{};
    bool page_store_performed{};
    bool input_active_byte_cleared{};
    bool camera_controller_callback_reached{};
    bool camera_controller_virtual_0x40_reached{};
    std::array<std::uint32_t, 3> camera_controller_calls{};
    std::uint8_t camera_controller_call_count{};
};

// Applies the page and camera-key effects observed in the state-1 input
// callback at 0x41A370. The pointer-backed camera controller and key mapping
// results are observations supplied by the caller.
[[nodiscard]] State1InputResult handle_state1_input_event(
    State1InputState& state,
    const State1InputObservation& observation) noexcept;

} // namespace viper
