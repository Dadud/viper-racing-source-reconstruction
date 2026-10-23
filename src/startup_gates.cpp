#include "viper/startup_gates.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace viper {

SingleInstanceGateResult evaluate_single_instance_gate(
    const SingleInstanceGateObservation& observation) noexcept
{
    SingleInstanceGateResult result{};
    result.panic_helper_reached = !observation.semaphore_created;
    if (observation.last_error == startup_error_already_exists) {
        result.window_lookup_reached = true;
        result.show_window_reached = observation.matching_window_found;
        result.show_command = result.show_window_reached ? 9U : 0U;
        result.close_handle_reached = true;
        result.return_value = observation.close_handle_result & 0xffffff00U;
    } else {
        result.return_value = (observation.last_error & 0xffffff00U) | 1U;
    }
    result.continue_startup = (result.return_value & 0xffU) != 0;
    return result;
}

CanaryGateResult evaluate_canary_gate(
    const CanaryGateObservation& observation) noexcept
{
    CanaryGateResult result{};
    result.get_last_error_reached = observation.semaphore_created;
    if (observation.semaphore_created &&
        observation.last_error == startup_error_already_exists) {
        result.return_value = 1U;
    } else {
        result.close_handle_reached = observation.semaphore_created;
        result.warning_message_reached = true;
        result.message_box_flags = 0x30U;
        result.return_value = observation.message_box_result & 0xffffff00U;
    }
    result.continue_startup = (result.return_value & 0xffU) != 0;
    return result;
}

LoadingWindowPlan plan_loading_window(
    const LoadingWindowObservation& observation) noexcept
{
    LoadingWindowPlan result{};
    result.window_class_style = 0;
    result.class_extra_bytes = 0;
    result.window_extra_bytes = 0;
    result.cursor_resource_id = 0x7f00U;
    result.background_resource_id = 6U;
    result.extended_window_style = 0x80U;
    result.window_style = 0x80800000U;
    result.width = 300;
    result.height = 0x175;
    result.x = (observation.screen_width - result.width) / 2;
    result.y = (observation.screen_height - result.height) / 2;
    result.create_window_reached = observation.class_registration_succeeded;
    result.show_window_reached = result.create_window_reached &&
        observation.window_creation_succeeded;
    result.update_window_reached = result.show_window_reached;
    result.show_command = result.show_window_reached ? 5U : 0U;
    return result;
}

ClockBackendResult select_clock_backend(
    const ClockBackendObservation& observation) noexcept
{
    ClockBackendResult result{};
    result.cpuid_tsc_gate_passed = observation.eflags_id_bit_mutable &&
        observation.cpuid_leaf_1_available && observation.leaf_1_tsc_feature;
    result.calibrated_tsc_path_selected = result.cpuid_tsc_gate_passed &&
        observation.logical_processor_count == 1;
    result.high_resolution_flag_0x4ef320 =
        result.calibrated_tsc_path_selected;
    result.fallback_clock_callbacks_installed =
        !result.calibrated_tsc_path_selected;
    result.performance_frequency_queried = true;
    result.counter_anchor_0x4ef318 =
        observation.prior_fallback_counter_anchor;

    if (result.calibrated_tsc_path_selected) {
        result.tsc_origin_0x4d2370 = observation.initial_tsc_counter;
        result.returns_success = observation.performance_frequency_succeeded;
        result.performance_counter_queried =
            observation.performance_frequency_succeeded;
        if (observation.performance_frequency_succeeded) {
            result.tsc_calibration_reached = true;
            result.tsc_calibration = calibrate_tsc_clock(
                observation.performance_frequency_value,
                observation.tsc_calibration);
        }
    } else {
        result.callback_0x4d235c = 0x004135f0U;
        result.callback_0x4d2350 = 0x004135e0U;
        result.callback_0x4d234c = 0x004135e0U;
        result.callback_0x4d2354 = 0x004135e0U;
        result.callback_0x4d2360 = 0x00413600U;
        result.callback_0x4d2358 = 0x004135e0U;
        if (observation.performance_frequency_succeeded) {
            result.frequency_per_millisecond_0x4ef310 =
                static_cast<std::uint32_t>(
                    observation.performance_frequency_value) / 1000U;
            result.performance_counter_queried = true;
            result.counter_anchor_0x4ef318 =
                observation.performance_counter_value;
        }
        // Both fallback return paths force AL to one.
        result.returns_success = true;
    }
    return result;
}

TscCalibrationResult calibrate_tsc_clock(
    const std::uint64_t performance_frequency,
    const TscCalibrationObservation& observation) noexcept
{
    TscCalibrationResult result{};
    result.tsc_delta_low = observation.tsc_end_low - observation.tsc_start_low;
    if (performance_frequency == 0) {
        return result;
    }

    const long double overshoot_ratio =
        static_cast<long double>(observation.performance_counter_overshoot_low) /
        static_cast<long double>(performance_frequency);
    const long double estimated_rate =
        static_cast<long double>(result.tsc_delta_low) / (1.0L + overshoot_ratio);
    if (!std::isfinite(estimated_rate) || estimated_rate < 0.0L ||
        estimated_rate > static_cast<long double>(
            std::numeric_limits<std::int64_t>::max())) {
        return result;
    }

    // __ftol sets x87 rounding control to truncate toward zero before FISTP.
    result.estimated_cycles_per_second =
        static_cast<std::int64_t>(estimated_rate);
    result.cycles_per_millisecond = result.estimated_cycles_per_second / 1000;
    result.global_0x4d2364 =
        static_cast<std::uint32_t>(result.cycles_per_millisecond);
    if (result.global_0x4d2364 == 0) {
        return result;
    }
    result.global_0x4d2368 = 0xffffffffU / result.global_0x4d2364;
    result.valid = true;
    return result;
}

StartupInitializationResult plan_startup_initialization(
    const StartupInitializationObservation& observation,
    CriticalSectionPoolState& critical_section_pool,
    NamedCriticalSectionRegistryState& named_critical_sections,
    StartupFileRegistryState& startup_files)
{
    StartupInitializationResult result{};
    const auto call = [&result](const std::uint32_t address) {
        if (result.call_count < result.call_addresses.size()) {
            result.call_addresses[result.call_count++] = address;
        }
    };
    const auto panic = [&]() {
        call(0x411000U);
        result.panic_helper_reached = true;
        ++result.panic_call_count;
    };
    const auto run_cleanup = [&]() {
        call(0x411280U);
        call(0x416890U);
        call(0x4149f0U);
        call(0x414fa0U);
        call(0x4165cfU);
        result.cleanup_thunk_reached = true;
    };
    const auto record_named_lock_panics = [&result](
        const NamedCriticalSectionRegistrationResult& registration) {
        result.panic_call_count += registration.duplicate_name_panic_reached ? 1U : 0U;
        result.panic_call_count += registration.registry_full_panic_reached ? 1U : 0U;
        result.panic_call_count +=
            registration.critical_section_allocation_panic_reached ? 1U : 0U;
        result.panic_helper_reached = result.panic_helper_reached ||
            registration.duplicate_name_panic_reached ||
            registration.registry_full_panic_reached ||
            registration.critical_section_allocation_panic_reached;
    };

    if (observation.already_initialized_0x4d2fbc) {
        panic();
        result.return_value = observation.panic_return_value & 0xffffff00U;
    } else {
        call(0x413610U);
        const auto clock_backend = select_clock_backend(observation.clock_backend);
        if (!clock_backend.returns_success) {
            panic();
            result.return_value = observation.panic_return_value & 0xffffff00U;
        } else {
            call(0x414f80U);
            result.critical_section_gate_reached = true;
            result.critical_section_allocation =
                allocate_critical_section(critical_section_pool);
            result.global_critical_section_pointer_0x4d278c =
                result.critical_section_allocation.storage_address;
            if (result.critical_section_allocation.panic_helper_reached) {
                result.panic_helper_reached = true;
                ++result.panic_call_count;
            }
            if (!result.critical_section_allocation.allocated) {
                panic();
                call(0x4165cfU);
                result.cleanup_thunk_reached = true;
                result.return_value =
                    observation.cleanup_thunk_return_value & 0xffffff00U;
            } else {
                call(0x414840U);
                result.task_timer_lock = register_named_critical_section(
                    "task", named_critical_sections, critical_section_pool);
                record_named_lock_panics(result.task_timer_lock);
                result.timer_probe.timer_thread_create_reached = true;
                result.timer_probe.timer_thread_creation_succeeded =
                    observation.timer_worker_thread_creation_succeeded;
                result.timer_probe.thread_creation_failure_log_reached =
                    observation.timer_worker_thread_creation_succeeded.has_value() &&
                    !*observation.timer_worker_thread_creation_succeeded;
                result.timer_probe.completion_flag_cleared_before_start = true;
                result.timer_probe.sleep_count = observation.timer_callback_signaled
                    ? std::min(observation.timer_polls_before_signal,
                               result.timer_probe.maximum_sleep_count)
                    : result.timer_probe.maximum_sleep_count;
                result.timer_probe.panic_message_reached =
                    !observation.timer_callback_signaled ||
                    observation.timer_polls_before_signal >
                        result.timer_probe.maximum_sleep_count;
                result.timer_probe.timer_stopped = true;
                result.timer_probe.completion_flag_cleared_after_stop = true;
                if (result.timer_probe.panic_message_reached) {
                    result.panic_helper_reached = true;
                    ++result.panic_call_count;
                }
                call(0x4111f0U);
                result.file_registry_lock = register_named_critical_section(
                    "File", named_critical_sections, critical_section_pool);
                record_named_lock_panics(result.file_registry_lock);
                result.file_registry_initialization.reached = true;
                result.file_registry_initialization.named_lock_registration_reached = true;
                result.file_registry_initialization.records_initialized =
                    startup_file_registry_capacity;
                for (auto& record : startup_files.records) {
                    record.path.clear();
                    record.handle = invalid_vxd_handle;
                }
                startup_files.allocated_count = 0;
                call(0x410aa0U);
                result.logger_lock = register_named_critical_section(
                    "Log", named_critical_sections, critical_section_pool);
                result.logger_output_lock = register_named_critical_section(
                    "Mono", named_critical_sections, critical_section_pool);
                record_named_lock_panics(result.logger_lock);
                record_named_lock_panics(result.logger_output_lock);
                result.memory_subsystem_reached = true;
                result.memory_subsystem_succeeded = true;
                call(0x416680U);
                result.background_task.reached = true;
                result.background_task.lock_registration =
                    register_named_critical_section(
                        "BG", named_critical_sections, critical_section_pool);
                record_named_lock_panics(result.background_task.lock_registration);
                result.background_task.thread_creation_succeeded =
                    observation.background_worker_thread_creation_succeeded;
                result.background_task.thread_creation_failure_log_reached =
                    observation.background_worker_thread_creation_succeeded.has_value() &&
                    !*observation.background_worker_thread_creation_succeeded;
                result.background_task.task_one_based_index =
                    observation.background_task_one_based_index;
                call(0x4154a0U);
                result.exception_handler.filter_install_reached = true;
                result.exception_handler.floating_point_control_reached = true;
                result.exception_handler.prior_control_word =
                    observation.exception_handler.prior_control_word;
                if (observation.exception_handler.prior_control_word.has_value()) {
                    result.exception_handler.requested_control_word =
                        *observation.exception_handler.prior_control_word |
                        result.exception_handler.exception_mask_bits;
                }
                result.executable_overlay = map_executable_overlay(
                    observation.executable_overlay);
                result.exception_log.create_file_reached = true;
                result.exception_log.returned_handle =
                    observation.exception_log_file_handle;
                if (observation.exception_log_file_handle.has_value()) {
                    if (*observation.exception_log_file_handle != invalid_vxd_handle) {
                        result.exception_log.registration = register_startup_file_handle(
                            result.exception_log.path,
                            *observation.exception_log_file_handle,
                            startup_files);
                        result.exception_log_registration =
                            result.exception_log.registration;
                        if (!result.exception_log.registration.panic_helper_reached) {
                            result.exception_log.one_based_file_index =
                                result.exception_log.registration.one_based_index;
                        }
                    } else {
                        result.exception_log.one_based_file_index = 0U;
                    }
                    if (result.exception_log.registration.panic_helper_reached) {
                        result.panic_helper_reached = true;
                        ++result.panic_call_count;
                    }
                }
                call(0x414110U);
                result.heap_create_reached = true;
                result.heap_create_flags = 1U;
                result.heap_initial_size = 0x01400000U;
                result.heap_maximum_size = 0U;
                if (observation.heap_creation_0x414110_succeeded) {
                    result.secondary_allocator_initialized = true;
                    result.heap_counters_zeroed = true;
                    result.heap_allocator_lock = register_named_critical_section(
                        "Mem", named_critical_sections, critical_section_pool);
                    record_named_lock_panics(result.heap_allocator_lock);
                    result.vxd_allocator_status_checked = true;
                    result.vxd_allocator_message_reached =
                        observation.vxd_allocator_active_0x4161c0;
                    call(0x412430U);
                    call(0x413c80U);
                    call(0x412f90U);
                    call(0x4144c0U);
                    call(0x4168f0U);
                    result.initialized_flag_set_0x4d2fbc = true;
                    result.return_value =
                        (observation.final_initializer_return_value &
                         0xffffff00U) | 1U;
                } else {
                    panic();
                    call(0x410dd0U);
                    result.heap_failure_cleanup_reached = true;
                    run_cleanup();
                    result.return_value =
                        observation.cleanup_thunk_return_value & 0xffffff00U;
                }
            }
        }
    }

    result.success_low_byte = (result.return_value & 0xffU) != 0;
    result.named_lock_registry_after = named_critical_sections;
    return result;
}

StartupShutdownResult plan_startup_shutdown(
    bool& initialized_flag_0x4d2fbc,
    const StartupShutdownObservation& observation) noexcept
{
    StartupShutdownResult result{};
    const auto call = [&result](const std::uint32_t address) {
        if (result.call_count < result.call_addresses.size()) {
            result.call_addresses[result.call_count++] = address;
        }
    };

    if (!initialized_flag_0x4d2fbc) {
        call(0x411000U);
        result.panic_helper_reached = true;
        result.return_value = observation.panic_return_value;
        return result;
    }

    result.cleanup_sequence_reached = true;
    call(0x416c70U);
    call(0x4144e0U);
    call(0x413080U);
    call(0x413cc0U);
    call(0x412be0U);
    call(0x414430U);
    call(0x415c20U);
    call(0x411230U);
    call(0x416890U);
    call(0x410dd0U);
    call(0x411280U);
    call(0x4149f0U);
    call(0x414fa0U);
    call(0x413bc0U); // thunk to 0x4165CF
    result.cleanup_thunk_reached = true;
    result.return_value = observation.cleanup_thunk_return_value;
    initialized_flag_0x4d2fbc = false;
    result.initialized_flag_cleared_0x4d2fbc = true;
    return result;
}

HandleFinalizationResult close_vxd_file_handle(
    std::uint32_t& handle_global_0x4d2f20) noexcept
{
    HandleFinalizationResult result{};
    result.handle_value = handle_global_0x4d2f20;
    result.close_handle_reached =
        handle_global_0x4d2f20 != invalid_vxd_handle;
    if (result.close_handle_reached) {
        handle_global_0x4d2f20 = invalid_vxd_handle;
        result.global_reset_to_invalid = true;
    }
    result.global_value_after = handle_global_0x4d2f20;
    return result;
}

HandleFinalizationResult close_startup_semaphore_handle(
    const std::uint32_t handle_global_0x4d1cb8) noexcept
{
    HandleFinalizationResult result{};
    result.handle_value = handle_global_0x4d1cb8;
    result.global_value_after = handle_global_0x4d1cb8;
    result.close_handle_reached = true;
    return result;
}

VxdOpenResult plan_vxd_open(const std::uint32_t create_file_handle) noexcept
{
    VxdOpenResult result{};
    result.handle_value = create_file_handle;
    result.import_patch_scan_reached = create_file_handle != invalid_vxd_handle;
    result.returns_success = result.import_patch_scan_reached;
    return result;
}

ApplicationStartupHandoffResult plan_application_startup_handoff(
    const ApplicationStartupHandoffObservation& observation,
    CriticalSectionPoolState& critical_section_pool)
{
    ApplicationStartupHandoffResult result{};
    result.global_startup_flag_0x4d1cb4_cleared = true;
    result.global_module_handle_0x4d1ca8 = observation.module_handle;
    result.global_command_line_0x4d1cac = observation.command_line;
    result.named_critical_sections = observation.named_critical_sections;
    result.startup_files = observation.startup_files;
    const auto call = [&result](const std::uint32_t address) {
        if (result.call_count < result.call_addresses.size())
            result.call_addresses[result.call_count++] = address;
    };

    const auto dedicated = observation.command_line.find("-dedicated");
    result.dedicated_mode_0x4d1cbc = dedicated != std::string_view::npos;
    if (result.dedicated_mode_0x4d1cbc &&
        dedicated + 10 < observation.command_line.size() &&
        observation.command_line[dedicated + 10] == ':') {
        const std::string port_text(observation.command_line.substr(dedicated + 11));
        result.dedicated_port = static_cast<std::int16_t>(std::atoi(port_text.c_str()));
    }

    call(0x4120a0U);
    result.initial_gate_reached = true;
    const auto initial_gate = evaluate_single_instance_gate(observation.single_instance);
    if (initial_gate.continue_startup) {
        call(0x412050U);
        result.canary_gate_reached = true;
        const auto canary_gate = evaluate_canary_gate(observation.canary);
        if (canary_gate.continue_startup) {
            if (!result.dedicated_mode_0x4d1cbc) {
                call(0x412270U);
                result.loading_window_setup_reached = true;
                (void)plan_loading_window(observation.loading_window);
                call(0x416080U);
                result.vxd_open = plan_vxd_open(observation.vxd_handle);
                call(0x416360U);
                result.initialization = plan_startup_initialization(
                    observation.initialization, critical_section_pool,
                    result.named_critical_sections, result.startup_files);
                if (result.initialization.success_low_byte) {
                    call(0x412120U);
                    result.config_directory_reached = true;
                    result.config_directory = plan_config_directory(
                        observation.module_path,
                        observation.get_module_file_name_succeeded);
                    result.config_directory_creation =
                        evaluate_config_directory_creation(
                            result.config_directory,
                            observation.config_directory_create_observations);
                    result.process_exit_reached =
                        result.config_directory.exit_process_reached;
                    result.long_config_directory_warning_reached =
                        result.config_directory.returns_success &&
                        result.config_directory.config_path_0x4ef0c0.size() > 200;
                    if (result.config_directory.returns_success) {
                        call(0x401280U);
                        result.command_line_parser_reached = true;
                        result.command_line_parse = parse_startup_command_line(
                            std::string(observation.command_line),
                            observation.initial_command_state,
                            observation.command_line_observation);
                        if (result.command_line_parse.returns_success) {
                            if (observation.loading_window_handle_nonzero &&
                                observation.loading_window_is_window) {
                                result.loading_window_destroyed = true;
                            }
                            call(0x401460U);
                            result.frontend_reached = true;
                        }
                    }
                    if (!result.process_exit_reached) {
                        call(0x416450U);
                        result.shutdown_reached = true;
                        bool initialized = result.initialization.initialized_flag_set_0x4d2fbc;
                        result.shutdown = plan_startup_shutdown(initialized, observation.shutdown);
                    }
                }
                if (!result.process_exit_reached) {
                    call(0x4161a0U);
                    result.vxd_finalizer_reached = true;
                    auto vxd_handle = observation.vxd_handle;
                    result.vxd_finalization = close_vxd_file_handle(vxd_handle);
                }
            } else {
                call(0x416360U);
                result.initialization = plan_startup_initialization(
                    observation.initialization, critical_section_pool,
                    result.named_critical_sections, result.startup_files);
                if (result.initialization.success_low_byte) {
                    call(0x49d130U);
                    result.dedicated_handler_reached = true;
                    call(0x416450U);
                    result.shutdown_reached = true;
                    bool initialized = result.initialization.initialized_flag_set_0x4d2fbc;
                    result.shutdown = plan_startup_shutdown(initialized, observation.shutdown);
                }
            }
        }
        if (!result.process_exit_reached) {
            call(0x412110U);
            result.semaphore_finalizer_reached = true;
            result.semaphore_finalization =
                close_startup_semaphore_handle(observation.semaphore_handle);
        }
    }
    // 0x411F10 returns zero regardless of which startup gate or branch ran.
    result.return_value = 0;
    return result;
}

CriticalSectionPoolAllocation allocate_critical_section(
    CriticalSectionPoolState& state) noexcept
{
    CriticalSectionPoolAllocation result{};
    std::size_t slot = critical_section_pool_capacity;
    if (state.allocated_count < critical_section_pool_capacity) {
        slot = state.allocated_count;
        ++state.allocated_count;
        state.slot_used[slot] = true;
    } else {
        for (std::size_t index = 0;
             index < state.allocated_count &&
                 index < critical_section_pool_capacity;
             ++index) {
            if (!state.slot_used[index]) {
                slot = index;
                state.slot_used[index] = true;
                break;
            }
        }
    }

    if (slot == critical_section_pool_capacity) {
        result.panic_helper_reached = true;
        return result;
    }

    result.allocated = true;
    result.initialize_call_reached = true;
    result.slot_index = slot;
    result.storage_address = critical_section_pool_base +
        static_cast<std::uint32_t>(slot) * critical_section_pool_stride;
    return result;
}

NamedCriticalSectionRegistrationResult register_named_critical_section(
    const std::string_view name,
    NamedCriticalSectionRegistryState& registry,
    CriticalSectionPoolState& pool)
{
    NamedCriticalSectionRegistrationResult result{};
    result.manager_lock_entered = true;

    const auto equal_case_insensitive = [](const std::string_view left,
                                           const std::string_view right) {
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            char a = left[index];
            char b = right[index];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b) return false;
        }
        return true;
    };

    const auto searchable_count = std::min(
        registry.registered_count, named_critical_section_capacity);
    for (std::size_t index = 0; index < searchable_count; ++index) {
        const auto& record = registry.records[index];
        if (record.pointer_token != 0 &&
            equal_case_insensitive(record.name, name)) {
            result.duplicate_name_panic_reached = true;
            break;
        }
    }

    if (registry.registered_count < named_critical_section_capacity) {
        result.registry_index = registry.registered_count++;
    } else {
        for (std::size_t index = 0; index < searchable_count; ++index) {
            if (registry.records[index].pointer_token == 0) {
                result.registry_index = index;
                result.slot_reused = true;
                break;
            }
        }
        if (result.registry_index == named_critical_section_capacity) {
            result.registry_full_panic_reached = true;
        }
    }

    result.critical_section_allocation_reached = true;
    result.allocation = allocate_critical_section(pool);
    result.critical_section_allocation_panic_reached =
        result.allocation.panic_helper_reached;
    if (result.registry_index == named_critical_section_capacity) {
        // The binary continues after its panic and computes index -1; surface
        // the invalid store attempt without performing an out-of-bounds write.
        result.invalid_registry_index_store_reached = true;
    } else {
        auto& record = registry.records[result.registry_index];
        record.name.assign(name);
        record.pointer_token = result.allocation.allocated
            ? result.allocation.storage_address
            : 0U;
        result.returned_one_based_index =
            static_cast<std::uint32_t>(result.registry_index + 1U);
    }
    result.manager_lock_left = true;
    return result;
}

StartupFileHandleRegistrationResult register_startup_file_handle(
    const std::string_view path,
    const std::uint32_t handle,
    StartupFileRegistryState& registry)
{
    StartupFileHandleRegistrationResult result{};
    std::size_t index = startup_file_registry_capacity;
    if (registry.allocated_count < startup_file_registry_capacity) {
        index = registry.allocated_count++;
    } else {
        for (std::size_t candidate = 0;
             candidate < startup_file_registry_capacity;
             ++candidate) {
            if (registry.records[candidate].handle == invalid_vxd_handle) {
                index = candidate;
                result.reused_slot = true;
                break;
            }
        }
    }

    if (index == startup_file_registry_capacity) {
        result.panic_helper_reached = true;
        return result;
    }
    registry.records[index].path.assign(path);
    registry.records[index].handle = handle;
    result.slot_index = index;
    result.one_based_index = static_cast<std::uint32_t>(index + 1U);
    return result;
}

ExecutableOverlayMappingResult map_executable_overlay(
    const ExecutableOverlayMappingObservation& observation) noexcept
{
    ExecutableOverlayMappingResult result{};
    result.module_path_query_reached = true;
    if (!observation.module_path_query_succeeded) {
        result.error_log_reached = true;
        return result;
    }

    result.executable_open_reached = true;
    if (!observation.executable_open_succeeded) {
        result.error_log_reached = true;
        return result;
    }

    result.file_size_query_reached = true;
    result.mapping_creation_reached = true;
    if (!observation.mapping_creation_succeeded) {
        result.error_log_reached = true;
        result.file_close_reached = true;
        return result;
    }

    result.view_mapping_reached = true;
    if (!observation.view_mapping_succeeded) {
        result.error_log_reached = true;
        result.mapping_close_reached = true;
        result.file_close_reached = true;
        return result;
    }

    for (const auto& section : observation.sections) {
        result.maximum_raw_end = std::max(
            result.maximum_raw_end, section.raw_offset + section.raw_size);
    }
    result.overlay_size = observation.file_size - result.maximum_raw_end;
    if (result.overlay_size != 0U) {
        result.overlay_pointer_token =
            observation.mapped_view_token + result.maximum_raw_end;
        result.returns_overlay = true;
        result.mapping_retained = true;
        result.view_retained = true;
        return result;
    }

    result.no_overlay_log_reached = true;
    result.view_unmap_reached = true;
    result.mapping_close_reached = true;
    result.file_close_reached = true;
    return result;
}

} // namespace viper
