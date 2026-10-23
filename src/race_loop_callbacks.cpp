#include "viper/race_loop_callbacks.hpp"

#include "viper/legacy_fdiv.hpp"

#include <array>
#include <bit>
#include <cstddef>

namespace viper {
namespace {

constexpr std::array<RaceLoopCallbackSlots, 6> observed_rows{{
    {0x004035E0, 0x004052D0, 0x004052E0},
    {0x00000000, 0x0041A690, 0x00000000},
    {0x00000000, 0x00423380, 0x00000000},
    {0x00000000, 0x0040D570, 0x00000000},
    {0x00404960, 0x004052E0, 0x00000000},
    {0x00000000, 0x00422A70, 0x00000000},
}};

[[nodiscard]] std::int32_t float_word_as_signed(const float value) noexcept
{
    return std::bit_cast<std::int32_t>(value);
}

[[nodiscard]] std::uint32_t float_word_as_unsigned(const float value) noexcept
{
    return std::bit_cast<std::uint32_t>(value);
}

} // namespace

std::optional<RaceLoopCallbackSlots> race_loop_callbacks_for_state(
    const std::int32_t state) noexcept
{
    if (state < 0 || static_cast<std::size_t>(state) >= observed_rows.size()) {
        return std::nullopt;
    }
    return observed_rows[static_cast<std::size_t>(state)];
}

RaceMouseInputRefreshResult refresh_race_mouse_input(
    const RaceMouseInputRefreshObservation& observation) noexcept
{
    const auto half_x = observation.extent_x_0x509a4c / 2;
    const auto half_y = observation.extent_y_0x509a2c / 2;
    RaceMouseInputRefreshResult result{};
    result.axes_0x509660[0] = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(observation.raw_x) - half_x,
            static_cast<long double>(half_x),
            observation.fdiv_compatibility_enabled));
    result.axes_0x509660[1] = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(observation.raw_y) - half_y,
            static_cast<long double>(half_y),
            observation.fdiv_compatibility_enabled));
    for (std::size_t index = 0; index < result.button_bytes_0x509668.size(); ++index) {
        result.button_bytes_0x509668[index] = static_cast<std::uint8_t>(
            (observation.button_mask >> index) & 1U);
    }
    return result;
}

RaceControllerInputRefreshResult refresh_race_controller_input(
    const RaceControllerInputRefreshObservation& observation) noexcept
{
    RaceControllerInputRefreshResult result{};
    if (observation.state_read_result != 0U) {
        return result;
    }

    result.state_written = true;
    for (std::size_t index = 0; index < result.axes.size(); ++index) {
        auto value = (static_cast<long double>(observation.axis_words[index]) -
                      32768.0L) *
                     static_cast<long double>(3.0517578e-05F);
        if (value > 1.0L) {
            value = 1.0L;
        } else if (value < -1.0L) {
            value = -1.0L;
        }
        result.axes[index] = static_cast<float>(value);
    }

    result.pov_direction_0x5098c0 =
        static_cast<std::uint16_t>(observation.pov_word_0x20) == 0xffffU
            ? 0U
            : observation.pov_word_0x20 / 9000U + 1U;
    for (std::size_t index = 0; index < result.button_bytes_0x5098a0.size(); ++index) {
        result.button_bytes_0x5098a0[index] = static_cast<std::uint8_t>(
            (observation.button_bytes_0x30[index] >> 7U) & 1U);
    }
    return result;
}

float evaluate_race_input_binding(
    const RaceInputBinding& binding,
    const RaceInputBindingObservation& observation) noexcept
{
    const auto nonnegative = [](const float value) {
        return value <= 0.0F ? 0.0F : value;
    };
    switch (binding.kind) {
    case 1:
        return observation.device_button_states[
                   static_cast<std::uint8_t>(binding.selector)] != 0U
                   ? 1.0F
                   : 0.0F;
    case 2: {
        if (binding.selector < 8U) {
            const auto axis = observation.controller_axes_0x509888[
                binding.selector / 2U];
            return (binding.selector & 1U) == 0U
                       ? nonnegative(-axis)
                       : nonnegative(axis);
        }
        if (binding.selector < 12U) {
            const auto axis = observation.controller_axes_0x509888[
                binding.selector - 8U];
            return (1.0F - axis) * 0.5F;
        }
        if (binding.selector < 16U) {
            const auto axis = observation.controller_axes_0x509888[
                binding.selector - 12U];
            return (axis + 1.0F) * 0.5F;
        }
        return 0.0F;
    }
    case 3:
        return binding.selector < observation.button_bytes_0x5098a0.size() &&
                       observation.button_bytes_0x5098a0[binding.selector] != 0U
                   ? 1.0F
                   : 0.0F;
    case 4:
        return binding.selector == observation.pov_direction_0x5098c0
                   ? 1.0F
                   : 0.0F;
    case 5: {
        if (binding.selector < 4U) {
            const auto axis = observation.mouse_axes_0x509660[
                binding.selector / 2U];
            return (binding.selector & 1U) == 0U
                       ? nonnegative(-axis)
                       : nonnegative(axis);
        }
        if (binding.selector == 4U) {
            return (1.0F - observation.mouse_axes_0x509660[0]) * 0.5F;
        }
        if (binding.selector == 5U) {
            return (observation.mouse_axes_0x509660[0] + 1.0F) * 0.5F;
        }
        if (binding.selector == 6U) {
            return (1.0F - observation.mouse_axes_0x509660[1]) * 0.5F;
        }
        if (binding.selector == 7U) {
            return (observation.mouse_axes_0x509660[1] + 1.0F) * 0.5F;
        }
        return 0.0F;
    }
    case 6: {
        constexpr std::array<std::size_t, 3> byte_order{{0U, 2U, 1U}};
        if (binding.selector >= byte_order.size()) {
            return 0.0F;
        }
        return observation.mouse_button_bytes_0x509668[
                   byte_order[binding.selector]] != 0U
                   ? 1.0F
                   : 0.0F;
    }
    default:
        return 0.0F;
    }
}

bool race_input_binding_is_active(
    const RaceInputBinding& binding,
    const RaceInputBindingObservation& observation) noexcept
{
    const auto value = evaluate_race_input_binding(binding, observation);
    // The native unordered floating-point case also reaches the true arm:
    // Ghidra recovered !(value < 0.2) && !(value == 0.2).
    return !(value < 0.2F) && !(value == 0.2F);
}

bool race_input_binding_uses_ramped_path(
    const RaceInputBinding& binding) noexcept
{
    return binding.kind == 1U || binding.kind == 3U ||
           binding.kind == 4U || binding.kind == 6U;
}

RaceLoopBindingSamples sample_race_loop_bindings(
    const std::array<RaceInputBinding, race_loop_binding_slot_count>& bindings,
    const RaceInputBindingObservation& observation) noexcept
{
    RaceLoopBindingSamples samples{};
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        samples.values[index] =
            evaluate_race_input_binding(bindings[index], observation);
        samples.active[index] = static_cast<std::uint8_t>(
            race_input_binding_is_active(bindings[index], observation));
        samples.uses_ramped_path[index] = static_cast<std::uint8_t>(
            race_input_binding_uses_ramped_path(bindings[index]));
    }
    return samples;
}

void update_race_loop_control_state(
    RaceLoopControlState& state,
    const RaceLoopControlObservation& observation) noexcept
{
    state.value_0x5093b8 = observation.value_0x5093b0;
    state.flag_0x5093f0 = static_cast<std::uint8_t>(
        observation.input_0x509408_active);

    if (!observation.input_0x509430_uses_ramped_path) {
        state.value_0x5093c8 = observation.value_0x509430;
    } else {
        if (!observation.input_0x509430_active) {
            if (float_word_as_signed(state.value_0x5093c8) > 0) {
                state.value_0x5093c8 -= observation.frame_delta * 5.0F;
                if (state.value_0x5093c8 <= 0.0F) {
                    state.value_0x5093c8 = 0.0F;
                }
            }
        } else {
            state.value_0x5093c8 += observation.frame_delta * 10.0F;
        }
        if (float_word_as_signed(state.value_0x5093c8) > 0x3f800000) {
            state.value_0x5093c8 = 1.0F;
        }
    }

    state.flag_0x5093cc = static_cast<std::uint8_t>(
        observation.input_0x5093d0_active);
    state.flag_0x5093f8 = static_cast<std::uint8_t>(
        observation.input_0x509398_active);
    state.flag_0x509360 = static_cast<std::uint8_t>(
        observation.input_0x5093e8_active);
    state.difference_0x509344 =
        observation.value_0x509378 - observation.value_0x509400;
    state.flag_0x509420 = static_cast<std::uint8_t>(
        observation.input_0x5093e0_active);
    state.difference_0x509340 =
        observation.value_0x5093d8 - observation.value_0x509418;

    if (!observation.input_0x509418_active) {
        if (!observation.input_0x5093d8_active) {
            state.transition_latch_0x509354 = 0U;
        } else if (state.transition_latch_0x509354 == 0U) {
            if (state.counter_0x5093bc > -1) {
                --state.counter_0x5093bc;
            }
            state.transition_latch_0x509354 = 1U;
        }
    } else if (state.transition_latch_0x509354 == 0U) {
        if (state.counter_0x5093bc < observation.counter_limit_0x50935c) {
            ++state.counter_0x5093bc;
        }
        state.transition_latch_0x509354 = 1U;
    }
}

void update_race_loop_steering(
    RaceLoopSteeringState& state,
    const RaceLoopSteeringObservation& observation) noexcept
{
    const bool use_ramped_path = observation.input_0x5093a0_forces_response ||
                          observation.negative_binding_type_uses_ramped_path ||
                          observation.positive_binding_type_uses_ramped_path;
    if (!use_ramped_path) {
        state.value_0x509358 = observation.positive_binding_value_0x5093c0 -
                               observation.negative_binding_value_0x509338;
        const auto bias = 1.0F - observation.smoothing_0x509428;
        auto softened = (bias + 1.0F) * state.value_0x509358;
        if (float_word_as_signed(state.value_0x509358) < 1) {
            softened += bias;
            if (!(softened < 0.0F)) {
                softened = 0.0F;
            }
        } else {
            softened -= bias;
            if (softened <= 0.0F) {
                softened = 0.0F;
            }
        }
        state.value_0x509358 = observation.gain_0x509390 * softened;
        if (!(state.value_0x509358 >= -1.0F)) {
            state.value_0x509358 = -1.0F;
        }
        if (float_word_as_signed(state.value_0x509358) > 0x3f800000) {
            state.value_0x509358 = 1.0F;
        }
        return;
    }

    const auto response = observation.response_rate_0x5093a8 * 2.5F;
    const auto response_twice = response * 2.0F;
    if (float_word_as_signed(state.value_0x509358) > 0) {
        state.value_0x509358 -= response * observation.frame_delta;
        if (state.value_0x509358 <= 0.0F) {
            state.value_0x509358 = 0.0F;
        }
    }
    if (float_word_as_unsigned(state.value_0x509358) > 0x80000000U) {
        state.value_0x509358 += response * observation.frame_delta;
        if (!(state.value_0x509358 < 0.0F)) {
            state.value_0x509358 = 0.0F;
        }
    }
    if (observation.negative_binding_active) {
        state.value_0x509358 -= response_twice * observation.frame_delta;
    }
    if (observation.positive_binding_active) {
        state.value_0x509358 += response_twice * observation.frame_delta;
    }
    if (float_word_as_signed(state.value_0x509358) > 0x3f800000) {
        state.value_0x509358 = 1.0F;
    }
    if (float_word_as_unsigned(state.value_0x509358) > 0xbf800000U) {
        state.value_0x509358 = -1.0F;
    }
}

void update_race_loop_analog_channel(
    RaceLoopAnalogChannelState& state,
    const RaceLoopAnalogChannelObservation& observation) noexcept
{
    if (!observation.force_ramped_path &&
        !observation.binding_type_uses_ramped_path) {
        state.value = observation.binding_value * observation.binding_scale +
                      observation.binding_bias - 1.0F;
        if (state.value > 1.0F) {
            state.value = 1.0F;
        }
        if (float_word_as_unsigned(state.value) > 0x80000000U) {
            state.value = 0.0F;
        }
        return;
    }

    if (float_word_as_signed(state.value) > 0) {
        state.value -= observation.ramp_rate * 6.0F *
                       observation.frame_delta;
        if (state.value <= 0.0F) {
            state.value = 0.0F;
        }
    }
    if (observation.binding_active) {
        state.value += observation.ramp_rate * 12.0F *
                       observation.frame_delta;
    }
    if (float_word_as_signed(state.value) > 0x3f800000) {
        state.value = 1.0F;
    }
}

RaceOverlayTransition toggle_race_overlay_from_key_0x31(
    const std::int32_t current_state) noexcept
{
    return {
        current_state == 0 ? 4 : 0,
        false,
        true,
    };
}

RaceLoopPageTransition transition_race_loop_page_for_event(
    const std::int32_t current_page,
    const std::uint16_t event) noexcept
{
    if (event == 0x37bU) {
        return {11, true};
    }
    if (event >= 0x170U && event <= 0x17aU) {
        const auto page = static_cast<std::int32_t>(event - 0x170U);
        return {page, true};
    }
    return {current_page, false};
}

RaceLoopActiveFramePlan plan_race_loop_active_frame(
    const RaceLoopActiveFrameObservation& observation)
{
    RaceLoopActiveFramePlan plan{};
    plan.active_update_branch = observation.active_update_branch;
    const auto append = [&plan](const std::uint32_t address) {
        plan.calls.push_back({address});
    };
    const auto append_callback = [&plan](const std::uint32_t address,
                                         const bool receives_context) {
        RaceLoopPlannedCall call{};
        call.address = address;
        call.state_indexed_callback = true;
        call.receives_render_context = receives_context;
        plan.calls.push_back(call);
    };

    if (!observation.active_update_branch) {
        append(0x425920U);
        append(0x425530U);
        return plan;
    }

    append(0x45bc20U);
    append(0x448440U);
    append(0x45bd20U);
    const auto callbacks = race_loop_callbacks_for_state(
        observation.callback_state);
    if (callbacks && callbacks->slot_0x08 != 0U) {
        append_callback(callbacks->slot_0x08, false);
    }
    append(0x448450U);
    RaceLoopPlannedCall call_6b640{};
    call_6b640.address = 0x46b640U;
    call_6b640.trailing_arguments[0] = 0U;
    call_6b640.trailing_argument_count = 1U;
    plan.calls.push_back(call_6b640);
    append(0x41aea0U);
    append(0x425920U);
    append(0x425530U);
    append(0x4045b0U);

    if (observation.callback_state == 4) {
        append(0x404810U);
        if (!observation.mode4_render_helper_0x404810) {
            append(0x40c2f0U);
            plan.render_gate_passed =
                observation.mode4_secondary_render_check_0x40c2f0;
        } else {
            plan.render_gate_passed = true;
        }
    } else {
        plan.render_gate_passed = true;
    }
    if (!plan.render_gate_passed) {
        return plan;
    }

    if (callbacks && callbacks->slot_0x00 != 0U) {
        append_callback(callbacks->slot_0x00, false);
    }
    plan.render_context_requested = true;
    append(0x447c90U);
    if (!observation.render_context_acquired) {
        return plan;
    }

    append(0x449900U);
    if (observation.callback_state == 0) {
        RaceLoopPlannedCall viewport{};
        viewport.address = 0x4498a0U;
        viewport.trailing_arguments = {0U, 0x40U, 0x280U, 0x1e0U};
        viewport.trailing_argument_count = 4U;
        plan.calls.push_back(viewport);
    }
    append(0x45c090U);
    plan.frame_submitted = true;
    if (observation.callback_state == 0) {
        append(0x4498e0U);
    }
    if (callbacks && callbacks->slot_0x04 != 0U) {
        append_callback(callbacks->slot_0x04, true);
    }
    append(0x40aa30U);
    append(0x40c2f0U);
    if (observation.post_submit_context_check_0x40c2f0) {
        RaceLoopPlannedCall context_check{};
        context_check.address = 0x40c330U;
        context_check.receives_render_context = true;
        plan.calls.push_back(context_check);
        plan.post_submit_context_callback_reached = true;
    }
    append(0x447ce0U);
    plan.surface_unlock = plan_surface_unlock();
    return plan;
}

EscapeMenuInitializationPlan plan_escape_menu_initialization(
    EscapeMenuInitializationState& state,
    const EscapeMenuInitializationObservation& observation) noexcept
{
    EscapeMenuInitializationPlan plan{};
    const auto append = [&plan](const std::uint32_t address,
                                const std::array<std::uint32_t, 4>& arguments = {},
                                const std::uint8_t count = 0U) {
        RaceLoopPlannedCall call{};
        call.address = address;
        call.trailing_arguments = arguments;
        call.trailing_argument_count = count;
        plan.calls.push_back(call);
    };
    const auto initialize_text = [&state, &plan, &append](
                                     const std::uint32_t bit,
                                     const std::uint32_t handle,
                                     const std::uint32_t label,
                                     const std::uint32_t cleanup) {
        if ((state.initialized_mask_0x4eb21c & bit) != 0U) {
            return;
        }
        append(0x418c90U, {handle, label, 0U, 0U}, 2U);
        append(0x4bd130U, {cleanup, 0U, 0U, 0U}, 1U);
        state.initialized_mask_0x4eb21c |= bit;
        ++plan.localized_text_entries_initialized;
    };
    const auto refresh = [&append](const bool needs_refresh,
                                   const std::uint32_t handle) {
        if (needs_refresh) {
            append(0x418cc0U, {handle, 0U, 0U, 0U}, 1U);
        }
    };

    initialize_text(0x01U, 0x004eb200U, 0x004cf93cU, 0x004029b0U);
    initialize_text(0x02U, 0x004eb188U, 0x004cf950U, 0x004029a0U);
    initialize_text(0x04U, 0x004eb158U, 0x004cf964U, 0x00402990U);
    initialize_text(0x08U, 0x004eb178U, 0x004cf978U, 0x00402980U);

    if (observation.current_page_0x4cf5b4 != 12) {
        state.saved_page_0x4eb184 = observation.current_page_0x4cf5b4;
        plan.saved_current_page = true;
    }

    constexpr std::array<std::uint32_t, 4> button_bits{{
        0x10U, 0x20U, 0x40U, 0x80U,
    }};
    constexpr std::array<std::uint32_t, 4> descriptor_addresses{{
        0x004cf640U, 0x004cf678U, 0x004cf6b0U, 0x004cf6e8U,
    }};
    constexpr std::uint32_t descriptor_color = 0xfff5d3d6U;
    constexpr std::uint32_t descriptor_activate = 0x004029e0U;
    constexpr std::uint32_t descriptor_deactivate = 0x00402a10U;
    constexpr std::uint32_t descriptor_callback = 0x00402a40U;
    for (std::size_t index = 0; index < button_bits.size(); ++index) {
        const auto bit = button_bits[index];
        if ((state.initialized_mask_0x4eb21c & bit) != 0U) {
            continue;
        }
        switch (bit) {
        case 0x10U:
            refresh(observation.resume_text_needs_refresh_0x4eb200, 0x004eb200U);
            refresh(observation.restart_text_needs_refresh_0x4eb188, 0x004eb188U);
            refresh(observation.end_race_text_needs_refresh_0x4eb158, 0x004eb158U);
            break;
        case 0x20U:
            refresh(observation.resume_text_needs_refresh_0x4eb200, 0x004eb200U);
            refresh(observation.end_race_text_needs_refresh_0x4eb158, 0x004eb158U);
            break;
        case 0x40U:
            refresh(observation.resume_text_needs_refresh_0x4eb200, 0x004eb200U);
            refresh(observation.restart_text_needs_refresh_0x4eb188, 0x004eb188U);
            refresh(observation.end_race_text_needs_refresh_0x4eb158, 0x004eb158U);
            break;
        case 0x80U:
            refresh(observation.resume_text_needs_refresh_0x4eb200, 0x004eb200U);
            refresh(observation.disconnect_text_needs_refresh_0x4eb178, 0x004eb178U);
            break;
        default:
            break;
        }
        EscapeMenuButtonDescriptorInitialization descriptor{};
        descriptor.address = descriptor_addresses[index];
        descriptor.written_word_mask = 0x1fffU;
        const auto set_common_callbacks = [&descriptor](
                                             const std::uint32_t action,
                                             const std::uint32_t final_action) {
            descriptor.words[10] = descriptor_activate;
            descriptor.words[11] = descriptor_deactivate;
            descriptor.words[12] = descriptor_callback;
            descriptor.words[9] = descriptor_color;
            descriptor.words[8] = action;
            descriptor.words[5] = final_action;
        };
        descriptor.words[0] = observation.resume_text_handle_0x4eb204;
        switch (bit) {
        case 0x10U:
            descriptor.words[2] = observation.restart_text_handle_0x4eb18c;
            descriptor.words[4] = observation.end_race_text_handle_0x4eb15c;
            set_common_callbacks(3U, 1U);
            break;
        case 0x20U:
            descriptor.words[2] = observation.end_race_text_handle_0x4eb15c;
            descriptor.words[3] = 1U;
            set_common_callbacks(2U, 0U);
            break;
        case 0x40U:
            descriptor.words[2] = observation.restart_text_handle_0x4eb18c;
            descriptor.words[3] = 2U;
            descriptor.words[4] = observation.end_race_text_handle_0x4eb15c;
            set_common_callbacks(3U, 3U);
            break;
        case 0x80U:
            descriptor.words[2] = observation.disconnect_text_handle_0x4eb17c;
            descriptor.words[3] = 3U;
            set_common_callbacks(2U, 0U);
            break;
        default:
            break;
        }
        plan.button_descriptors.push_back(descriptor);
        state.initialized_mask_0x4eb21c |= bit;
        ++plan.button_groups_initialized;
    }
    return plan;
}

EscapeMenuEventPlan plan_escape_menu_event(
    EscapeMenuEventState& state,
    const EscapeMenuEventObservation& observation)
{
    EscapeMenuEventPlan plan{};
    const auto append = [&plan](const std::uint32_t address,
                                const std::array<std::uint32_t, 4>& arguments = {},
                                const std::uint8_t count = 0U) {
        RaceLoopPlannedCall call{};
        call.address = address;
        call.trailing_arguments = arguments;
        call.trailing_argument_count = count;
        plan.calls.push_back(call);
    };
    append(0x413d50U);
    if (!observation.input_available_0x413d50) {
        return plan;
    }
    plan.event_available = true;
    append(0x413d20U);
    append(0x40c2f0U);
    if (observation.render_cancel_gate_0x40c2f0) {
        append(0x40c3f0U, {observation.event_0x413d20, 0U, 0U, 0U}, 1U);
        plan.event_consumed = true;
        return plan;
    }

    if (observation.state_callback_present) {
        RaceLoopPlannedCall callback{};
        callback.address = observation.state_callback_address;
        callback.state_indexed_callback = true;
        callback.trailing_arguments[0] = observation.event_0x413d20;
        callback.trailing_argument_count = 1U;
        plan.calls.push_back(callback);
        if (observation.state_callback_handled) {
            plan.event_consumed = true;
            return plan;
        }
    }

    append(0x4018f0U, {observation.event_0x413d20, 0U, 0U, 0U}, 1U);
    const auto event = observation.event_0x413d20;
    if (event == 0x50U || event == 0x70U) {
        append(0x49be40U);
        if (!observation.multiplayer_mode_0x49be40) {
            state.mode_toggle_0x4eb238 = static_cast<std::uint8_t>(
                state.mode_toggle_0x4eb238 == 0U);
            if (state.mode_toggle_0x4eb238 == 0U) {
                append(0x425800U);
                append(0x46b900U);
            } else {
                append(0x4257d0U);
                append(0x46b8f0U);
            }
        }
        plan.event_consumed = true;
        return plan;
    }

    if (event == 0x31U) {
        plan.overlay_transition = toggle_race_overlay_from_key_0x31(
            state.callback_state_0x4cf5b8);
        state.callback_state_0x4cf5b8 =
            plan.overlay_transition->next_state;
        // 0x402960 calls 0x4035D0 and 0x402060. Those helpers set the
        // startup byte and refresh resources; neither sets 0x4EB20C.
        plan.startup_setup_byte_set = true;
        append(0x402960U,
               {static_cast<std::uint32_t>(state.callback_state_0x4cf5b8),
                0U, 0U, 0U}, 1U);
    } else if (event == 0x1bU) {
        append(0x49be40U);
        if (!observation.multiplayer_mode_0x49be40) {
            append(0x45c230U);
            if (observation.selected_player_ordinal_0x45c230 != -1) {
                append(0x45c230U);
                plan.unresolved_selected_player_virtual_call = true;
                plan.selected_player_vtable_argument =
                    observation.selected_player_ordinal_after_requery_0x45c230;
                if (!observation.selected_player_vtable_handled) {
                    append(0x45c2c0U);
                    append(0x40c4c0U,
                           {observation.active_player_count_0x45c2c0 < 4
                                ? 0x004cf640U : 0x004cf678U,
                            0U, 0U, 0U},
                           1U);
                } else {
                    append(0x402a40U, {1U, 0U, 0U, 0U}, 1U);
                    plan.exit_latch_set = true;
                }
            } else {
                append(0x402a40U, {1U, 0U, 0U, 0U}, 1U);
                plan.exit_latch_set = true;
            }
        } else {
            append(0x49be50U);
            if (observation.multiplayer_disconnected_0x49be50) {
                append(0x40c4c0U, {0x004cf6e8U, 0U, 0U, 0U}, 1U);
            } else {
                append(0x40a160U);
                if (observation.mode_event_poll_result_0x40a160 == 1) {
                    append(0x40c4c0U, {0x004cf6b0U, 0U, 0U, 0U}, 1U);
                }
            }
        }
    } else if (event == 0x121U) {
        append(0x45c1b0U);
    } else if (event == 0x122U) {
        append(0x45c1d0U);
    } else if (event == 0x124U) {
        append(0x45c230U);
        append(0x45c210U,
               {static_cast<std::uint32_t>(
                    observation.selected_player_ordinal_0x45c230),
                0U, 0U, 0U},
               1U);
    } else {
        plan.page_transition = transition_race_loop_page_for_event(
            state.page_0x4cf5b4, event);
        if (plan.page_transition->page_store_performed) {
            state.page_0x4cf5b4 = plan.page_transition->next_page;
        }
    }
    plan.event_consumed = true;
    return plan;
}

RaceLoopOuterIterationPlan plan_race_loop_outer_iteration(
    RaceLoopOuterState& state,
    const RaceLoopOuterObservation& observation)
{
    RaceLoopOuterIterationPlan plan{};
    bool exit_page_dispatched = false;
    const auto append = [&plan](const std::uint32_t address) {
        plan.calls.push_back({address});
    };

    append(0x40a160U);
    if (observation.poll_value_0x4d0c54 == 3) {
        state.exit_latch_0x4eb1e8 = 1U;
        --state.exit_poll_countdown_0x10;
    }
    state.escape_menu_event.callback_state_0x4cf5b8 =
        observation.frame.callback_state;
    state.page_side_effect_gate_0x4eb20c = static_cast<std::uint8_t>(
        observation.page_side_effect_gate_0x4eb20c);
    state.escape_menu_event.overlay_dirty_flag_0x4eb20c =
        state.page_side_effect_gate_0x4eb20c;

    if (state.startup_countdown_0x10 < 0) {
        if (state.exit_latch_0x4eb1e8 == 0U) {
            auto menu_initialization = observation.timeout_menu_initialization;
            menu_initialization.current_page_0x4cf5b4 =
                state.page_0x4cf5b4;
            plan.timeout_menu_initialization = plan_escape_menu_initialization(
                state.escape_menu, menu_initialization);
            append(0x4022f0U);
            plan.timeout_handler_reached = true;
            if (observation.timeout_menu_event) {
                state.escape_menu_event.page_0x4cf5b4 =
                    state.page_0x4cf5b4;
                plan.timeout_menu_event = plan_escape_menu_event(
                    state.escape_menu_event,
                    *observation.timeout_menu_event);
                state.page_0x4cf5b4 =
                    state.escape_menu_event.page_0x4cf5b4;
                if (plan.timeout_menu_event->overlay_dirty_flag_set) {
                    state.page_side_effect_gate_0x4eb20c = 1U;
                }
            }
            if (observation.page_after_timeout_menu) {
                state.page_0x4cf5b4 =
                    *observation.page_after_timeout_menu;
            }
            if (observation.callback_state_after_timeout_menu) {
                state.escape_menu_event.callback_state_0x4cf5b8 =
                    *observation.callback_state_after_timeout_menu;
            }
            state.exit_latch_0x4eb1e8 = static_cast<std::uint8_t>(
                observation.exit_latch_after_timeout_handler ||
                (plan.timeout_menu_event &&
                 plan.timeout_menu_event->exit_latch_set));
            if (plan.timeout_menu_event &&
                plan.timeout_menu_event->startup_setup_byte_set) {
                state.startup_setup_byte_0x4eb300 = 1U;
            }
        } else {
            state.page_0x4cf5b4 = 12;
            state.task_exit_byte_0x5087b4 = 1U;
            plan.page_store_to_12 = true;
            append(0x425710U);
            exit_page_dispatched = true;
        }
    } else {
        const bool countdown_was_zero =
            state.startup_countdown_0x10 == 0;
        --state.startup_countdown_0x10;
        if (countdown_was_zero) {
            append(0x49bea0U);
            append(0x425800U);
            append(0x4035d0U);
            plan.startup_setup_reached = true;
            state.initialization_latch_0x4e3220 = 1U;
            state.physics_active_byte_0x508760 = 0U;
            state.startup_setup_byte_0x4eb300 = 1U;
            state.exit_latch_0x4eb1e8 = static_cast<std::uint8_t>(
                observation.exit_latch_after_startup_setup);
        }
    }

    if (state.exit_latch_0x4eb1e8 != 0U && !exit_page_dispatched) {
        state.page_0x4cf5b4 = 12;
        state.task_exit_byte_0x5087b4 = 1U;
        plan.page_store_to_12 = true;
        append(0x425710U);
    }
    plan.exit_requested_at_page_gate = state.exit_latch_0x4eb1e8 != 0U;

    if (state.page_side_effect_gate_0x4eb20c != 0U) {
        append(0x4029c0U);
        state.page_side_effect_gate_0x4eb20c = 0U;
        state.escape_menu_event.overlay_dirty_flag_0x4eb20c = 0U;
        append(observation.mode_0x49be40_active ? 0x49c2b0U : 0x425660U);
        state.page_0x4cf5b4 = observation.page_after_prelude_0x4cf5b4;
    }
    if (state.page_0x4cf5b4 == 11) {
        append(0x401930U);
        state.page_0x4cf5b4 = observation.page_after_page11_handler_0x4cf5b4;
    }
    RaceLoopPlannedCall page_dispatch{};
    page_dispatch.address = 0x4259b0U;
    page_dispatch.trailing_arguments[0] = static_cast<std::uint32_t>(
        state.page_0x4cf5b4);
    page_dispatch.trailing_argument_count = 1U;
    plan.calls.push_back(page_dispatch);
    state.dispatched_page_0x508758 = static_cast<std::uint32_t>(
        state.page_0x4cf5b4);

    auto frame_observation = observation.frame;
    frame_observation.callback_state =
        state.escape_menu_event.callback_state_0x4cf5b8;
    plan.frame = plan_race_loop_active_frame(frame_observation);
    plan.calls.insert(plan.calls.end(), plan.frame.calls.begin(),
                      plan.frame.calls.end());
    append(0x49c110U);

    if (observation.active_state_after_frame &&
        state.startup_countdown_0x10 < 1) {
        append(0x447c50U);
        if (observation.surface_flip) {
            plan.surface_flip = plan_surface_flip(*observation.surface_flip);
        }
    }
    plan.returns_from_loop = state.exit_poll_countdown_0x10 < 1;
    return plan;
}

RaceLoopSimulationStepPlan plan_race_loop_simulation_step(
    RaceLoopSimulationStepState& state,
    const RaceLoopSimulationStepObservation& observation) noexcept
{
    RaceLoopSimulationStepPlan plan{};
    const long double raw_elapsed =
        static_cast<long double>(observation.current_time_0x425790) -
        static_cast<long double>(state.previous_time_0x4d4df0);
    plan.raw_elapsed = static_cast<float>(raw_elapsed);
    if (raw_elapsed >= 0.25L) {
        plan.update_delta = 0.25F;
        plan.delta_was_clamped = true;
    } else if (raw_elapsed <= 0.01L) {
        plan.update_delta = 0.01F;
        plan.delta_was_clamped = true;
    } else {
        plan.update_delta = static_cast<float>(raw_elapsed);
    }

    const auto append = [&plan](const std::uint32_t address,
                                const std::array<std::uint32_t, 4>& args,
                                const std::uint8_t count) {
        auto& call = plan.calls[plan.call_count++];
        call.address = address;
        call.trailing_arguments = args;
        call.trailing_argument_count = count;
    };
    append(0x415090U,
        {observation.synchronization_object_0x4d4ddc, 0U, 0U, 0U}, 3U);
    append(0x425790U, {}, 0U);
    append(0x43b290U,
        {std::bit_cast<std::uint32_t>(plan.update_delta), 0U, 0U, 0U}, 1U);

    state.previous_time_0x4d4df0 = observation.current_time_0x425790;
    plan.time_anchor_updated_before_timer_dispatch = true;
    append(0x46b620U,
        {observation.timer_object_0x4d4de4,
         std::bit_cast<std::uint32_t>(observation.current_time_0x425790), 0U, 0U}, 2U);
    append(0x4150a0U,
        {observation.synchronization_object_0x4d4ddc, 0U, 0U, 0U}, 3U);
    plan.call_count = 5U;
    return plan;
}

RaceLoopPeriodicUpdatePlan plan_race_loop_periodic_update_dispatch(
    RaceLoopPeriodicUpdateState& state,
    const RaceLoopPeriodicUpdateObservation& observation) noexcept
{
    RaceLoopPeriodicUpdatePlan plan{};
    const long double accumulated =
        static_cast<long double>(state.accumulator_0x509424) +
        static_cast<long double>(observation.simulation_delta);
    plan.periodic_tick = accumulated >= static_cast<long double>(0.055555556F);
    state.accumulator_0x509424 = plan.periodic_tick
        ? 0.0F : static_cast<float>(accumulated);

    auto& interpolation = plan.calls[plan.call_count++];
    interpolation.address = 0x444e10U;
    interpolation.trailing_arguments[0] = plan.periodic_tick ? 1U : 0U;
    interpolation.trailing_argument_count = 1U;
    if (plan.periodic_tick) {
        auto& periodic = plan.calls[plan.call_count++];
        periodic.address = 0x4170a0U;
        for (std::size_t index = 0;
             index < observation.periodic_callback_arguments_0x509384.size();
             ++index) {
            periodic.trailing_arguments[index] =
                observation.periodic_callback_arguments_0x509384[index];
        }
        periodic.trailing_argument_count = 3U;
    }
    return plan;
}

RaceLoop43B290Plan update_race_loop_43b290(
    RaceLoop43B290State& state,
    const RaceLoop43B290Observation& observation) noexcept
{
    RaceLoop43B290Plan plan{};
    plan.cadence_dispatch = plan_race_loop_periodic_update_dispatch(
        state.cadence,
        {observation.simulation_delta,
         observation.periodic_callback_arguments_0x509384});
    auto input_snapshot = observation.post_refresh_inputs[
        plan.cadence_dispatch.periodic_tick ? 1U : 0U];
    if (plan.cadence_dispatch.periodic_tick && observation.controller_refresh) {
        // 0x444E10 clears the four axis globals before calling 0x416D30.
        input_snapshot.controller_axes_0x509888.fill(0.0F);
        const auto controller = refresh_race_controller_input(
            *observation.controller_refresh);
        if (controller.state_written) {
            input_snapshot.controller_axes_0x509888 = {
                controller.axes[0], controller.axes[1],
                controller.axes[3], controller.axes[2]};
            input_snapshot.controller_axes_extra_0x509898 = {
                controller.axes[4], controller.axes[5]};
            input_snapshot.pov_direction_0x5098c0 =
                controller.pov_direction_0x5098c0;
            input_snapshot.button_bytes_0x5098a0 =
                controller.button_bytes_0x5098a0;
            plan.controller_refresh_applied = true;
        }
    }
    if (observation.mouse_refresh) {
        const auto mouse = refresh_race_mouse_input(*observation.mouse_refresh);
        input_snapshot.mouse_axes_0x509660 = mouse.axes_0x509660;
        input_snapshot.mouse_button_bytes_0x509668 = mouse.button_bytes_0x509668;
        plan.mouse_refresh_applied = true;
    }
    plan.sampled_inputs = input_snapshot;
    const auto bindings = sample_race_loop_bindings(
        observation.bindings, input_snapshot);
    const auto value = [&bindings](const RaceLoopBindingSlot slot) {
        return bindings.values[static_cast<std::size_t>(slot)];
    };
    const auto active = [&bindings](const RaceLoopBindingSlot slot) {
        return bindings.active[static_cast<std::size_t>(slot)] != 0U;
    };
    const auto ramped = [&bindings](const RaceLoopBindingSlot slot) {
        return bindings.uses_ramped_path[
                   static_cast<std::size_t>(slot)] != 0U;
    };

    RaceLoopSteeringObservation steering{};
    steering.frame_delta = observation.simulation_delta;
    steering.smoothing_0x509428 = observation.steering_smoothing_0x509428;
    steering.gain_0x509390 = observation.steering_gain_0x509390;
    steering.response_rate_0x5093a8 = observation.steering_response_rate_0x5093a8;
    steering.negative_binding_value_0x509338 = value(
        RaceLoopBindingSlot::steering_negative_0x509338);
    steering.positive_binding_value_0x5093c0 = value(
        RaceLoopBindingSlot::steering_positive_0x5093c0);
    steering.input_0x5093a0_forces_response =
        observation.force_ramped_path_0x5093a0;
    steering.negative_binding_type_uses_ramped_path = ramped(
        RaceLoopBindingSlot::steering_negative_0x509338);
    steering.positive_binding_type_uses_ramped_path = ramped(
        RaceLoopBindingSlot::steering_positive_0x5093c0);
    steering.negative_binding_active = active(
        RaceLoopBindingSlot::steering_negative_0x509338);
    steering.positive_binding_active = active(
        RaceLoopBindingSlot::steering_positive_0x5093c0);
    update_race_loop_steering(state.steering, steering);

    RaceLoopAnalogChannelObservation first_analog{};
    first_analog.frame_delta = observation.simulation_delta;
    first_analog.binding_value = value(RaceLoopBindingSlot::analog_0x509328);
    first_analog.binding_scale = observation.analog_0x5093f4_scale_0x50934c;
    first_analog.binding_bias = observation.analog_0x5093f4_bias_0x509370;
    first_analog.ramp_rate = observation.analog_0x5093f4_rate_0x509350;
    first_analog.force_ramped_path = observation.force_ramped_path_0x5093a0;
    first_analog.binding_type_uses_ramped_path = ramped(
        RaceLoopBindingSlot::analog_0x509328);
    first_analog.binding_active = active(RaceLoopBindingSlot::analog_0x509328);
    update_race_loop_analog_channel(state.analog_0x5093f4, first_analog);

    RaceLoopAnalogChannelObservation second_analog{};
    second_analog.frame_delta = observation.simulation_delta;
    second_analog.binding_value = value(RaceLoopBindingSlot::analog_0x509368);
    second_analog.binding_scale = observation.analog_0x509320_scale_0x509330;
    second_analog.binding_bias = observation.analog_0x509320_bias_0x509380;
    second_analog.ramp_rate = observation.analog_0x509320_rate_0x509348;
    second_analog.force_ramped_path = observation.force_ramped_path_0x5093a0;
    second_analog.binding_type_uses_ramped_path = ramped(
        RaceLoopBindingSlot::analog_0x509368);
    second_analog.binding_active = active(RaceLoopBindingSlot::analog_0x509368);
    update_race_loop_analog_channel(state.analog_0x509320, second_analog);

    RaceLoopControlObservation controls{};
    controls.frame_delta = observation.simulation_delta;
    controls.value_0x5093b0 = value(RaceLoopBindingSlot::direct_value_0x5093b0);
    controls.value_0x509430 = value(RaceLoopBindingSlot::analog_value_0x509430);
    controls.value_0x509400 = value(
        RaceLoopBindingSlot::difference_value_0x509400);
    controls.value_0x509378 = value(
        RaceLoopBindingSlot::difference_value_0x509378);
    controls.value_0x5093d8 = value(
        RaceLoopBindingSlot::difference_value_0x5093d8);
    controls.value_0x509418 = value(
        RaceLoopBindingSlot::difference_value_0x509418);
    controls.counter_limit_0x50935c = observation.counter_limit_0x50935c;
    controls.input_0x509408_active = active(RaceLoopBindingSlot::flag_0x509408);
    controls.input_0x509430_uses_ramped_path = ramped(
        RaceLoopBindingSlot::analog_value_0x509430);
    controls.input_0x509430_active = active(
        RaceLoopBindingSlot::analog_value_0x509430);
    controls.input_0x5093d0_active = active(RaceLoopBindingSlot::flag_0x5093d0);
    controls.input_0x509398_active = active(RaceLoopBindingSlot::flag_0x509398);
    controls.input_0x5093e8_active = active(RaceLoopBindingSlot::flag_0x5093e8);
    controls.input_0x5093e0_active = active(RaceLoopBindingSlot::flag_0x5093e0);
    controls.input_0x509418_active = active(
        RaceLoopBindingSlot::difference_value_0x509418);
    controls.input_0x5093d8_active = active(
        RaceLoopBindingSlot::difference_value_0x5093d8);
    update_race_loop_control_state(state.controls, controls);
    return plan;
}

RaceLoopSimulationFramePlan plan_race_loop_simulation_frame(
    RaceLoopSimulationStepState& step_state,
    RaceLoopPeriodicUpdateState& update_state,
    const RaceLoopSimulationStepObservation& step_observation,
    const std::array<std::uint32_t, 3>& periodic_callback_arguments) noexcept
{
    RaceLoopSimulationFramePlan plan{};
    plan.step = plan_race_loop_simulation_step(step_state, step_observation);
    plan.periodic_update = plan_race_loop_periodic_update_dispatch(
        update_state,
        {plan.step.update_delta, periodic_callback_arguments});
    return plan;
}

RaceOverlayToggleFramePlan plan_race_overlay_toggle_following_frame(
    const std::int32_t current_state,
    const RaceLoopActiveFrameObservation& following_frame_observation)
{
    RaceOverlayToggleFramePlan plan{};
    plan.transition = toggle_race_overlay_from_key_0x31(current_state);
    auto frame_observation = following_frame_observation;
    frame_observation.callback_state = plan.transition.next_state;
    plan.following_frame = plan_race_loop_active_frame(frame_observation);
    return plan;
}

State1InputResult handle_state1_input_event(
    State1InputState& state,
    const State1InputObservation& observation) noexcept
{
    State1InputResult result{};
    const auto select_page = [&state, &result](const std::int32_t page,
                                               const State1InputAction action) {
        state.page_0x4d3dd0 = page;
        result.action = action;
        result.page_store_performed = true;
        result.callback_return_value = true;
    };

    switch (observation.event) {
    case 0x3fU:
    case 0x68U:
        select_page(0, State1InputAction::select_help_page);
        break;
    case 0x69U:
        select_page(1, State1InputAction::select_info_page);
        break;
    case 0x6cU:
        select_page(2, State1InputAction::select_metrics_page);
        break;
    case 0x73U:
        if (observation.save_page_gate_passed) {
            select_page(3, State1InputAction::select_save_page);
        } else {
            // The event's table case remains handled even when its object gate
            // suppresses the page store.
            result.callback_return_value = true;
        }
        break;
    default:
        if (state.page_0x4d3dd0 >= 3) {
            state.page_0x4d3dd0 = 1;
            result.action = State1InputAction::translate_save_key;
            result.page_store_performed = true;
            result.callback_return_value = observation.save_key_maps_to_g;
        } else if (state.page_0x4d3dd0 == 0 &&
                   (observation.event == 0x3fU ||
                    observation.event == 0x48U)) {
            select_page(1, State1InputAction::select_info_page);
        } else if (state.page_0x4d3dd0 == 2) {
            switch (observation.event) {
            case 0x30U:
                state.overlay_flag_0x4d3dd4 ^= 1U;
                result.action = State1InputAction::toggle_overlay_flag;
                result.callback_return_value = true;
                break;
            case 0x42U:
                state.saved_coordinate_0x4f0b58 =
                    state.camera_coordinate_0x5081cc;
                state.saved_coordinate_0x4f0b60 =
                    state.camera_coordinate_0x5081d4;
                result.action = State1InputAction::capture_camera_coordinates;
                result.callback_return_value = true;
                break;
            case 0x51U:
                state.value_0x4d3df8 +=
                    observation.coordinate_increment_0x4c7560;
                result.action = State1InputAction::increment_value_0x4d3df8;
                result.callback_return_value = true;
                break;
            case 0x56U:
                state.camera_controller_value_0x4f0b5c +=
                    observation.controller_increment_0x4c7564;
                result.action = State1InputAction::update_camera_controller;
                result.camera_controller_callback_reached = true;
                result.camera_controller_virtual_0x40_reached = true;
                result.camera_controller_calls = {
                    0U, 0x419e80U, 0x41d1d0U};
                result.camera_controller_call_count = 3U;
                state.camera_controller_value_0x4f0b5c -=
                    observation.controller_increment_0x4c7564;
                result.callback_return_value = true;
                break;
            case 0x57U:
                state.value_0x4d3df4 +=
                    observation.coordinate_increment_0x4c7560;
                result.action = State1InputAction::increment_value_0x4d3df4;
                result.callback_return_value = true;
                break;
            case 0x62U:
                state.camera_coordinate_0x5081cc =
                    state.saved_coordinate_0x4f0b58;
                state.camera_coordinate_0x5081d4 =
                    state.saved_coordinate_0x4f0b60;
                result.action = State1InputAction::restore_camera_coordinates;
                result.callback_return_value = true;
                break;
            case 0x71U:
                state.value_0x4d3df8 -=
                    observation.coordinate_increment_0x4c7560;
                result.action = State1InputAction::decrement_value_0x4d3df8;
                result.callback_return_value = true;
                break;
            case 0x76U:
                state.camera_controller_value_0x4f0b5c = 0.0F;
                state.saved_coordinate_0x4f0b58 =
                    observation.reset_source_coordinates_0x4f0b24[0];
                state.saved_coordinate_0x4f0b60 =
                    observation.reset_source_coordinates_0x4f0b24[1];
                result.action = State1InputAction::reset_camera_controller;
                result.callback_return_value = true;
                break;
            case 0x77U:
                state.value_0x4d3df4 -=
                    observation.coordinate_increment_0x4c7560;
                result.action = State1InputAction::decrement_value_0x4d3df4;
                result.callback_return_value = true;
                break;
            default:
                break;
            }
        }
        break;
    }

    // 0x41A370 clears this byte for handled input and for unhandled input on
    // pages other than 2. Page 2 intentionally leaves it alone on a miss.
    if (result.callback_return_value || state.page_0x4d3dd0 != 2) {
        state.input_active_0x4f0b44 = 0U;
        result.input_active_byte_cleared = true;
    }
    return result;
}

} // namespace viper
