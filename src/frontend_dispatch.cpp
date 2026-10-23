#include "viper/frontend_dispatch.hpp"

#include <algorithm>
#include <utility>

namespace viper {

std::optional<int> main_menu_control_value(const MainMenuItem item) noexcept
{
    switch (item) {
    case MainMenuItem::single_race: return 0;
    case MainMenuItem::multiplayer: return 1;
    case MainMenuItem::career: return 2;
    case MainMenuItem::options: return 3;
    case MainMenuItem::replay: return 4;
    case MainMenuItem::exit: return -1;
    }
    return std::nullopt;
}

int normalize_main_menu_result(const int result) noexcept
{
    return result > -3 && result < 0 ? 8 : result;
}

FrontendHandler frontend_handler_for(const int selector) noexcept
{
    switch (selector) {
    case 0: return FrontendHandler::state_0; // original handler: 0x40A410
    case 1: return FrontendHandler::state_1; // original handler: 0x40A570
    case 2: return FrontendHandler::state_2; // original handler: 0x4AA460
    case 3: return FrontendHandler::state_3; // original handler: 0x47B4E0
    case 4: return FrontendHandler::state_4; // original handler: 0x406950
    case 7: return FrontendHandler::selector_7_noop; // 0x41C870 returns zero
    case 8: return FrontendHandler::return_to_caller;
    default: return FrontendHandler::fallback_error_path;
    }
}

PreRaceScreenOutcome classify_pre_race_screen_result(const int result) noexcept
{
    switch (result) {
    case -2: return PreRaceScreenOutcome::outcome_from_minus_two;
    case 0x29a: return PreRaceScreenOutcome::outcome_from_0x29a;
    default: return PreRaceScreenOutcome::default_outcome;
    }
}

PreRaceRoute pre_race_route_for_screen_result(const int result) noexcept
{
    switch (result) {
    case 0: return PreRaceRoute::reopen_setup;
    case 1: return PreRaceRoute::initialize_current_entry;
    case 2: return PreRaceRoute::copy_entry_then_initialize;
    default: return PreRaceRoute::fallback_error_path;
    }
}

std::optional<unsigned char> escape_menu_selection_for_event(
    const unsigned int event_code) noexcept
{
    if (event_code >= 0x170 && event_code <= 0x17a) {
        return static_cast<unsigned char>(event_code - 0x170);
    }
    if (event_code == 0x37b) {
        return static_cast<unsigned char>(11U);
    }
    return std::nullopt;
}

StartupFrontendPlan plan_startup_frontend(
    const int startup_mode,
    const std::size_t mode_zero_dispatch_call_count) noexcept
{
    StartupFrontendPlan plan{};
    const auto call = [&plan](const std::uint32_t address) {
        if (plan.call_count < plan.call_addresses.size()) {
            plan.call_addresses[plan.call_count++] = address;
        }
    };

    call(0x417270U);
    switch (startup_mode) {
    case 0:
        call(0x4014d0U);
        plan.main_dispatch_loop_reached = true;
        plan.main_dispatch_call_count = mode_zero_dispatch_call_count == 0
            ? 1
            : mode_zero_dispatch_call_count;
        for (std::size_t index = 0;
             index < plan.main_dispatch_call_count &&
                 plan.call_count < plan.call_addresses.size() - 3;
             ++index) {
            call(0x401e10U);
        }
        plan.call_sequence_truncated =
            plan.main_dispatch_call_count > plan.call_count - 2;
        call(0x4ba310U);
        call(0x401650U);
        break;
    case 1: call(0x402be0U); break;
    case 2: call(0x402a90U); break;
    case 3: call(0x402a80U); break;
    case 4: call(0x402aa0U); break;
    default: break;
    }
    call(0x4172a0U);
    return plan;
}

StartupModeEntryPlan plan_startup_mode_entry(
    const StartupModeEntryObservation& observation)
{
    StartupModeEntryPlan plan{};
    plan.startup_mode = observation.startup_mode;
    std::array<std::uint32_t, 3> expected_sources{};
    std::array<std::uint32_t, 3> destination_offsets{};
    switch (observation.startup_mode) {
    case 1:
        plan.handler_address = 0x402BE0U;
        plan.global_mode_value_0x4cf5b4 = 4U;
        expected_sources = {0x4CF9A4U, 0x4CF9ACU, 0x4CF9B4U};
        destination_offsets = {0x08U, 0x2CU, 0x39U};
        break;
    case 2:
        plan.handler_address = 0x402A90U;
        plan.calls.push_back(plan.handler_address);
        return plan;
    case 3:
        plan.handler_address = 0x402A80U;
        plan.calls = {plan.handler_address, 0x4014D0U, 0x401650U};
        return plan;
    case 4:
        plan.handler_address = 0x402AA0U;
        plan.global_mode_value_0x4cf5b4 = 11U;
        expected_sources = {0x4EB130U, 0x4CF994U, 0x4CF99CU};
        destination_offsets = {0x08U, 0x2CU, 0x39U};
        break;
    default:
        plan.input_complete = false;
        return plan;
    }

    plan.calls.push_back(plan.handler_address);
    plan.calls.push_back(0x45C5B0U);
    plan.local_record_initializer_called = true;
    for (std::size_t index = 0; index < expected_sources.size(); ++index) {
        const auto& source = observation.strings[index];
        if (source.source_address != expected_sources[index] || !source.source_string) {
            plan.input_complete = false;
            continue;
        }
        const auto nul = source.source_string->find('\0');
        const auto copied = source.source_string->substr(0, nul);
        plan.record_string_copies.push_back({
            expected_sources[index], destination_offsets[index], copied,
            static_cast<std::uint32_t>(copied.size() + 1U)});
    }

    if (observation.startup_mode == 4) {
        plan.mode_4_global_words_copied = true;
        if (observation.mode_4_words_0x4eb1b8) {
            plan.copied_mode_4_words = observation.mode_4_words_0x4eb1b8;
        } else {
            plan.input_complete = false;
        }
    }

    plan.calls.insert(plan.calls.end(), {
        0x4014D0U, 0x40A5D0U, 0x401EA0U, 0x40A600U, 0x401650U});
    return plan;
}

MultiplayerFrontendPlan plan_multiplayer_frontend(
    const std::span<const bool> multiplayer_poll_results) noexcept
{
    MultiplayerFrontendPlan plan{};
    for (const bool active : multiplayer_poll_results) {
        ++plan.multiplayer_poll_call_count;
        if (!active) {
            plan.loop_termination_observed = true;
            return plan;
        }
        ++plan.race_setup_call_count;
        ++plan.post_step_call_count;
    }

    // The do/while-shaped call chain reaches another poll after every active
    // result. Its return value is outside the supplied observation span.
    ++plan.multiplayer_poll_call_count;
    plan.next_poll_result_unknown = true;
    return plan;
}

MultiplayerPostStepPlan plan_multiplayer_post_step(
    const MultiplayerPostStepObservation& observation)
{
    constexpr std::array<std::string_view, 8> difficulty_resources{{
        "easy.res", "medium.res", "hard.res", "career1.res",
        "career2.res", "career3.res", "career4.res", "career5.res",
    }};

    MultiplayerPostStepPlan plan{};
    const auto requested_count = observation.participant_count_0xca8 > 0
        ? static_cast<std::size_t>(observation.participant_count_0xca8)
        : 0U;
    const auto available_count = std::min(
        requested_count, observation.entrant_resource_stems.size());
    plan.participant_unload_count = available_count;
    if (available_count != requested_count) {
        plan.input_complete = false;
    }
    plan.resource_actions.reserve(available_count + 4U);

    const auto c_string_prefix = [](const std::string_view value) {
        const auto nul = value.find('\0');
        return value.substr(0, nul);
    };
    for (std::size_t index = 0; index < available_count; ++index) {
        plan.resource_actions.push_back({
            MultiplayerResourceActionKind::unload_resource_set,
            std::string(c_string_prefix(observation.entrant_resource_stems[index])) +
                ".car",
            0x004178C0U,
            static_cast<std::int32_t>(index),
        });
    }

    const auto difficulty_index = observation.difficulty_resource_index_0xccc;
    if (difficulty_index >= 0 &&
        static_cast<std::size_t>(difficulty_index) < difficulty_resources.size()) {
        plan.resource_actions.push_back({
            MultiplayerResourceActionKind::unload_resource_set,
            std::string(difficulty_resources[
                static_cast<std::size_t>(difficulty_index)]),
            0x004178C0U,
            -1,
        });
    } else {
        plan.input_complete = false;
    }

    plan.resource_actions.push_back({
        MultiplayerResourceActionKind::unload_resource_set,
        std::string(c_string_prefix(observation.track_resource_stem)) + ".trk",
        0x004178C0U,
        -1,
    });
    plan.resource_actions.push_back({
        MultiplayerResourceActionKind::unload_resource_set,
        "race.res",
        0x004178C0U,
        -1,
    });
    plan.resource_actions.push_back({
        MultiplayerResourceActionKind::load_resource_set,
        "ui.res",
        0x00417740U,
        -1,
    });
    return plan;
}

MultiplayerFrontendDetailedPlan plan_multiplayer_frontend_detailed(
    const MultiplayerFrontendObservation& observation)
{
    MultiplayerFrontendDetailedPlan result{};
    result.loop = plan_multiplayer_frontend(observation.poll_results);
    result.post_steps.reserve(result.loop.race_setup_call_count);
    for (std::size_t iteration = 0;
         iteration < result.loop.race_setup_call_count; ++iteration) {
        if (iteration >= observation.post_step_observations.size()) {
            result.post_steps.emplace_back(std::nullopt);
            result.post_step_observations_complete = false;
            continue;
        }
        auto post_step = plan_multiplayer_post_step(
            observation.post_step_observations[iteration]);
        if (!post_step.input_complete) {
            result.post_step_observations_complete = false;
        }
        result.post_steps.emplace_back(std::move(post_step));
    }
    return result;
}

MultiplayerTelephonyInitializationPlan
plan_multiplayer_telephony_initialization(
    const MultiplayerTelephonyInitializationObservation& observation)
{
    MultiplayerTelephonyInitializationPlan result{};
    result.initialized_flag_after = observation.initial_flag_0x5632cc;
    const auto observed_event_count = std::min(
        observation.event_creation_results.size(),
        MultiplayerTelephonyInitializationPlan::expected_event_count);
    for (std::size_t index = 0; index < observed_event_count; ++index) {
        ++result.event_creation_call_count;
        if (!observation.event_creation_results[index]) {
            result.failure = MultiplayerTelephonyInitializationFailure::event_creation;
            result.failed_event_index = index;
            result.returned_flag = false;
            return result;
        }
    }
    if (observed_event_count !=
        MultiplayerTelephonyInitializationPlan::expected_event_count) {
        result.input_complete = false;
        return result;
    }

    result.line_initialize_called = true;
    if (!observation.line_initialize_succeeded.has_value()) {
        result.input_complete = false;
        result.initialized_flag_after = std::nullopt;
        return result;
    }
    if (!*observation.line_initialize_succeeded) {
        result.failure = MultiplayerTelephonyInitializationFailure::line_initialize;
        result.error_log_called = true;
        result.error_message_source = 0x004E2F6CU;
        result.returned_flag = observation.initial_flag_0x5632cc;
        return result;
    }

    result.initialized_flag_after = true;
    if (!observation.line_device_count.has_value()) {
        result.input_complete = false;
        return result;
    }
    result.device_count = *observation.line_device_count;
    const auto observed_negotiation_count = std::min(
        observation.api_negotiation_results.size(), *result.device_count);
    for (std::size_t device = 0; device < observed_negotiation_count; ++device) {
        ++result.api_negotiation_call_count;
        if (!observation.api_negotiation_results[device]) {
            result.failure = MultiplayerTelephonyInitializationFailure::api_negotiation;
            result.failed_device_index = device;
            result.error_log_called = true;
            result.error_message_source = 0x004E2F48U;
            result.initialized_flag_after = false;
            result.returned_flag = false;
            return result;
        }
    }
    if (observed_negotiation_count != *result.device_count) {
        result.input_complete = false;
        result.initialized_flag_after = std::nullopt;
        return result;
    }

    result.returned_flag = true;
    return result;
}

MultiplayerConnectionObjectPlan plan_multiplayer_connection_object(
    const MultiplayerConnectionObjectObservation& observation)
{
    MultiplayerConnectionObjectPlan result{};
    if (!observation.object_allocation_succeeded.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.object_allocation_succeeded) {
        result.object_returned = false;
        return result;
    }

    result.constructor_called = true;
    result.live_info_registration_requested = true;
    result.worker_enable_requested = true;
    result.current_object_global_set_0x56434c = true;
    if (!observation.worker_thread_id_0x30.has_value()) {
        result.input_complete = false;
        result.current_object_global_set_0x56434c = std::nullopt;
        return result;
    }
    if (*observation.worker_thread_id_0x30 == 0) {
        result.failure_cleanup_called = true;
        result.object_freed = true;
        result.current_object_global_set_0x56434c = false;
        result.object_returned = false;
        return result;
    }

    result.object_returned = true;
    return result;
}

MultiplayerCommunicationSetupPlan plan_multiplayer_communication_setup(
    const MultiplayerCommunicationSetupObservation& observation)
{
    MultiplayerCommunicationSetupPlan result{};
    const auto finish_from_transport = [&](const std::optional<bool> transport_present) {
        if (!transport_present.has_value()) {
            result.input_complete = false;
            return;
        }
        result.status_byte_0x00_after = *transport_present;
        if (!*transport_present) {
            result.communication_helper_result = false;
            result.caller_failure_cleanup_reached = true;
            result.caller_connection_global_cleared = true;
            result.caller_retries_connection_setup = true;
            return;
        }
        result.multiplayer_ui_called = true;
        result.calls.push_back(0x00485A10U);
        if (!observation.multiplayer_ui_result.has_value()) {
            result.input_complete = false;
            return;
        }
        result.communication_helper_result = *observation.multiplayer_ui_result;
        result.caller_failure_cleanup_reached = !*observation.multiplayer_ui_result;
        result.caller_connection_global_cleared =
            result.caller_failure_cleanup_reached;
        result.caller_retries_connection_setup =
            result.caller_failure_cleanup_reached;
    };

    if (observation.prior_status_0x00 != 0) {
        result.path = MultiplayerCommunicationSetupPath::existing_status;
        result.status_byte_0x00_after = true;
        result.calls = {0x00485E30U, 0x00485A10U};
        result.multiplayer_ui_called = true;
        if (observation.multiplayer_ui_result.has_value()) {
            result.communication_helper_result = *observation.multiplayer_ui_result;
            result.caller_failure_cleanup_reached =
                !*observation.multiplayer_ui_result;
            result.caller_connection_global_cleared =
                result.caller_failure_cleanup_reached;
            result.caller_retries_connection_setup =
                result.caller_failure_cleanup_reached;
        } else {
            result.input_complete = false;
        }
        return result;
    }

    const bool line_mode = observation.line_mode_0x04 != 0;
    result.path = line_mode
        ? MultiplayerCommunicationSetupPath::line_transport
        : MultiplayerCommunicationSetupPath::direct_transport;
    result.line_open_mode_argument = line_mode ? 9U : 1U;
    result.line_open_flags = line_mode ? 1U : 0U;
    result.calls.push_back(0x004A1100U);
    result.calls.push_back(0x004A1000U);
    result.line_address_set_called = true;

    if (!observation.line_session_object_created.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.line_session_object_created) {
        // 0x485940 calls the member helper unconditionally after 0x4A1100;
        // 0x4A1000 dereferences this+0xD4 without a null check.
        result.line_session_null_dereference_possible = true;
        result.input_complete = false;
        return result;
    }

    if (!line_mode) {
        result.calls.push_back(0x00485E70U);
        result.direct_transport_helper =
            observation.direct_transport_variant_0x05 != 0
                ? 0x004A3D20U
                : 0x004A3CD0U;
        result.calls.push_back(result.direct_transport_helper);
        finish_from_transport(observation.direct_transport_object_created);
        return result;
    }

    result.calls.push_back(0x00485E40U);
    if (!observation.line_interface_created.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.line_interface_created) {
        finish_from_transport(std::optional<bool>{false});
        return result;
    }

    result.line_interface_vtable_0x30_called = true;
    result.calls.push_back(0x004A3D70U);
    finish_from_transport(observation.line_transport_object_created);
    return result;
}

MultiplayerWorkerPlan plan_multiplayer_worker(
    const MultiplayerWorkerObservation& observation)
{
    MultiplayerWorkerPlan result{};
    result.thread_priority_set_succeeded =
        observation.thread_priority_set_succeeded;
    if (!observation.thread_priority_set_succeeded.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.thread_priority_set_succeeded) {
        result.priority_failure_log_called = true;
        result.panic_called = true;
        result.panic_continuation_unknown = true;
        result.input_complete = false;
        return result;
    }
    result.initial_wait_call_reached = true;

    result.startup_thread_flag_0x18_0x414e20 =
        observation.startup_thread_flag_0x18_0x414e20;
    if (!observation.startup_thread_flag_0x18_0x414e20.has_value()) {
        result.input_complete = false;
        return result;
    }
    result.startup_thread_suspend_helper_0x414e80_called =
        *observation.startup_thread_flag_0x18_0x414e20;

    for (std::size_t index = 0; index < observation.iterations.size(); ++index) {
        const auto& observed = observation.iterations[index];
        MultiplayerWorkerIterationPlan iteration{};
        iteration.iteration_index = index;
        iteration.connection_updates_reached =
            observed.connection_global_present &&
            observed.connection_object_active_byte_0;
        if (iteration.connection_updates_reached) {
            iteration.line_session_update_called =
                observed.line_session_at_0x0c_present;
            iteration.communication_vtable_plus_4_called =
                observed.communication_object_at_0x08_present;
            iteration.transport_vtable_plus_8_called =
                observed.transport_object_at_0x04_present;
        }
        iteration.thread_flag_0x18_0x414e20 =
            observed.thread_flag_0x18_0x414e20;
        if (!observed.thread_flag_0x18_0x414e20.has_value()) {
            result.input_complete = false;
        } else {
            iteration.thread_suspend_helper_0x414e80_called =
                *observed.thread_flag_0x18_0x414e20;
        }
        iteration.thread_flag_0x19_0x414e40 =
            observed.thread_flag_0x19_0x414e40;
        if (!observed.thread_flag_0x19_0x414e40.has_value()) {
            result.input_complete = false;
        } else if (*observed.thread_flag_0x19_0x414e40) {
            result.worker_returned = true;
            result.iterations.push_back(iteration);
            return result;
        } else {
            result.worker_returned = false;
        }
        result.iterations.push_back(iteration);
    }

    result.next_iteration_unknown = true;
    return result;
}

MultiplayerStatusLabelId multiplayer_status_label_id(
    const std::int32_t status_value) noexcept
{
    if (status_value < 0 || status_value >= 22) {
        return MultiplayerStatusLabelId::unknown_index;
    }
    return static_cast<MultiplayerStatusLabelId>(status_value);
}

MultiplayerStatusSupervisorPlan plan_multiplayer_status_supervisor(
    const MultiplayerStatusSupervisorObservation& observation)
{
    enum class SnapshotPhase {
        outer_state_dispatch,
        communication_ready_wait,
        status_0x11_wait,
    };

    MultiplayerStatusSupervisorPlan result{};
    result.thread_identity_matches = observation.thread_identity_matches;
    if (observation.thread_identity_matches.has_value()) {
        result.thread_identity_cleanup_called =
            !*observation.thread_identity_matches;
        if (!*observation.thread_identity_matches) {
            result.worker_cleanup_timed_out =
                observation.worker_stopped_within_cleanup_timeout.has_value()
                    ? std::optional<bool>{
                        !*observation.worker_stopped_within_cleanup_timeout}
                    : std::nullopt;
            result.cleanup_panic_called =
                observation.worker_stopped_within_cleanup_timeout.has_value() &&
                !*observation.worker_stopped_within_cleanup_timeout;
            result.cleanup_panic_continuation_unknown =
                result.cleanup_panic_called;
            if (!observation.worker_stopped_within_cleanup_timeout.has_value()) {
                result.input_complete = false;
            } else if (result.cleanup_panic_called) {
                result.input_complete = false;
            }
        }
    } else {
        result.input_complete = false;
    }
    result.synchronizing_text_initialization_called =
        !observation.synchronizing_text_already_initialized;
    result.waiting_text_initialization_called =
        !observation.waiting_text_already_initialized;
    result.snapshots.reserve(observation.snapshots.size());
    auto phase = SnapshotPhase::outer_state_dispatch;

    for (std::size_t index = 0; index < observation.snapshots.size(); ++index) {
        const auto& observed = observation.snapshots[index];
        MultiplayerStatusSnapshotPlan snapshot{};
        snapshot.snapshot_index = index;
        snapshot.status_value = observed.status_value;
        snapshot.label_id = multiplayer_status_label_id(observed.status_value);

        if (phase == SnapshotPhase::outer_state_dispatch &&
            observed.status_value >= 5 && observed.status_value <= 0x10) {
            snapshot.update_tick_called = true;
            if (observed.status_value == 0x0f) {
                snapshot.status_0x0f_virtual_plus_0x6c_called = true;
                snapshot.status_0x0f_virtual_plus_0x68_called = true;
                snapshot.status_0x0f_string_lookup_called = true;
                if (!observed.status_0x0f_text.has_value()) {
                    result.input_complete = false;
                } else {
                    const auto nul = observed.status_0x0f_text->find('\0');
                    const auto text_length = nul == std::string_view::npos
                        ? observed.status_0x0f_text->size()
                        : nul;
                    const auto copy_size = text_length + 1U;
                    snapshot.status_0x0f_copy_size_including_nul = copy_size;
                    if (observed.destination_capacity.has_value()) {
                        snapshot.destination_overrun_possible =
                            *observed.destination_capacity < copy_size;
                    } else {
                        snapshot.destination_overrun_possible = std::nullopt;
                        result.input_complete = false;
                    }
                }
                snapshot.status_0x0f_virtual_plus_0x50_called = true;
                // Race's ChatLine +0x50 is 0x4A2F10. At state 15 it calls
                // 0x4A3AC0, then stores PGS_SYNCHRONIZING (16) at this+4.
                snapshot.status_0x0f_transition_helper_0x4a3ac0_called = true;
                snapshot.status_after_virtual_plus_0x50 = 0x10;

                if (!observed.connection_index_0x18.has_value() ||
                    *observed.connection_index_0x18 < 0) {
                    snapshot.status_0x0f_send_input_complete = false;
                } else {
                    constexpr std::uint64_t record_stride = 0x1cU;
                    constexpr std::uint64_t required_row_bytes = 0x1aU;
                    const auto record_offset =
                        static_cast<std::uint64_t>(
                            *observed.connection_index_0x18) * record_stride;
                    if (record_offset > observed.connection_records_0x14.size() ||
                        observed.connection_records_0x14.size() -
                                static_cast<std::size_t>(record_offset) <
                            required_row_bytes) {
                        snapshot.status_0x0f_send_input_complete = false;
                    } else {
                        const auto row_offset = static_cast<std::size_t>(
                            record_offset);
                        snapshot.status_0x0f_connection_record_offset = row_offset;
                        const auto channel = observed.connection_records_0x14[
                            row_offset + 0x18U];
                        const auto flags = observed.connection_records_0x14[
                            row_offset + 0x19U];
                        const bool send_enabled = (flags & 1U) != 0U;
                        snapshot.status_0x0f_send_gate_open = send_enabled;
                        snapshot.status_0x0f_message_prefix[3] = 0x38U;
                        snapshot.status_0x0f_message_prefix[4] = 0x39U;
                        snapshot.status_0x0f_message_prefix[5] = 0x30U;
                        snapshot.status_0x0f_message_prefix[6] = 0U;
                        snapshot.status_0x0f_message_prefix[7] = 0U;
                        snapshot.status_0x0f_message_prefix_known_mask = 0xf8U;
                        if (send_enabled) {
                            snapshot.status_0x0f_sender_0x4a8be0_called = true;
                            snapshot.status_0x0f_message_prefix[0] = 1U;
                            snapshot.status_0x0f_message_prefix[2] = channel;
                            snapshot.status_0x0f_message_prefix_known_mask =
                                0xfdU;

                            if (!observed
                                     .status_0x0f_packet_allocation_succeeded
                                     .has_value()) {
                                snapshot.status_0x0f_packet_delivery_route =
                                    MultiplayerPacketDeliveryRoute::
                                        inputs_incomplete;
                                result.input_complete = false;
                            } else if (!*observed
                                            .status_0x0f_packet_allocation_succeeded) {
                                snapshot.status_0x0f_packet_delivery_route =
                                    MultiplayerPacketDeliveryRoute::
                                        allocation_failed;
                            } else if (
                                !observed
                                     .status_0x0f_pending_queue_head_0x24_present
                                     .has_value() ||
                                !observed
                                     .status_0x0f_sender_state_0x20_present
                                     .has_value() ||
                                (*observed
                                     .status_0x0f_sender_state_0x20_present &&
                                 (!observed
                                      .status_0x0f_sender_sequence_0x28
                                      .has_value() ||
                                  !observed
                                       .status_0x0f_pending_sequence_0x01
                                       .has_value()))) {
                                snapshot.status_0x0f_packet_delivery_route =
                                    MultiplayerPacketDeliveryRoute::
                                        inputs_incomplete;
                                result.input_complete = false;
                            } else {
                                bool sequence_window_open = true;
                                if (*observed
                                         .status_0x0f_sender_state_0x20_present) {
                                    const auto distance =
                                        static_cast<std::uint8_t>(
                                            *observed
                                                 .status_0x0f_sender_sequence_0x28 -
                                            *observed
                                                 .status_0x0f_pending_sequence_0x01);
                                    sequence_window_open = distance < 2U;
                                }
                                const bool dispatch_immediately =
                                    sequence_window_open &&
                                    !*observed
                                         .status_0x0f_pending_queue_head_0x24_present;
                                if (dispatch_immediately) {
                                    snapshot.status_0x0f_packet_delivery_route =
                                        MultiplayerPacketDeliveryRoute::
                                            immediate_dispatch;
                                    snapshot.status_0x0f_immediate_dispatch_helper_0x4a9c10_called =
                                        true;
                                } else {
                                    snapshot.status_0x0f_packet_delivery_route =
                                        MultiplayerPacketDeliveryRoute::
                                            pending_queue;
                                    snapshot.status_0x0f_pending_queue_helper_0x4a9a80_called =
                                        true;
                                }
                            }
                        }
                    }
                }
                if (!snapshot.status_0x0f_send_input_complete) {
                    result.input_complete = false;
                }
            }
            result.snapshots.push_back(snapshot);
            continue;
        }

        const bool communication_query_required =
            phase == SnapshotPhase::communication_ready_wait ||
            (phase == SnapshotPhase::outer_state_dispatch &&
             observed.communication_object_at_0x04_present);
        if (communication_query_required) {
            snapshot.communication_query_called = true;
            snapshot.communication_query_result_0x24 =
                observed.communication_query_result_0x24;
            if (!observed.communication_query_result_0x24.has_value()) {
                result.input_complete = false;
                result.snapshots.push_back(snapshot);
                return result;
            }
            if (!*observed.communication_query_result_0x24 &&
                observed.status_value > 4) {
                snapshot.update_tick_called = true;
                phase = SnapshotPhase::communication_ready_wait;
                result.snapshots.push_back(snapshot);
                continue;
            }
        }

        if (phase != SnapshotPhase::status_0x11_wait) {
            snapshot.waiting_text_path_reached = true;
        }
        if (phase != SnapshotPhase::status_0x11_wait &&
            observed.status_value == 0x11) {
            snapshot.update_tick_called = true;
            phase = SnapshotPhase::status_0x11_wait;
            result.snapshots.push_back(snapshot);
            continue;
        }

        if (phase == SnapshotPhase::status_0x11_wait &&
            observed.status_value == 0x11) {
            snapshot.update_tick_called = true;
            result.snapshots.push_back(snapshot);
            continue;
        }

        result.terminal_status_text_draw_called = true;
        result.terminal_thread_priority_set_to_0 = true;
        if (observed.status_value < 4) {
            result.completion_global_published = true;
            result.no_op_completion_helper_called = true;
            result.completion_global_cleared = true;
            result.return_low_byte = std::uint8_t{0};
            result.return_upper_24_bits_unknown = true;
        } else {
            const auto raw_status = static_cast<std::uint32_t>(
                observed.status_value);
            result.return_low_byte = std::uint8_t{1};
            result.return_upper_24_bits = raw_status & 0xffffff00U;
        }
        result.snapshots.push_back(snapshot);
        return result;
    }

    result.next_snapshot_unknown = true;
    return result;
}

MultiplayerStatusCallbackPlan plan_multiplayer_status_callback(
    const MultiplayerStatusCallbackObservation& observation)
{
    MultiplayerStatusCallbackPlan result{};
    if (!observation.status_object_global_present) {
        result.returned_low_byte.emplace(std::uint8_t{0});
        return result;
    }
    if (!observation.status_gate_value.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (*observation.status_gate_value <= 4) {
        result.returned_low_byte.emplace(std::uint8_t{0});
        return result;
    }

    if (!observation.callback_argument.has_value()) {
        result.input_complete = false;
        return result;
    }
    result.virtual_plus_0x84_called = true;
    result.status_label_lookup_called = true;
    result.status_label_id.emplace(
        multiplayer_status_label_id(*observation.callback_argument));
    result.no_op_helper_0x4a1af0_called =
        *observation.callback_argument >= 15;
    result.indexed_status_flag_byte_offset.emplace(
        0x1c8LL + static_cast<std::int64_t>(*observation.callback_argument) * 0xd9LL);
    const auto offset = *result.indexed_status_flag_byte_offset;
    if (offset < 0 ||
        static_cast<std::uint64_t>(offset) >=
            observation.chatline_object_bytes.size()) {
        result.input_complete = false;
        return result;
    }
    result.returned_low_byte.emplace(observation.chatline_object_bytes[
        static_cast<std::size_t>(offset)]);
    return result;
}

MultiplayerTelephonyShutdownPlan plan_multiplayer_telephony_shutdown(
    const MultiplayerTelephonyShutdownObservation& observation)
{
    constexpr std::uint32_t wait_object_0 = 0x00000000U;
    constexpr std::uint32_t wait_abandoned_0 = 0x00000080U;
    constexpr std::uint32_t wait_timeout = 0x00000102U;
    constexpr std::uint32_t wait_failed = 0xFFFFFFFFU;

    MultiplayerTelephonyShutdownPlan result{};
    const auto slot_count = std::min(
        observation.slots.size(),
        MultiplayerTelephonyShutdownPlan::expected_slot_count);
    result.input_complete = slot_count ==
        MultiplayerTelephonyShutdownPlan::expected_slot_count;
    result.slots.reserve(slot_count);
    std::size_t active_slot_count = 0;
    std::size_t primary_close_count = 0;
    std::size_t event_close_count = 0;

    for (std::size_t index = 0; index < slot_count; ++index) {
        const auto& observed = observation.slots[index];
        MultiplayerTelephonySlotShutdownPlan slot{};
        slot.slot_index = index;
        slot.wait_called = observed.primary_slot_value_active;
        slot.wait_used_missing_event_handle =
            slot.wait_called && !observed.completion_event_handle_present;
        slot.completion_event_closed = observed.completion_event_handle_present;

        if (!observed.primary_slot_value_active) {
            slot.primary_value_remains_after_wait = false;
            slot.primary_handle_closed = false;
        } else {
            ++active_slot_count;
            if (!observed.wait_result.has_value()) {
                result.input_complete = false;
                slot.primary_value_remains_after_wait = std::nullopt;
                slot.primary_handle_closed = std::nullopt;
            } else {
                slot.wait_result = *observed.wait_result;
                slot.abandoned_wait_warning =
                    *observed.wait_result == wait_abandoned_0;
                slot.wait_api_error = *observed.wait_result == wait_failed;
                slot.fatal_wait_result =
                    *observed.wait_result != wait_object_0 &&
                    *observed.wait_result != wait_abandoned_0 &&
                    *observed.wait_result != wait_timeout &&
                    *observed.wait_result != wait_failed;
                const bool primary_remains =
                    *observed.wait_result == wait_timeout;
                slot.primary_value_remains_after_wait = primary_remains;
                slot.pending_data_warning = primary_remains;
                slot.primary_handle_closed = primary_remains;
            }
        }

        if (slot.primary_handle_closed.value_or(false)) {
            ++primary_close_count;
        }
        if (slot.completion_event_closed.value_or(false)) {
            ++event_close_count;
        }
        result.slots.push_back(slot);
    }

    if (result.input_complete) {
        result.active_counter_after_quiesce =
            0x10 - static_cast<std::int32_t>(active_slot_count);
        result.primary_handle_close_count = primary_close_count;
        result.completion_event_close_count = event_close_count;
    }
    result.line_shutdown_called = true;
    result.line_handle_cleared = true;
    result.initialized_flag_cleared = true;
    return result;
}

CareerFrontendPlan plan_career_frontend(
    CareerFrontendGlobals& globals,
    const std::span<const int> menu_results) noexcept
{
    CareerFrontendPlan plan{};
    globals.saved_value_0x4d10f8 = globals.current_value_0x4d10fc;
    globals.current_value_0x4d10fc = 0;

    for (const int selection : menu_results) {
        ++plan.menu_poll_call_count;
        if (selection < 0) {
            plan.menu_exit_observed = true;
            globals.current_value_0x4d10fc = globals.saved_value_0x4d10f8;
            plan.globals_restored = true;
            return plan;
        }
        ++plan.selected_action_call_count;
    }

    ++plan.menu_poll_call_count;
    plan.next_menu_result_unknown = true;
    return plan;
}

CareerTestingRacePlan plan_career_testing_race(
    const CareerTestingRaceObservation& observation)
{
    CareerTestingRacePlan plan{};
    constexpr std::size_t track_local_capacity = 36;
    plan.track_copy_size_including_nul = observation.selected_track.size() + 1;
    plan.track_local_overrun_possible =
        plan.track_copy_size_including_nul > track_local_capacity;
    plan.reversed_option = observation.reversed_option;

    if (observation.career_payload.size() < 0x128) {
        plan.input_complete = false;
    } else {
        const auto read_u32 = [&](const std::size_t offset) {
            return static_cast<std::uint32_t>(observation.career_payload[offset]) |
                (static_cast<std::uint32_t>(observation.career_payload[offset + 1]) << 8U) |
                (static_cast<std::uint32_t>(observation.career_payload[offset + 2]) << 16U) |
                (static_cast<std::uint32_t>(observation.career_payload[offset + 3]) << 24U);
        };
        plan.career_dword_0x10.emplace(read_u32(0x10));
        plan.career_byte_0x14.emplace(observation.career_payload[0x14]);
        plan.career_dword_0x124.emplace(read_u32(0x124));
        plan.career_dword_0x124_plus_four.emplace(read_u32(0x124) + 4U);
        plan.career_dword_0x124_plus_three.emplace(read_u32(0x124) + 3U);
    }

    for (const int result : observation.race_menu_results) {
        ++plan.menu_interaction_count;
        if (result == 1) {
            plan.race_setup_modes.push_back(0);
        } else if (result == 2) {
            plan.race_setup_modes.push_back(1);
        } else {
            plan.loop_exit_observed = true;
            plan.race_context_cleanup_called = true;
            plan.season_reload_called = true;
            return plan;
        }
    }

    plan.next_menu_result_unknown = true;
    plan.input_complete = false;
    return plan;
}

} // namespace viper
