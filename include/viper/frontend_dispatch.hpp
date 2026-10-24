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

// Visible main-menu items and their recovered UI control values. Exit's -1
// control result is normalized by the original UI to selector 8.
enum class MainMenuItem {
    single_race,
    multiplayer,
    career,
    options,
    replay,
    exit,
};

[[nodiscard]] std::optional<int> main_menu_control_value(MainMenuItem item) noexcept;
[[nodiscard]] int normalize_main_menu_result(int result) noexcept;

struct MainMenuLabelRegistration {
    std::uint32_t initialized_mask_bit{};
    std::uint32_t resource_object_address{};
    std::uint32_t localization_key_address{};
    std::uint32_t atexit_callback_address{};
};

struct MainMenuScreenObservation {
    // Low eight bits are the one-time registration flags at DAT_00561008.
    std::uint32_t initialized_label_mask{};
    bool default_player_name_resource_stale{};
    // SingleRace, Career, Multiplayer, Replay, Options, Exit cache tests.
    std::array<bool, 6> label_resources_stale{};
    int raw_menu_result{};
};

struct MainMenuScreenPlan {
    std::vector<MainMenuLabelRegistration> registrations;
    std::vector<std::uint32_t> refreshed_resource_objects;
    std::uint32_t initialized_label_mask_after{};
    std::uint32_t player_name_prepare_helper_address{0x0046b020U};
    std::uint32_t interaction_helper_address{0x00472b30U};
    std::uint32_t player_name_save_helper_address{0x0046b080U};
    bool refresh_default_player_name{};
    bool interaction_called{true};
    bool player_name_saved{true};
    int raw_menu_result{};
    int normalized_menu_result{};
};

// Projects 0x4805C0's one-time label registrations, generation-driven
// resource refreshes, and result save/normalization around 0x472B30.
[[nodiscard]] MainMenuScreenPlan plan_main_menu_screen(
    const MainMenuScreenObservation& observation);

// The original frontend dispatches these selector values to distinct native
// handlers. Selector 7 is accepted by the switch but is not assigned to one
// of the visible main-menu entries recovered so far.
enum class FrontendHandler {
    state_0,
    state_1,
    state_2,
    state_3,
    state_4,
    selector_7_noop,
    return_to_caller,
    fallback_error_path,
};

[[nodiscard]] FrontendHandler frontend_handler_for(int selector) noexcept;

// Raw return classifications observed at the end of the pre-race UI routine
// (0x40EED0). Their button semantics have not been established.
enum class PreRaceScreenOutcome {
    default_outcome,
    outcome_from_minus_two,
    outcome_from_0x29a,
};

[[nodiscard]] PreRaceScreenOutcome classify_pre_race_screen_result(int result) noexcept;

// Caller-side routes observed in 0x40A410 after the screen's return value has
// been classified by 0x40EED0.
enum class PreRaceRoute {
    reopen_setup,
    initialize_current_entry,
    copy_entry_then_initialize,
    fallback_error_path,
};

[[nodiscard]] PreRaceRoute pre_race_route_for_screen_result(int result) noexcept;

// Escape-menu event codes observed in 0x4022F0. Events 0x170..0x17A select
// values 0..10; 0x37B selects 11. Event 0x17B does not store a selection.
[[nodiscard]] std::optional<unsigned char> escape_menu_selection_for_event(
    unsigned int event_code) noexcept;

struct StartupFrontendPlan {
    std::array<std::uint32_t, 16> call_addresses{};
    std::size_t call_count{};
    std::size_t main_dispatch_call_count{};
    bool main_dispatch_loop_reached{};
    bool call_sequence_truncated{};
};

// Projects the mode switch and call order in 0x401460. For mode zero, the
// supplied dispatch count includes the final 0x401E10 call that returns zero;
// the observed do-while body always calls the dispatcher at least once.
[[nodiscard]] StartupFrontendPlan plan_startup_frontend(
    int startup_mode,
    std::size_t mode_zero_dispatch_call_count = 0) noexcept;

struct StartupModeStringCopyObservation {
    std::uint32_t source_address{};
    std::optional<std::string> source_string;
};

struct StartupModeStringCopy {
    std::uint32_t source_address{};
    std::uint32_t destination_record_offset{};
    std::string copied_string;
    std::uint32_t bytes_copied{};
};

struct StartupModeEntryObservation {
    int startup_mode{};
    std::array<StartupModeStringCopyObservation, 3> strings{};
    std::optional<std::array<std::uint32_t, 12>> mode_4_words_0x4eb1b8;
};

struct StartupModeEntryPlan {
    bool input_complete{true};
    int startup_mode{};
    std::uint32_t handler_address{};
    std::optional<std::uint32_t> global_mode_value_0x4cf5b4;
    bool local_record_initializer_called{};
    std::vector<StartupModeStringCopy> record_string_copies;
    bool mode_4_global_words_copied{};
    std::optional<std::array<std::uint32_t, 12>> copied_mode_4_words;
    std::vector<std::uint32_t> calls;
};

// Expands the mode-specific handler body routed by 0x401460 for selectors 1-4.
[[nodiscard]] StartupModeEntryPlan plan_startup_mode_entry(
    const StartupModeEntryObservation& observation);

struct MultiplayerFrontendPlan {
    std::array<std::uint32_t, 2> entry_calls{0x45c5b0U, 0x40a230U};
    std::array<std::uint32_t, 3> active_iteration_calls{
        0x401ea0U, 0x40a400U, 0x40a230U};
    std::size_t zeroed_dword_count{0x335};
    std::uint32_t local_record_size{0xcd4U};
    std::uint32_t local_record_type{3U};
    std::uint32_t race_setup_result_pointer{};
    std::uint32_t race_setup_result_count_pointer{};
    std::size_t multiplayer_poll_call_count{};
    std::size_t race_setup_call_count{};
    std::size_t post_step_call_count{};
    bool loop_termination_observed{};
    bool next_poll_result_unknown{};
    bool local_record_reused_as_race_setup_input{true};
};

enum class MultiplayerResourceActionKind : std::uint8_t {
    unload_resource_set,
    load_resource_set,
};

struct MultiplayerResourceAction {
    MultiplayerResourceActionKind kind{};
    std::string resource_set;
    std::uint32_t helper{};
    // -1 for the shared difficulty/track/UI resources.
    std::int32_t entrant_index{-1};
};

struct MultiplayerPostStepObservation {
    std::int32_t participant_count_0xca8{};
    // Resource stems read from participant records at +0x39, stride 200.
    std::span<const std::string> entrant_resource_stems;
    std::int32_t difficulty_resource_index_0xccc{};
    // Resource stem passed from the record at +0x08 and suffixed with .trk.
    std::string_view track_resource_stem;
};

struct MultiplayerPostStepPlan {
    bool input_complete{true};
    std::array<std::uint32_t, 4> post_step_call_order{
        0x40a400U, 0x40a600U, 0x40a760U, 0x40a800U};
    std::size_t participant_unload_count{};
    std::vector<MultiplayerResourceAction> resource_actions;
};

// Projects the ordered resource work reached by 0x40A400 -> 0x40A600.
// Resource manager outcomes remain environmental observations.
[[nodiscard]] MultiplayerPostStepPlan plan_multiplayer_post_step(
    const MultiplayerPostStepObservation& observation);

struct MultiplayerFrontendObservation {
    std::span<const bool> poll_results;
    // One entry for each active poll that reaches 0x40A400, in loop order.
    std::span<const MultiplayerPostStepObservation> post_step_observations;
};

struct MultiplayerFrontendDetailedPlan {
    MultiplayerFrontendPlan loop{};
    bool post_step_observations_complete{true};
    // Null means the corresponding active iteration lacked post-step inputs.
    std::vector<std::optional<MultiplayerPostStepPlan>> post_steps;
};

// Composes each active poll iteration with the resource effects reached by its
// 0x40A400 post-step wrapper.
[[nodiscard]] MultiplayerFrontendDetailedPlan
plan_multiplayer_frontend_detailed(
    const MultiplayerFrontendObservation& observation);

struct MultiplayerTelephonyInitializationObservation {
    // CreateEventA outcomes, in the native 260-slot table's order.
    std::span<const bool> event_creation_results;
    bool initial_flag_0x5632cc{};
    // lineInitialize returns zero on success and writes the device count.
    std::optional<bool> line_initialize_succeeded;
    std::optional<std::size_t> line_device_count;
    // One lineNegotiateAPIVersion outcome per reported device.
    std::span<const bool> api_negotiation_results;
};

enum class MultiplayerTelephonyInitializationFailure : std::uint8_t {
    none,
    event_creation,
    line_initialize,
    api_negotiation,
};

struct MultiplayerTelephonyInitializationPlan {
    static constexpr std::size_t expected_event_count = 260;
    bool input_complete{true};
    bool preparation_helper_0x49b000_called{true};
    bool event_handles_are_manual_reset{true};
    bool event_handles_initially_signaled{};
    bool event_handles_unnamed{true};
    std::size_t event_creation_call_count{};
    std::optional<std::size_t> failed_event_index;
    bool line_initialize_called{};
    std::uint32_t line_initialize_callback{0x0049AFB0U};
    std::optional<std::size_t> device_count;
    std::uint32_t api_version_minimum{0x00010004U};
    std::uint32_t api_version_maximum{0x00010004U};
    std::size_t api_negotiation_call_count{};
    std::optional<std::size_t> failed_device_index;
    MultiplayerTelephonyInitializationFailure failure{
        MultiplayerTelephonyInitializationFailure::none};
    bool error_log_called{};
    std::uint32_t error_message_source{};
    std::optional<bool> initialized_flag_after;
    std::optional<bool> returned_flag;
};

// Models the TAPI event, line initialization, and API-version gates in
// 0x49AEA0. Native handles and API results stay observation inputs.
[[nodiscard]] MultiplayerTelephonyInitializationPlan
plan_multiplayer_telephony_initialization(
    const MultiplayerTelephonyInitializationObservation& observation);

struct MultiplayerConnectionObjectObservation {
    // FUN_00414170(0x3C) allocation result and FUN_00414A70 worker-table ID.
    std::optional<bool> object_allocation_succeeded;
    std::optional<std::uint32_t> worker_thread_id_0x30;
};

struct MultiplayerConnectionObjectPlan {
    bool input_complete{true};
    bool object_allocation_attempted{true};
    bool constructor_called{};
    bool live_info_registration_requested{};
    bool worker_enable_requested{};
    std::optional<bool> current_object_global_set_0x56434c;
    std::optional<bool> object_returned;
    bool failure_cleanup_called{};
    bool object_freed{};
};

// Models allocation, constructor publication, worker-ID validation, and the
// failed-construction cleanup in 0x49C620/0x49C670/0x49C950.
[[nodiscard]] MultiplayerConnectionObjectPlan
plan_multiplayer_connection_object(
    const MultiplayerConnectionObjectObservation& observation);

struct MultiplayerCommunicationSetupObservation {
    std::uint8_t prior_status_0x00{};
    std::uint8_t line_mode_0x04{};
    std::uint8_t direct_transport_variant_0x05{};
    std::optional<bool> line_session_object_created;
    std::optional<bool> line_interface_created;
    std::optional<bool> line_transport_object_created;
    std::optional<bool> direct_transport_object_created;
    std::optional<bool> multiplayer_ui_result;
};

enum class MultiplayerCommunicationSetupPath : std::uint8_t {
    existing_status,
    direct_transport,
    line_transport,
};

struct MultiplayerCommunicationSetupPlan {
    bool input_complete{true};
    MultiplayerCommunicationSetupPath path{};
    std::vector<std::uint32_t> calls;
    std::uint32_t line_open_mode_argument{};
    std::uint32_t line_open_flags{};
    bool line_address_set_called{};
    std::uint32_t line_address_copy_limit{0x10U};
    std::uint32_t direct_transport_helper{};
    bool line_session_null_dereference_possible{};
    bool line_interface_vtable_0x30_called{};
    std::optional<bool> status_byte_0x00_after;
    bool multiplayer_ui_called{};
    std::optional<bool> communication_helper_result;
    bool caller_failure_cleanup_reached{};
    bool caller_connection_global_cleared{};
    bool caller_retries_connection_setup{};
    std::array<std::uint32_t, 3> caller_failure_cleanup_call_order{
        0x49c6d0U, 0x414420U, 0x49b080U};
};

// Models the mode-selected line/direct construction and final multiplayer UI
// gate in 0x485940. Native transport and UI outcomes remain observations.
[[nodiscard]] MultiplayerCommunicationSetupPlan
plan_multiplayer_communication_setup(
    const MultiplayerCommunicationSetupObservation& observation);

struct MultiplayerWorkerIterationObservation {
    bool connection_global_present{};
    bool connection_object_active_byte_0{};
    bool line_session_at_0x0c_present{};
    bool communication_object_at_0x08_present{};
    bool transport_object_at_0x04_present{};
    std::optional<bool> thread_flag_0x18_0x414e20;
    std::optional<bool> thread_flag_0x19_0x414e40;
};

struct MultiplayerWorkerObservation {
    std::optional<bool> thread_priority_set_succeeded;
    std::optional<bool> startup_thread_flag_0x18_0x414e20;
    std::span<const MultiplayerWorkerIterationObservation> iterations;
};

struct MultiplayerWorkerIterationPlan {
    std::size_t iteration_index{};
    bool connection_updates_reached{};
    bool line_session_update_called{};
    bool communication_vtable_plus_4_called{};
    bool transport_vtable_plus_8_called{};
    std::optional<bool> thread_flag_0x18_0x414e20;
    bool thread_suspend_helper_0x414e80_called{};
    std::optional<bool> thread_flag_0x19_0x414e40;
};

struct MultiplayerWorkerPlan {
    bool input_complete{true};
    std::uint32_t initial_thread_priority_argument{0x0fU};
    std::uint32_t wait_interval_ms{0xfaU};
    std::optional<bool> thread_priority_set_succeeded;
    bool priority_failure_log_called{};
    bool panic_called{};
    bool panic_continuation_unknown{};
    bool initial_wait_call_reached{};
    std::optional<bool> startup_thread_flag_0x18_0x414e20;
    bool startup_thread_suspend_helper_0x414e80_called{};
    std::optional<bool> worker_returned;
    bool next_iteration_unknown{};
    std::vector<MultiplayerWorkerIterationPlan> iterations;
};

// Projects the callback fragment passed by 0x49C670 to the thread helper.
// Its original instructions at 0x49BD40 are not assigned a Ghidra function;
// thread-table flags and object state are supplied observations.
[[nodiscard]] MultiplayerWorkerPlan plan_multiplayer_worker(
    const MultiplayerWorkerObservation& observation);

struct MultiplayerStatusSnapshotObservation {
    // Consecutive values read along the reached path, which can remain inside
    // the communication-ready or status-0x11 wait loops.
    std::int32_t status_value{};
    bool communication_object_at_0x04_present{};
    std::optional<bool> communication_query_result_0x24;
    std::optional<std::string_view> status_0x0f_text;
    // Capacity in bytes for the caller-owned buffer passed to virtual +0x6C.
    std::optional<std::size_t> destination_capacity;
    std::optional<std::int32_t> connection_index_0x18;
    // Contiguous 0x1C-byte connection rows beginning at ChatLine +0x14's
    // selected connection array. The status-15 send reads row bytes +0x18/+0x19.
    std::span<const std::uint8_t> connection_records_0x14;
    // Inputs read by 0x4A9C40 after its packet allocation succeeds.
    std::optional<bool> status_0x0f_packet_allocation_succeeded;
    std::optional<bool> status_0x0f_sender_state_0x20_present;
    std::optional<std::uint8_t> status_0x0f_sender_sequence_0x28;
    std::optional<std::uint8_t> status_0x0f_pending_sequence_0x01;
    std::optional<bool> status_0x0f_pending_queue_head_0x24_present;
};

enum class MultiplayerPacketDeliveryRoute : std::uint8_t {
    not_called,
    inputs_incomplete,
    allocation_failed,
    immediate_dispatch,
    pending_queue,
};

enum class MultiplayerStatusLabelId : std::uint8_t {
    pgs_invalid,
    server_refused_connection,
    server_not_found,
    unconnected,
    pgs_error_not_a_state,
    searching_for_server,
    server_found_waiting_for_reply,
    pgs_connected_not_a_state,
    pgs_hello,
    pgs_running,
    pgs_proceeding,
    pgs_car_choice,
    pgs_car_waiting,
    pgs_car_get,
    pgs_race_waiting,
    pgs_race_begin,
    pgs_synchronizing,
    pgs_postsync_waiting,
    pgs_go_waiting,
    pgs_go_ready,
    pgs_greenflag_waiting,
    pgs_greenflag,
    unknown_index,
};

[[nodiscard]] MultiplayerStatusLabelId multiplayer_status_label_id(
    std::int32_t status_value) noexcept;

struct MultiplayerStatusSupervisorObservation {
    std::optional<bool> thread_identity_matches;
    std::optional<bool> worker_stopped_within_cleanup_timeout;
    bool synchronizing_text_already_initialized{};
    bool waiting_text_already_initialized{};
    std::span<const MultiplayerStatusSnapshotObservation> snapshots;
};

struct MultiplayerStatusSnapshotPlan {
    std::size_t snapshot_index{};
    std::int32_t status_value{};
    MultiplayerStatusLabelId label_id{MultiplayerStatusLabelId::unknown_index};
    bool communication_query_called{};
    std::optional<bool> communication_query_result_0x24;
    bool update_tick_called{};
    bool waiting_text_path_reached{};
    bool status_0x0f_virtual_plus_0x6c_called{};
    bool status_0x0f_virtual_plus_0x68_called{};
    bool status_0x0f_string_lookup_called{};
    std::optional<std::size_t> status_0x0f_copy_size_including_nul;
    std::optional<bool> destination_overrun_possible;
    bool status_0x0f_virtual_plus_0x50_called{};
    bool status_0x0f_transition_helper_0x4a3ac0_called{};
    std::optional<std::int32_t> status_after_virtual_plus_0x50;
    bool status_0x0f_send_input_complete{true};
    std::optional<std::size_t> status_0x0f_connection_record_offset;
    std::optional<bool> status_0x0f_send_gate_open;
    bool status_0x0f_sender_0x4a8be0_called{};
    std::uint32_t status_0x0f_sender_object_offset_from_chatline{0x7cU};
    std::uint32_t status_0x0f_send_target_offset_in_connection_record{0x0cU};
    std::uint32_t status_0x0f_message_length{0x34U};
    bool status_0x0f_sender_final_argument_is_null{true};
    // Packet fields written by 0x4A9B50 after 0x4A9C40 allocates a packet.
    std::uint32_t status_0x0f_packet_payload_offset{};
    std::uint32_t status_0x0f_packet_length_field_offset{0xe4U};
    std::uint32_t status_0x0f_packet_zero_field_offset{0xe8U};
    std::uint32_t status_0x0f_packet_remote_address_offset{0xecU};
    std::uint32_t status_0x0f_packet_remote_address_size{0x0cU};
    MultiplayerPacketDeliveryRoute status_0x0f_packet_delivery_route{
        MultiplayerPacketDeliveryRoute::not_called};
    bool status_0x0f_immediate_dispatch_helper_0x4a9c10_called{};
    bool status_0x0f_pending_queue_helper_0x4a9a80_called{};
    std::array<std::uint8_t, 8> status_0x0f_message_prefix{};
    std::uint8_t status_0x0f_message_prefix_known_mask{};
    std::array<std::uint32_t, 2> status_0x0f_sender_nested_calls{{
        0x004a8d90U, 0x004a9c40U,
    }};
};

struct MultiplayerStatusSupervisorPlan {
    bool input_complete{true};
    std::optional<bool> thread_identity_matches;
    std::optional<bool> thread_identity_cleanup_called;
    std::optional<bool> worker_cleanup_timed_out;
    bool cleanup_panic_called{};
    bool cleanup_panic_continuation_unknown{};
    bool synchronizing_text_initialization_called{};
    bool waiting_text_initialization_called{};
    bool initial_synchronizing_text_draw_called{true};
    bool initial_thread_priority_set_to_2{true};
    bool terminal_status_text_draw_called{};
    bool terminal_thread_priority_set_to_0{};
    bool completion_global_published{};
    bool completion_global_cleared{};
    bool no_op_completion_helper_called{};
    std::optional<std::uint8_t> return_low_byte;
    std::optional<std::uint32_t> return_upper_24_bits;
    bool return_upper_24_bits_unknown{};
    bool next_snapshot_unknown{};
    std::vector<MultiplayerStatusSnapshotPlan> snapshots;
};

// Projects the numeric-state supervisor at 0x49C2E0. State meanings,
// virtual methods, and the caller-provided status-0x0F buffer remain inputs.
[[nodiscard]] MultiplayerStatusSupervisorPlan
plan_multiplayer_status_supervisor(
    const MultiplayerStatusSupervisorObservation& observation);

struct MultiplayerStatusCallbackObservation {
    bool status_object_global_present{};
    // Signed DWORD at ((global + 8) + 4), used only for the > 4 gate.
    std::optional<std::int32_t> status_gate_value;
    // Original 0x49C5C0 parameter passed unchanged to virtual +0x84.
    std::optional<std::int32_t> callback_argument;
    // Raw bytes beginning at the Race's ChatLine object base (this pointer).
    // The known ChatLine target reads +0x1C8 + callback_argument * 0xD9.
    std::span<const std::uint8_t> chatline_object_bytes;
};

struct MultiplayerStatusCallbackPlan {
    bool input_complete{true};
    bool virtual_plus_0x84_called{};
    bool status_label_lookup_called{};
    std::optional<MultiplayerStatusLabelId> status_label_id;
    bool no_op_helper_0x4a1af0_called{};
    std::optional<std::int64_t> indexed_status_flag_byte_offset;
    std::optional<std::uint8_t> returned_low_byte;
};

// Models 0x49C5C0 and its Race's ChatLine +0x84 target at 0x4A1B70. A short
// byte span reports incomplete input instead of reading outside caller memory.
[[nodiscard]] MultiplayerStatusCallbackPlan plan_multiplayer_status_callback(
    const MultiplayerStatusCallbackObservation& observation);

struct MultiplayerTelephonySlotObservation {
    // The slot's first DWORD is not -1, so 0x49AD40 waits on its event handle.
    bool primary_slot_value_active{};
    bool completion_event_handle_present{};
    std::optional<std::uint32_t> wait_result;
};

struct MultiplayerTelephonyShutdownObservation {
    std::span<const MultiplayerTelephonySlotObservation> slots;
};

struct MultiplayerTelephonySlotShutdownPlan {
    std::size_t slot_index{};
    bool wait_called{};
    bool wait_used_missing_event_handle{};
    std::optional<std::uint32_t> wait_result;
    std::optional<bool> primary_value_remains_after_wait;
    bool abandoned_wait_warning{};
    bool wait_api_error{};
    bool fatal_wait_result{};
    bool pending_data_warning{};
    std::optional<bool> primary_handle_closed;
    std::optional<bool> completion_event_closed;
};

struct MultiplayerTelephonyShutdownPlan {
    static constexpr std::size_t expected_slot_count = 260;
    bool input_complete{true};
    std::uint32_t wait_timeout_ms{};
    std::optional<std::int32_t> active_counter_after_quiesce;
    std::optional<bool> line_shutdown_called;
    std::optional<bool> line_handle_cleared;
    std::optional<bool> initialized_flag_cleared;
    std::optional<std::size_t> primary_handle_close_count;
    std::optional<std::size_t> completion_event_close_count;
    std::vector<MultiplayerTelephonySlotShutdownPlan> slots;
};

// Models the serial-I/O quiesce and TAPI/handle cleanup in 0x49AD40/0x49B080.
[[nodiscard]] MultiplayerTelephonyShutdownPlan
plan_multiplayer_telephony_shutdown(
    const MultiplayerTelephonyShutdownObservation& observation);

// Projects the 0x40A570 multiplayer loop from consecutive 0x40A230 results.
// If all supplied polls are true, the next poll is reached but its result is
// unknown; a false result ends the loop immediately.
[[nodiscard]] MultiplayerFrontendPlan plan_multiplayer_frontend(
    std::span<const bool> multiplayer_poll_results) noexcept;

struct CareerFrontendGlobals {
    std::uint32_t saved_value_0x4d10f8{};
    std::uint32_t current_value_0x4d10fc{};
};

struct CareerFrontendPlan {
    std::array<std::uint32_t, 4> call_template{
        0x40d0e0U, 0x4ac520U, 0x4aa490U, 0x40d100U};
    std::size_t menu_poll_call_count{};
    std::size_t selected_action_call_count{};
    bool menu_exit_observed{};
    bool next_menu_result_unknown{};
    bool globals_restored{};
};

// Models the career menu loop at 0x4AA460. Each nonnegative menu result is
// passed to 0x4AA490; the first negative result exits and restores the globals.
[[nodiscard]] CareerFrontendPlan plan_career_frontend(
    CareerFrontendGlobals& globals,
    std::span<const int> menu_results) noexcept;

struct CareerTestingRaceObservation {
    // Raw active career payload beginning at the base used by 0x4AA3E0.
    std::span<const std::uint8_t> career_payload;
    std::string_view selected_track;
    std::uint8_t reversed_option{};
    // Results returned by successive 0x40EED0 calls. Values 1 and 2 start
    // 0x401EA0 in modes 0 and 1; any other value ends the loop.
    std::span<const int> race_menu_results;
};

struct CareerTestingRacePlan {
    bool input_complete{true};
    std::size_t track_copy_size_including_nul{};
    bool track_local_overrun_possible{};
    std::optional<std::uint32_t> career_dword_0x10;
    std::optional<std::uint8_t> career_byte_0x14;
    std::optional<std::uint32_t> career_dword_0x124;
    std::optional<std::uint32_t> career_dword_0x124_plus_four;
    std::optional<std::uint32_t> career_dword_0x124_plus_three;
    std::uint8_t reversed_option{};
    std::size_t menu_interaction_count{};
    std::vector<std::uint8_t> race_setup_modes;
    bool loop_exit_observed{};
    bool next_menu_result_unknown{};
    bool race_context_cleanup_called{};
    bool season_reload_called{};
};

// Models the career testing race loop at 0x4AEC30. The career's raw fields
// and each nested menu result remain caller-supplied observations.
[[nodiscard]] CareerTestingRacePlan plan_career_testing_race(
    const CareerTestingRaceObservation& observation);

} // namespace viper
