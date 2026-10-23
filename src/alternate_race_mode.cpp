#include "viper/alternate_race_mode.hpp"

#include <bit>
#include <cmath>

namespace viper {

AlternateRaceLoopIteration advance_alternate_race_loop(
    AlternateRaceLoopState& state,
    const int mode_value,
    const bool render_context_available,
    const bool control_callback_requests_update,
    const std::optional<std::uint16_t> key_event) noexcept
{
    if (state.exited) {
        return {};
    }

    AlternateRaceLoopIteration result{};
    if (mode_value == 3) {
        const bool exit_on_observation = state.state_three_observations > 30;
        ++state.state_three_observations;
        if (exit_on_observation) {
            state.exited = true;
            result.exited_now = true;
            return result;
        }
    } else {
        result.wait_500ms = true;
    }

    result.render_frame = render_context_available;
    result.pump_messages_and_poll_input = true;
    result.advance_control_state = control_callback_requests_update;
    if (key_event && (*key_event == 0x1b || *key_event == 0x31)) {
        state.exited = true;
        result.exited_now = true;
        if (*key_event == 0x31) {
            state.test_flag_set = true;
            result.set_test_flag_now = true;
        }
    }
    return result;
}

AlternateTestPromptIteration advance_alternate_test_prompt(
    AlternateTestPromptState& state,
    const bool test_prompt_enabled,
    const bool render_context_available,
    const bool input_event_available) noexcept
{
    if (state.phase == AlternateTestPromptPhase::complete) {
        return {};
    }

    AlternateTestPromptIteration result{};
    if (state.phase == AlternateTestPromptPhase::not_started) {
        if (!test_prompt_enabled) {
            state.phase = AlternateTestPromptPhase::complete;
            result.call_41f4c0 = true;
            return result;
        }
        result.call_413ce0 = true;
        state.phase = AlternateTestPromptPhase::waiting_for_render_context;
    }

    if (state.phase == AlternateTestPromptPhase::waiting_for_render_context) {
        result.try_render_context = true;
        if (!render_context_available) {
            result.pump_messages = true;
            return result;
        }
        result.draw_prompt = true;
        result.call_447ce0 = true;
        result.call_447c50 = true;
        state.phase = AlternateTestPromptPhase::waiting_for_input;
    }

    if (state.phase == AlternateTestPromptPhase::waiting_for_input) {
        if (!input_event_available) {
            result.pump_messages = true;
            return result;
        }
        result.consume_input_event = true;
        state.phase = AlternateTestPromptPhase::complete;
        result.call_41f4c0 = true;
    }
    return result;
}

AlternateTestPromptTextPlan plan_alternate_test_prompt_text(
    const bool render_context_available) noexcept
{
    AlternateTestPromptTextPlan plan{};
    if (!render_context_available) {
        return plan;
    }
    plan.reached = true;
    plan.operations[0] = {0x0044D710U, 320, 430, 0x004D3F5CU, 0x004F0B14U};
    plan.operations[1] = {0x0044D710U, 320, 440, 0x004D3F84U, 0x004F0B14U};
    plan.operations[2] = {0x0044D710U, 320, 450, 0x004D3FACU, 0x004F0B14U};
    return plan;
}

AlternateThreeFrameIteration advance_alternate_three_frame_helper(
    AlternateThreeFrameState& state,
    const bool render_context_available)
{
    AlternateThreeFrameIteration iteration{};
    if (state.successful_frames >= 3U) {
        iteration.completed = true;
        return iteration;
    }

    iteration.acquisition_attempted = true;
    iteration.ordered_helpers.push_back(0x00447C90U);
    iteration.acquired = render_context_available;
    if (!render_context_available) {
        // 0x41F4C0 retries immediately; it does not pump messages on failure.
        return iteration;
    }

    iteration.ordered_helpers = {
        0x00447C90U, 0x00449900U, 0x00449AA0U, 0x00447CE0U, 0x00447C50U};
    ++state.successful_frames;
    iteration.completed = state.successful_frames >= 3U;
    return iteration;
}

AlternateTestPromptWorkflowPlan advance_alternate_test_prompt_workflow(
    AlternateTestPromptWorkflowState& state,
    const AlternateTestPromptWorkflowObservation& observation)
{
    AlternateTestPromptWorkflowPlan plan{};
    if (state.complete) {
        plan.complete = true;
        return plan;
    }

    if (!state.three_frame_helper_started) {
        plan.prompt = advance_alternate_test_prompt(
            state.prompt, observation.test_prompt_enabled,
            observation.prompt_render_context_available,
            observation.input_event_available);
        if (plan.prompt.draw_prompt) {
            plan.prompt_text = plan_alternate_test_prompt_text(true);
        }
        if (plan.prompt.call_41f4c0) {
            state.three_frame_helper_started = true;
        }
    }

    if (state.three_frame_helper_started) {
        plan.three_frame = advance_alternate_three_frame_helper(
            state.three_frame,
            observation.three_frame_render_context_available);
        if (plan.three_frame->completed) {
            state.complete = true;
        }
    }
    plan.complete = state.complete;
    return plan;
}

bool should_switch_to_alternate_mode_three(
    const int current_mode,
    const bool outer_gate_426e50,
    const bool mode_gate_49be40,
    const bool result_index_available,
    const bool timer_latch_0x7d8,
    const bool stored_time_0x7d4_is_before_now,
    const std::span<const AlternateModeThreeCandidate> candidates) noexcept
{
    if (outer_gate_426e50 || current_mode != 1 || mode_gate_49be40 ||
        !result_index_available ||
        !timer_latch_0x7d8 || !stored_time_0x7d4_is_before_now) {
        return false;
    }

    for (const auto& candidate : candidates) {
        if (candidate.callback_is_eligible && !candidate.callback_succeeded) {
            return false;
        }
    }
    return true;
}

bool alternate_mode_object_is_ready(const AlternateModeObjectFields& fields) noexcept
{
    for (const auto value : fields.required_values_0x44_0x48_0x4c) {
        if (value == 0) {
            return false;
        }
    }
    return true;
}

void reset_alternate_mode_object(AlternateModeObjectFields& fields,
                                 std::int32_t& global_mode) noexcept
{
    fields.reset_values_0x54_0x5c_0x60_0x64 = {};
    global_mode = 1;
}

void store_alternate_mode_three(std::int32_t& global_mode) noexcept
{
    global_mode = 3;
}

bool alternate_mode_region_query(
    const AlternateModeRegionObservation& region,
    const std::array<float, 3>& first_point,
    const std::array<float, 3>& second_point) noexcept
{
    const auto span_z = static_cast<long double>(region.value_0x80) -
        region.value_0x74;
    const auto reverse_span_x = static_cast<long double>(region.value_0x6c) -
        region.value_0x78;
    const auto point_x_from_min = static_cast<long double>(second_point[0]) -
        region.value_0x6c;
    const auto point_z_from_min = static_cast<long double>(second_point[2]) -
        region.value_0x74;
    const auto span_x = static_cast<long double>(region.value_0x78) -
        region.value_0x6c;
    const auto first_side = span_x * point_x_from_min +
        span_z * point_z_from_min;
    if (!(first_side > 0.0L)) {
        return false;
    }

    const auto second_side =
        (static_cast<long double>(second_point[2]) - region.value_0x80) *
            span_z +
        (static_cast<long double>(second_point[0]) - region.value_0x78) *
            span_x;
    if (!(second_side < 0.0L)) {
        return false;
    }

    const auto first_point_side =
        (static_cast<long double>(first_point[2]) - region.value_0x74) *
            reverse_span_x +
        (static_cast<long double>(first_point[0]) - region.value_0x6c) *
            span_z;
    if (!(first_point_side > 0.0L)) {
        return false;
    }

    const auto final_side = reverse_span_x * point_z_from_min +
        span_z * point_x_from_min;
    return final_side < 0.0L;
}

AlternateModeInterfaceUpdateResult update_alternate_mode_interface(
    AlternateModeInterfaceState& state,
    const AlternateModeInterfaceUpdateInput& input,
    AlternateModeDisplayQueueState* display_queue) noexcept
{
    AlternateModeInterfaceUpdateResult result{};
    result.vehicle_adjustment_0x64 = input.vehicle_adjustment_0x64;
    for (std::size_t row = 0; row < result.position_0x38.size(); ++row) {
        const auto offset =
            static_cast<long double>(input.vehicle_orientation_0x38[row]) *
                input.position_offsets_0xd7c[0] +
            static_cast<long double>(input.vehicle_orientation_0x38[row + 3U]) *
                input.position_offsets_0xd7c[1] +
            static_cast<long double>(input.vehicle_orientation_0x38[row + 6U]) *
                input.position_offsets_0xd7c[2];
        result.position_0x38[row] = static_cast<float>(
            static_cast<long double>(input.vehicle_position_0x5c[row]) + offset);
    }
    state.position_0x38 = result.position_0x38;

    // The vtable method nudges the vehicle's +0x64 adjustment by 1000 when
    // it leaves the target object's [value-50, value+950] band.
    const auto target_value = static_cast<long double>(
        input.other_object_value_0x48_0x64);
    const auto lower = target_value - 50.0L;
    const auto upper = lower + 1000.0L;
    if (!(upper >= static_cast<long double>(result.vehicle_adjustment_0x64))) {
        result.vehicle_adjustment_0x64 = static_cast<float>(
            static_cast<long double>(result.vehicle_adjustment_0x64) - 1000.0L);
        result.vehicle_adjustment_changed = true;
    }
    if (!(static_cast<long double>(result.vehicle_adjustment_0x64) >= lower)) {
        result.vehicle_adjustment_0x64 = static_cast<float>(
            static_cast<long double>(result.vehicle_adjustment_0x64) + 1000.0L);
        result.vehicle_adjustment_changed = true;
    }

    const auto region_query = [&state, &input](
                                  const std::optional<AlternateModeRegionObservation>& region,
                                  const bool observed_result) {
        return region
                   ? alternate_mode_region_query(
                         *region, input.reference_point_0x2c,
                         state.position_0x38)
                   : observed_result;
    };

    switch (state.state_0x64) {
    case 0U:
        if (region_query(input.state_zero_region,
                         input.state_zero_query_succeeded)) {
            result.message_0x4d5b8c = true;
            state.state_0x64 = 1U;
            state.state_time_0x58 = input.current_time;
        }
        break;
    case 1U:
        if (region_query(input.state_one_region,
                         input.state_one_query_succeeded)) {
            result.message_0x4d5b98 = true;
        } else if (input.current_time - state.state_time_0x58 > 3.0F) {
            state.state_time_0x58 = input.current_time;
            state.state_0x64 = 2U;
            result.message_0x4d5ba4 = true;
        }
        break;
    case 2U:
        if (input.current_time - state.state_time_0x58 > 3.0F) {
            state.state_0x64 = 0U;
        }
        if (region_query(input.state_two_region,
                         input.state_two_query_succeeded)) {
            state.state_time_0x58 = input.current_time;
            state.state_0x64 = 3U;
        }
        break;
    case 3U: {
        result.virtual_slot_0x64_called = true;
        if (input.current_time - state.state_time_0x58 >
            60.0F) {
            state.state_0x64 = 0U;
        }
        result.virtual_slot_0x68_called = true;
        const auto x = static_cast<long double>(input.vehicle_velocity_0x234[0]);
        const auto y = static_cast<long double>(input.vehicle_velocity_0x234[1]);
        const auto z = static_cast<long double>(input.vehicle_velocity_0x234[2]);
        constexpr auto speed_squared_threshold =
            std::bit_cast<float>(0x4431c71cU);
        if (x * x + y * y + z * z > speed_squared_threshold) {
            state.elapsed_since_state_0x5c =
                input.current_time - state.state_time_0x58;
            state.vehicle_speed_0x60 = static_cast<float>(
                std::sqrt(x * x + y * y + z * z));
            ++state.transition_count_0x54;
            result.result_helpers_called = true;
            result.result_helper_vehicle_ordinal = input.vehicle_ordinal_0x510;
        }
        break;
    }
    default:
        // The native method reports an invalid state through 0x411000.
        result.invalid_state_panic_0x411000 = true;
        break;
    }

    const bool display_message_requested = result.message_0x4d5b8c ||
        result.message_0x4d5b98 || result.message_0x4d5ba4;
    if (display_message_requested && input.display_target &&
        input.display_target->active_flag_0x0c) {
        result.display_target_flag_stores = true;
        result.display_target_address_token =
            input.display_target->address_token;
        if (display_queue != nullptr) {
            result.display_queue_lock_entered = true;
            if (display_queue->count_0x158 <
                display_queue->address_tokens_0xd8.size()) {
                display_queue->address_tokens_0xd8[
                    display_queue->count_0x158] =
                    input.display_target->address_token;
                ++display_queue->count_0x158;
                result.display_queue_entry_added = true;
            } else {
                result.display_queue_full_panic_0x411000 = true;
            }
            result.display_queue_lock_left = true;
        }
    }
    return result;
}

std::vector<std::int32_t> alternate_visible_lap_indices(
    const std::int32_t lap_count,
    const std::span<const std::uint8_t> lap_record_available)
{
    const std::int32_t first_lap = lap_count > 20 ? lap_count - 20 : 1;
    std::vector<std::int32_t> visible;
    if (first_lap >= lap_count) {
        return visible;
    }
    visible.reserve(static_cast<std::size_t>(lap_count - first_lap));
    for (std::int32_t lap = first_lap; lap < lap_count; ++lap) {
        if (static_cast<std::size_t>(lap) >= lap_record_available.size() ||
            lap_record_available[static_cast<std::size_t>(lap)] == 0) {
            break;
        }
        visible.push_back(lap);
    }
    return visible;
}

AlternateRaceTextPlan plan_alternate_race_text(
    const AlternateRaceTextObservation& observation)
{
    AlternateRaceTextPlan plan{};
    if (!observation.render_context_available) {
        return plan;
    }

    plan.reached = true;
    plan.line_step = observation.font_line_height_0x4d770 + 3;
    const auto centered_x = observation.surface_width_0x509a4c / 2;
    const auto add = [&plan](const AlternateRaceTextKind kind,
                             const std::uint32_t draw_helper,
                             const std::int32_t x,
                             const std::int32_t y,
                             const std::uint32_t source_address,
                             const std::uint32_t color_source,
                             const std::int32_t lap_index = 0,
                             const std::array<std::uint32_t, 2> prerequisites = {},
                             const std::uint8_t prerequisite_count = 0,
                             const std::uint32_t format_address = 0) {
        plan.operations.push_back({kind, draw_helper, x, y, source_address,
            color_source, lap_index, prerequisites, prerequisite_count,
            format_address});
    };

    const auto line_height = observation.font_line_height_0x4d770;
    const auto step = plan.line_step;
    const auto y_name = 10;
    const auto y_notes = line_height + 13;
    const auto y_second_name = y_notes + step;
    const auto y_caption = y_second_name + step;
    add(AlternateRaceTextKind::entrant_name_field_0x05, 0x0044D710U,
        centered_x, y_name, 0x0045DFB0U, 0x004F0B78U);
    add(AlternateRaceTextKind::notes_caption, 0x0044D710U,
        centered_x, y_notes, 0x004D3F0CU, 0x004F0B68U);
    add(AlternateRaceTextKind::entrant_name_field_0x12, 0x0044D710U,
        centered_x, y_second_name, 0x0045DFB0U, 0x004F0B78U);
    add(AlternateRaceTextKind::results_caption, 0x0044D710U,
        centered_x, y_caption, 0x004D3F24U, 0x004F0B68U);
    add(AlternateRaceTextKind::track_direction_line, 0x0044D710U,
        centered_x, y_caption, observation.track_is_reversed_0xcd1
            ? 0x004D3F28U : 0x004D3F34U,
        0x004F0B78U, 0, {}, 0, 0x004D3F38U);
    add(AlternateRaceTextKind::elapsed_time_line, 0x0044D710U,
        centered_x, y_caption + step * 2, 0x00425790U, 0x004F0B68U,
        0, {0x00425790U, 0x00425A40U}, 2);

    const auto visible_laps = alternate_visible_lap_indices(
        observation.lap_count_from_vtable_0x48,
        observation.lap_record_available);
    auto lap_y = 100;
    for (const auto lap_index : visible_laps) {
        add(AlternateRaceTextKind::lap_time_line, 0x0044D6A0U,
            400, lap_y, 0x00424450U, 0x004F0B68U, lap_index,
            {0x00424450U, 0x00425A40U}, 2, 0x004D3F40U);
        lap_y += step;
    }
    return plan;
}

AlternateRaceFramePlan advance_alternate_race_frame(
    AlternateRaceLoopState& state,
    const AlternateRaceFrameObservation& observation)
{
    AlternateRaceFramePlan plan{};
    plan.loop = advance_alternate_race_loop(
        state, observation.mode_value, observation.render_context_available,
        observation.control_callback_requests_update, observation.key_event);
    if (plan.loop.render_frame) {
        auto text_observation = observation.text;
        text_observation.render_context_available = true;
        plan.text = plan_alternate_race_text(text_observation);
    }
    return plan;
}

} // namespace viper
