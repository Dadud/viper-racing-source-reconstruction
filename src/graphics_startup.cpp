#include "viper/graphics_startup.hpp"

#include <bit>

namespace viper {

DirectDrawVideoMemoryPlan plan_directdraw_video_memory_class(
    const DirectDrawVideoMemoryObservation& observation) noexcept
{
    constexpr std::uint32_t adjustment = 0x96000U;
    constexpr std::uint32_t two_megabytes = 2'000'000U;
    constexpr std::uint32_t four_megabytes = 4'000'000U;
    constexpr std::uint32_t eight_megabytes = 8'000'000U;
    constexpr std::uint32_t sixteen_megabytes = 16'000'000U;

    DirectDrawVideoMemoryPlan plan{};
    plan.adjusted_video_memory_total =
        observation.reported_video_memory_total + adjustment;
    const auto adjusted = plan.adjusted_video_memory_total;
    if (adjusted < two_megabytes) {
        plan.initial_memory_class = 0U;
    } else if (adjusted < four_megabytes) {
        plan.initial_memory_class = 2U;
    } else if (adjusted < eight_megabytes) {
        plan.initial_memory_class = 4U;
    } else if (adjusted < sixteen_megabytes) {
        plan.initial_memory_class = 8U;
    } else {
        plan.initial_memory_class = 16U;
    }

    plan.final_memory_class = plan.initial_memory_class;
    plan.low_texture_memory_warning = plan.final_memory_class > 2U &&
        observation.reported_texture_memory_total < two_megabytes;
    plan.agp_texture_memory_present =
        observation.reported_agp_texture_memory_total != 0U;
    if (plan.agp_texture_memory_present) {
        if (plan.final_memory_class == 2U &&
            observation.reported_agp_texture_memory_total > two_megabytes) {
            plan.final_memory_class = 4U;
        } else if (plan.final_memory_class == 4U &&
                   observation.reported_agp_texture_memory_total > four_megabytes) {
            plan.final_memory_class = 8U;
        } else if (plan.final_memory_class == 8U &&
                   observation.reported_agp_texture_memory_total > eight_megabytes) {
            plan.final_memory_class = 16U;
        }
    }

    switch (plan.final_memory_class) {
    case 2U:
        plan.resolution_flags_cleared[2] = true;
        [[fallthrough]];
    case 4U:
        plan.resolution_flags_cleared[3] = true;
        [[fallthrough]];
    case 8U:
        plan.resolution_flags_cleared[4] = true;
        [[fallthrough]];
    case 16U:
        plan.supported_memory_class = true;
        break;
    default:
        plan.process_exit_code = 5U;
        break;
    }
    return plan;
}

DirectDrawStartupPlan plan_directdraw_startup(
    const DirectDrawStartupObservation& observation)
{
    DirectDrawStartupPlan plan;
    plan.initial_state_cleared = true;
    plan.calls = {0x44E080U, 0x44E460U, 0x44E560U};

    bool device_selected = false;
    if (!observation.display_enumeration_already_done) {
        plan.display_enumeration_called = true;
        if (!observation.device_selected_after_enumeration) {
            plan.input_complete = false;
            return plan;
        }
        device_selected = *observation.device_selected_after_enumeration;
    }

    if (!device_selected) {
        plan.fallback_device_selection_called = true;
        plan.calls.push_back(0x44E4D0U);
        plan.calls.push_back(0x458470U);
        if (!observation.fallback_draw_creation_result) {
            plan.input_complete = false;
            return plan;
        }
        if (*observation.fallback_draw_creation_result ==
            FallbackDrawCreationResult::directdraw_create_failed) {
            plan.calls.push_back(0x410EA0U);
            plan.directdraw_create_error_logged = true;
            plan.exit = DirectDrawStartupExit::directdraw_create_exit_2;
            plan.process_exit_code = 2U;
            return plan;
        }
        if (*observation.fallback_draw_creation_result ==
            FallbackDrawCreationResult::query_interface_failed) {
            plan.base_directdraw_interface_released = true;
            plan.calls.push_back(0x452770U);
            plan.directdraw2_query_error_logged = true;
            plan.exit = DirectDrawStartupExit::directdraw_query_exit_2;
            plan.process_exit_code = 2U;
            return plan;
        }
        plan.base_directdraw_interface_released = true;
        plan.directdraw2_interface_acquired = true;
        plan.wrapper_allocation_attempted = true;
        if (*observation.fallback_draw_creation_result ==
            FallbackDrawCreationResult::created) {
            plan.fallback_draw_created = true;
            plan.wrapper_object_allocated = true;
            plan.calls.push_back(0x458C80U);
            plan.calls.push_back(0x458CB0U);
            plan.fallback_capability_checked = true;
            if (!observation.fallback_capability_bit_zero_set) {
                plan.input_complete = false;
                return plan;
            }
            device_selected = *observation.fallback_capability_bit_zero_set;
            if (device_selected) {
                plan.selected_fallback_object_stored = true;
            } else {
                plan.calls.push_back(0x458540U);
                plan.rejected_fallback_object_released = true;
            }
        } else {
            plan.directdraw2_interface_release_missing_on_allocation_failure = true;
        }
    }
    if (!device_selected) {
        plan.no_device_message_logged = true;
        plan.calls.push_back(0x410EA0U);
        plan.exit = DirectDrawStartupExit::no_device_exit_4;
        plan.process_exit_code = 4U;
        return plan;
    }

    plan.required_mode_enumerated = true;
    plan.calls.push_back(0x44EEB0U);
    plan.recognized_resolution_flags.assign(5U, false);
    plan.enum_modes_called = true;
    plan.calls.push_back(0x458D60U);
    if (!observation.mode_enumeration_call_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.mode_enumeration_call_succeeded) {
        plan.mode_enumeration_error_logged = true;
        plan.calls.push_back(0x410EA0U);
    }
    constexpr DisplayModeObservation supported_modes[] = {
        {320U, 200U}, {512U, 384U}, {640U, 480U}, {800U, 600U}, {1024U, 768U},
    };
    if (*observation.mode_enumeration_call_succeeded) {
        for (const auto& mode : observation.enumerated_display_modes) {
            DisplayModeCallbackResult callback;
            callback.mode = mode;
            for (std::uint32_t index = 0; index < 5U; ++index) {
                if (mode.width == supported_modes[index].width &&
                    mode.height == supported_modes[index].height) {
                    callback.matched_resolution_flag_index = index;
                    callback.callback_returned_continue = true;
                    plan.recognized_resolution_flags[index] = true;
                    break;
                }
            }
            plan.calls.push_back(0x44EE00U);
            plan.display_mode_callbacks.push_back(callback);
            if (!callback.callback_returned_continue) break;
        }
    }
    plan.mode_640x480_supported = plan.recognized_resolution_flags[2];
    if (!plan.mode_640x480_supported) {
        plan.required_mode_message_logged = true;
        plan.calls.push_back(0x410EA0U);
        plan.exit = DirectDrawStartupExit::required_mode_exit_5;
        plan.process_exit_code = 5U;
        return plan;
    }

    plan.exclusive_mode_attempted = true;
    plan.calls.push_back(0x458D20U);
    if (!observation.exclusive_mode_call_succeeded) {
        plan.input_complete = false;
        return plan;
    }
    if (!*observation.exclusive_mode_call_succeeded) {
        plan.exclusive_mode_message_logged = true;
        plan.calls.push_back(0x410EA0U);
        plan.exit = DirectDrawStartupExit::exclusive_mode_exit_2;
        plan.process_exit_code = 2U;
        return plan;
    }

    plan.success_state_cleared = true;
    plan.initialized_flag_set = true;
    plan.exit = DirectDrawStartupExit::initialized;
    return plan;
}

SurfaceLockPlan plan_surface_lock(
    const SurfaceLockObservation& observation) noexcept
{
    constexpr std::int32_t dd_ok = 0;
    constexpr std::int32_t dd_err_surface_lost =
        static_cast<std::int32_t>(0x887601c2U);
    constexpr std::int32_t dd_err_was_still_drawing =
        static_cast<std::int32_t>(0x8876021cU);

    SurfaceLockPlan plan;
    for (const auto result : observation.lock_results) {
        ++plan.lock_attempts;
        if (result == dd_ok) {
            plan.disposition = SurfaceLockDisposition::locked;
            return plan;
        }
        if (result == dd_err_surface_lost) {
            plan.disposition = SurfaceLockDisposition::surface_lost;
            plan.lost_surface_error_reported = true;
            plan.error_hresult = result;
            return plan;
        }
        if (result == dd_err_was_still_drawing) {
            plan.retry_still_drawing = true;
            continue;
        }
        plan.disposition = SurfaceLockDisposition::lock_error;
        plan.generic_lock_error_reported = true;
        plan.error_hresult = result;
        return plan;
    }
    return plan;
}

SurfaceRestorePlan plan_surface_restore(
    const SurfaceRestoreObservation& observation)
{
    SurfaceRestorePlan plan;
    plan.recovery_flag_set = true;
    plan.restore_call_count = 2;
    if (observation.primary_surface_hresult != 0) {
        ++plan.restore_error_count;
        plan.failed_hresults.push_back(observation.primary_surface_hresult);
    }
    if (observation.backbuffer_hresult != 0) {
        ++plan.restore_error_count;
        plan.failed_hresults.push_back(observation.backbuffer_hresult);
    }
    return plan;
}

SurfaceUnlockPlan plan_surface_unlock() noexcept
{
    SurfaceUnlockPlan plan{};
    plan.reached = true;
    return plan;
}

SurfaceFlipPlan plan_surface_flip(const SurfaceFlipObservation& observation)
{
    constexpr std::int32_t dd_ok = 0;
    constexpr std::int32_t dd_err_surface_lost =
        static_cast<std::int32_t>(0x887601c2U);
    constexpr std::int32_t dd_err_was_still_drawing =
        static_cast<std::int32_t>(0x8876021cU);

    SurfaceFlipPlan plan{};
    plan.ordered_operations.push_back({0x00412C80U, 0U, 0});
    if (!observation.renderer_ready) {
        return plan;
    }
    for (const auto result : observation.flip_results) {
        ++plan.flip_attempts;
        plan.ordered_operations.push_back({0x00459040U, plan.surface_global, result});
        if (result == dd_ok) {
            plan.disposition = SurfaceFlipDisposition::flipped;
            plan.return_word = 1U;
            return plan;
        }
        if (result == dd_err_was_still_drawing) {
            continue;
        }
        if (result == dd_err_surface_lost) {
            plan.disposition = SurfaceFlipDisposition::lost_surface_recovered;
            plan.ordered_operations.push_back({0x00410EA0U, 0U, result});
            plan.ordered_operations.push_back({0x0044EBD0U, 0U, 0});
            plan.restore = plan_surface_restore(observation.restore);
            plan.recovery_flag_set = true;
            plan.return_word = (observation.lost_surface_error_return_word |
                                observation.restore_return_word) & 0xFFFFFF00U;
            return plan;
        }
        plan.disposition = SurfaceFlipDisposition::non_lost_error_returned;
        plan.return_word = (static_cast<std::uint32_t>(result) & 0xFFFFFF00U) | 1U;
        return plan;
    }
    plan.disposition = SurfaceFlipDisposition::waiting_for_result;
    return plan;
}

RenderContextAcquisitionPlan plan_render_context_acquisition(
    const RenderContextAcquisitionObservation& observation)
{
    RenderContextAcquisitionPlan plan{};
    plan.ordered_operations.push_back({
        RenderContextAcquisitionOperationKind::clear_context, 0});
    plan.ordered_operations.push_back({
        RenderContextAcquisitionOperationKind::pump_messages, 0x00412C80U});
    if (!observation.renderer_ready) {
        plan.disposition = RenderContextAcquisitionDisposition::renderer_not_ready;
        plan.return_word = observation.message_pump_return_word & 0xFFFFFF00U;
        return plan;
    }

    plan.lock = plan_surface_lock(observation.lock);
    for (std::uint32_t attempt = 0; attempt < plan.lock.lock_attempts; ++attempt) {
        plan.ordered_operations.push_back({
            RenderContextAcquisitionOperationKind::lock_surface, 0x00458FB0U});
    }
    if (plan.lock.disposition == SurfaceLockDisposition::waiting_for_result) {
        plan.disposition = RenderContextAcquisitionDisposition::waiting_for_lock_result;
        return plan;
    }
    if (plan.lock.disposition != SurfaceLockDisposition::locked) {
        plan.disposition = RenderContextAcquisitionDisposition::lock_failed;
        if (plan.lock.lost_surface_error_reported) {
            plan.ordered_operations.push_back({
                RenderContextAcquisitionOperationKind::report_lost_surface,
                0x00410EA0U, 0, plan.lock.error_hresult.value_or(0)});
        } else if (plan.lock.generic_lock_error_reported) {
            plan.ordered_operations.push_back({
                RenderContextAcquisitionOperationKind::report_lock_error,
                plan.lock.error_helper, 0, plan.lock.error_hresult.value_or(0)});
        }
        plan.ordered_operations.push_back({
            RenderContextAcquisitionOperationKind::begin_surface_restore,
            0x0044EBD0U});
        plan.restore = plan_surface_restore(observation.restore);
        plan.ordered_operations.push_back({
            RenderContextAcquisitionOperationKind::restore_primary_surface,
            0x00459190U, 0, observation.restore.primary_surface_hresult});
        if (observation.restore.primary_surface_hresult != 0) {
            plan.ordered_operations.push_back({
                RenderContextAcquisitionOperationKind::report_restore_error,
                plan.restore->error_helper, 0,
                observation.restore.primary_surface_hresult});
        }
        plan.ordered_operations.push_back({
            RenderContextAcquisitionOperationKind::restore_backbuffer_surface,
            0x00459190U, 1, observation.restore.backbuffer_hresult});
        if (observation.restore.backbuffer_hresult != 0) {
            plan.ordered_operations.push_back({
                RenderContextAcquisitionOperationKind::report_restore_error,
                plan.restore->error_helper, 1,
                observation.restore.backbuffer_hresult});
        }
        plan.return_word = observation.restore_return_word & 0xFFFFFF00U;
        return plan;
    }

    plan.ordered_operations.push_back({
        RenderContextAcquisitionOperationKind::initialize_context, 0x00449350U});
    auto surface_bits = observation.surface_bits;
    auto row_pitch = observation.row_pitch;
    if (observation.locked_surface_description.has_value()) {
        constexpr std::size_t pitch_dword_index = 0x10U / sizeof(std::uint32_t);
        constexpr std::size_t surface_bits_dword_index =
            0x28U / sizeof(std::uint32_t);
        const auto& description = *observation.locked_surface_description;
        row_pitch = std::bit_cast<std::int32_t>(
            description[pitch_dword_index]);
        surface_bits = static_cast<std::uintptr_t>(
            description[surface_bits_dword_index]);
        plan.locked_surface_description_used = true;
    }
    plan.acquired_surface_bits = surface_bits;
    plan.acquired_row_pitch = row_pitch;
    initialize_render_context_from_surface(
        plan.context, observation.format_tag, surface_bits,
        observation.width, observation.height, row_pitch);
    if (surface_bits == 0) {
        plan.disposition = RenderContextAcquisitionDisposition::null_page_pointer;
        plan.ordered_operations.push_back({
            RenderContextAcquisitionOperationKind::report_null_page, 0x00410EA0U});
        plan.return_word = observation.null_page_error_return_word & 0xFFFFFF00U;
        return plan;
    }

    plan.disposition = RenderContextAcquisitionDisposition::acquired;
    plan.return_word =
        (observation.context_setup_return_word & 0xFFFFFF00U) | 1U;
    return plan;
}

} // namespace viper
