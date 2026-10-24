#include "viper/scene_car_update.hpp"

#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace viper {

SceneCarState scene_car_state_for_update(
    const int selected_car_ordinal,
    const int scene_car_ordinal,
    const int selection_state) noexcept
{
    if (selected_car_ordinal == scene_car_ordinal) {
        if (selection_state == 0) {
            return SceneCarState::state_1;
        }
        if (selection_state == 2) {
            return SceneCarState::state_2;
        }
    }
    return SceneCarState::state_0;
}

bool should_dispatch_scene_car_update(
    const std::uint16_t incoming_field,
    const std::uint32_t cached_field_0x38,
    const bool force_update_0x425830) noexcept
{
    return static_cast<std::uint32_t>(incoming_field) != cached_field_0x38
        || force_update_0x425830;
}

SceneCarDispatcherPlan plan_scene_car_dispatcher(
    const SceneCarDispatcherInput& input)
{
    SceneCarDispatcherPlan result{};
    result.resulting_shared_globals = input.shared_words;
    result.resulting_scalar_globals = input.scalar_globals;
    result.resulting_byte_global = input.byte_global;
    const auto requested_objects = input.scene_object_count > 0
        ? static_cast<std::size_t>(input.scene_object_count)
        : std::size_t{0};
    const auto requested_records = input.car_record_count > 0
        ? static_cast<std::size_t>(input.car_record_count)
        : std::size_t{0};
    const auto available_objects = std::min(requested_objects, input.scene_objects.size());
    const auto available_records = std::min(requested_records, input.car_records.size());
    result.operations.reserve(1 + scene_car_dispatch_local_record_count + 4
        + available_objects + available_records);

    const auto append = [&result](const SceneCarDispatcherOperationKind kind,
                          const std::size_t index = 0) -> SceneCarDispatcherOperation& {
        result.operations.push_back({});
        auto& operation = result.operations.back();
        operation.kind = kind;
        operation.index = index;
        return operation;
    };

    append(SceneCarDispatcherOperationKind::timer_begin_0x415090);
    for (std::size_t i = 0; i < scene_car_dispatch_local_record_count; ++i) {
        append(SceneCarDispatcherOperationKind::initialize_local_record_0x45dae0, i);
    }
    append(SceneCarDispatcherOperationKind::acquire_update_block_0x425880);
    auto& globals = append(SceneCarDispatcherOperationKind::copy_shared_globals);
    globals.shared_words = input.shared_words;
    globals.scalar_globals = input.scalar_globals;
    globals.byte_global = input.byte_global;
    append(SceneCarDispatcherOperationKind::parse_update_records_0x462d90);

    result.scene_object_inputs_exhausted =
        input.scene_object_count > 0 && input.scene_objects.size() < requested_objects;
    for (std::size_t i = 0; i < available_objects; ++i) {
        const auto& object = input.scene_objects[i];
        const bool dispatch = should_dispatch_scene_car_update(
            object.incoming_field,
            object.cached_field_0x38,
            object.force_update_0x425830);
        auto& operation = append(SceneCarDispatcherOperationKind::notify_scene_car_0x463530, i);
        operation.callback_dispatched = dispatch;
        operation.incoming_field = object.incoming_field;
        operation.resulting_cached_field = dispatch
            ? static_cast<std::uint32_t>(object.incoming_field)
            : object.cached_field_0x38;
        ++result.scene_objects_examined;
        result.scene_callbacks_dispatched += dispatch ? 1U : 0U;
    }

    result.car_record_inputs_exhausted =
        input.car_record_count > 0 && input.car_records.size() < requested_records;
    result.local_car_record_overread_possible =
        requested_records > scene_car_dispatch_local_record_count;
    for (std::size_t i = 0; i < available_records; ++i) {
        auto& operation = append(
            SceneCarDispatcherOperationKind::update_car_string_record_0x45e010, i);
        operation.record_words = input.car_records[i];
        ++result.car_records_examined;
    }
    append(SceneCarDispatcherOperationKind::timer_end_0x4150a0);
    return result;
}

std::array<SceneCarByteHandlerRegistration, 2>
scene_car_handlers_registered_by_scene_context() noexcept
{
    return {{{0x00468290U, 2U}, {0x00468450U, 1U}}};
}

SceneCarByteHandlerRegistration scene_car_handler_registered_by_effect_context() noexcept
{
    return {0x004692A0U, 0U};
}

std::optional<std::int32_t> scene_car_byte_handler_payload_bytes(
    const std::uint32_t callback_address) noexcept
{
    switch (callback_address) {
    case 0x00468290U: return 0x24;
    case 0x00468450U: return 0x18;
    case 0x004692A0U: return 0x1c;
    default: return std::nullopt;
    }
}

void reset_scene_car_byte_handler_registry(SceneCarByteHandlerRegistry& registry) noexcept
{
    registry.entries.clear();
    registry.capacity_overrun_possible = false;
}

void register_scene_car_byte_handler(
    SceneCarByteHandlerRegistry& registry,
    const SceneCarByteHandlerEntry handler)
{
    registry.entries.push_back(handler);
    registry.capacity_overrun_possible =
        registry.entries.size() > scene_car_byte_handler_entry_capacity;
}

SceneCarByteHandlerRemovalResult remove_scene_car_byte_handler(
    SceneCarByteHandlerRegistry& registry,
    const std::uint32_t callback_address) noexcept
{
    const auto found = std::find_if(
        registry.entries.begin(), registry.entries.end(),
        [callback_address](const SceneCarByteHandlerEntry& entry) {
            return entry.callback_address == callback_address;
        });
    if (found == registry.entries.end()) return {false, true};

    *found = registry.entries.back();
    registry.entries.pop_back();
    registry.capacity_overrun_possible =
        registry.entries.size() > scene_car_byte_handler_entry_capacity;
    return {true, false};
}

SceneCarByteParserPlan plan_scene_car_byte_parser(const SceneCarByteParserInput& input)
{
    SceneCarByteParserPlan result{};
    const auto handler_count = input.registry == nullptr
        ? std::size_t{0}
        : input.registry->entries.size();
    if (input.declared_payload_bytes <= 0) return result;

    const auto length = input.declared_payload_bytes;
    std::int32_t cursor = 0;
    while (cursor < length) {
        if (static_cast<std::size_t>(cursor) >= input.payload.size()) {
            result.status = SceneCarByteParserStatus::payload_input_exhausted;
            result.cursor_after = cursor;
            return result;
        }

        const auto tag = input.payload[static_cast<std::size_t>(cursor)];
        bool matched = false;
        std::int32_t last_result = -1;
        for (std::size_t index = 0; index < handler_count; ++index) {
            const auto& handler = input.registry->entries[index];
            if (handler.tag != tag) continue;
            matched = true;
            const auto recovered_length =
                scene_car_byte_handler_payload_bytes(handler.callback_address);
            if (recovered_length.has_value()) {
                last_result = *recovered_length;
            } else {
                if (result.callback_results_consumed >= input.callback_results.size()) {
                    result.status = SceneCarByteParserStatus::callback_result_input_exhausted;
                    result.cursor_after = cursor;
                    return result;
                }
                last_result = input.callback_results[result.callback_results_consumed++];
            }
            result.callbacks.push_back({
                static_cast<std::size_t>(cursor), index, handler.callback_address,
                tag, last_result, recovered_length.has_value()});
        }

        if (!matched) {
            result.status = SceneCarByteParserStatus::stopped_on_unregistered_tag;
            result.cursor_after = cursor;
            return result;
        }
        if (last_result == -1) {
            result.status = SceneCarByteParserStatus::stopped_by_handler_sentinel;
            result.cursor_after = cursor;
            return result;
        }

        const auto cursor_bits = std::bit_cast<std::uint32_t>(cursor);
        const auto advance_bits = std::bit_cast<std::uint32_t>(last_result) + 1U;
        cursor = std::bit_cast<std::int32_t>(cursor_bits + advance_bits);
        if (cursor < 0) {
            result.status = SceneCarByteParserStatus::invalid_negative_cursor;
            result.cursor_after = cursor;
            return result;
        }
    }

    result.cursor_after = cursor;
    return result;
}

SceneCarEffectTagZeroPlan plan_scene_car_effect_tag_zero_handler(
    const SceneCarEffectTagZeroInput& input) noexcept
{
    SceneCarEffectTagZeroPlan result{};
    result.cursor_after = input.cursor_0x540bf0;
    const auto available = std::min(
        scene_car_effect_tag_zero_pool_size, input.existing_effect_keys.size());
    for (std::size_t i = 0; i < available; ++i) {
        if (!input.existing_effect_keys[i].has_value()) {
            result.status = SceneCarEffectTagZeroStatus::null_pool_entry_fault;
            return result;
        }
        result.key_matches[i] = *input.existing_effect_keys[i] == input.payload_key_0x18;
        ++result.entries_compared;
    }
    if (available < scene_car_effect_tag_zero_pool_size) {
        result.status = SceneCarEffectTagZeroStatus::pool_observation_exhausted;
        return result;
    }

    const bool duplicate = std::any_of(
        result.key_matches.begin(), result.key_matches.end(), [](const bool matched) {
            return matched;
        });
    if (duplicate) {
        result.status = SceneCarEffectTagZeroStatus::duplicate_suppressed;
        result.returned_payload_bytes = 0x1c;
        return result;
    }

    const auto cursor_bits = std::bit_cast<std::uint32_t>(input.cursor_0x540bf0);
    const auto incremented_cursor = cursor_bits + 1U;
    result.selected_slot = static_cast<std::size_t>(incremented_cursor & 0x1fU);
    result.cursor_after = std::bit_cast<std::int32_t>(incremented_cursor & 0x1fU);
    result.cursor_written = true;
    if (!input.selected_slot_nonnull.has_value()) {
        result.status = SceneCarEffectTagZeroStatus::selected_slot_observation_missing;
        return result;
    }
    if (!*input.selected_slot_nonnull) {
        result.status = SceneCarEffectTagZeroStatus::selected_slot_null_fault;
        return result;
    }

    result.status = SceneCarEffectTagZeroStatus::generator_dispatched;
    result.generator_0x468af0_called = true;
    result.returned_payload_bytes = 0x1c;
    return result;
}

SceneCarEffectTagOnePlan plan_scene_car_effect_tag_one_handler(
    const SceneCarEffectTagOneInput& input) noexcept
{
    SceneCarEffectTagOnePlan result{};
    result.cursor_after = input.cursor_0x540b18;
    if (!input.scene_effects_enabled_0x509b74 || !input.effect_context_enabled_0x53a4c4) {
        result.status = SceneCarEffectTagOneStatus::context_gate_closed;
        return result;
    }
    if (input.cursor_0x540b18 < 0
        || static_cast<std::size_t>(input.cursor_0x540b18) >= scene_car_effect_tag_one_pool_size) {
        result.status = SceneCarEffectTagOneStatus::cursor_out_of_range;
        return result;
    }
    if (!input.selected_slot_time_0x18.has_value()) {
        result.status = SceneCarEffectTagOneStatus::slot_observation_missing;
        return result;
    }

    const float slot_time = *input.selected_slot_time_0x18;
    if (input.now_0x45bb70 > slot_time
        && slot_time + 0.5F > input.now_0x45bb70) {
        result.status = SceneCarEffectTagOneStatus::active_slot_retained;
        return result;
    }

    result.selected_slot = static_cast<std::size_t>(input.cursor_0x540b18);
    result.cursor_after = (input.cursor_0x540b18 + 1) & 0x1f;
    result.cursor_written = true;

    const float vx = input.payload_velocity[0] * 0.5F;
    const float vy = input.payload_velocity[1] * 0.5F
        + std::sqrt(input.payload_velocity[0] * input.payload_velocity[0]
            + input.payload_velocity[1] * input.payload_velocity[1]
            + input.payload_velocity[2] * input.payload_velocity[2]) * 0.1F;
    const float vz = input.payload_velocity[2] * 0.5F;
    result.stored_velocity = {vx, vy, vz};
    result.stored_origin = input.payload_origin;
    result.stored_time = input.now_0x45bb70;
    result.record_written = true;

    constexpr float random_scale = 0.001001001F;
    for (std::size_t draw = 0; draw < 3; ++draw) {
        if (draw >= input.random_samples.size()) {
            result.status = SceneCarEffectTagOneStatus::random_observation_exhausted;
            return result;
        }
        const float normalized = static_cast<float>(input.random_samples[draw]) * random_scale;
        const float perturbation = (-0.1F + 0.2F * normalized) * vx;
        const std::size_t axis = draw == 0 ? 0 : (draw == 1 ? 2 : 1);
        result.stored_velocity[axis] += perturbation;
        ++result.random_samples_consumed;
    }

    result.status = SceneCarEffectTagOneStatus::generated;
    return result;
}

SceneCarEffectTagTwoPlan plan_scene_car_effect_tag_two_handler(
    const SceneCarEffectTagTwoInput& input) noexcept
{
    SceneCarEffectTagTwoPlan result{};
    result.context_cursor_after = input.context_cursor_0x114;
    if (!input.scene_effects_enabled_0x509b74 || !input.effect_context_enabled_0x53a4c4) {
        result.status = SceneCarEffectTagTwoStatus::context_gate_closed;
        return result;
    }
    if (input.context_cursor_0x114 < 0
        || static_cast<std::size_t>(input.context_cursor_0x114)
            >= scene_car_effect_tag_two_context_count) {
        result.status = SceneCarEffectTagTwoStatus::cursor_out_of_range;
        return result;
    }

    result.selected_context = static_cast<std::size_t>(input.context_cursor_0x114);
    result.initializer_0x61ac0_called = true;
    result.child_initializer_position = input.child_initializer_position_0xc4;
    result.context_cursor_after = (input.context_cursor_0x114 + 1) & 0x0f;
    result.context_cursor_written = true;
    if (!input.previous_emit_time_0x118.has_value()) {
        result.status = SceneCarEffectTagTwoStatus::timestamp_observation_missing;
        return result;
    }
    if (!(input.now_0x45bb70 > *input.previous_emit_time_0x118 + 0.5F)) {
        result.status = SceneCarEffectTagTwoStatus::waiting_for_timestamp_window;
        return result;
    }

    result.stored_origin = input.payload_origin;
    result.stored_emit_time = input.now_0x45bb70;
    result.origin_and_time_written = true;

    float clamped_scale = input.payload_scale_0x1c * 0.01F;
    clamped_scale = std::isnan(clamped_scale)
        ? 7.0F
        : std::clamp(clamped_scale, 5.0F, 7.0F);
    result.clamped_payload_scale = clamped_scale;
    const float vx = input.payload_velocity[0] * 0.6F;
    const float vy = input.payload_velocity[1] * 0.6F;
    const float vz = input.payload_velocity[2] * 0.6F;
    result.vertical_scale = vy + clamped_scale;

    constexpr float random_scale = 0.001001001F;
    std::optional<std::size_t> pending_y_index;
    float pending_y{};
    const auto random_range = [&](const std::size_t index, const float minimum,
                                  const float maximum) {
        const float unit = static_cast<float>(input.random_samples[index]) * random_scale;
        return minimum + (maximum - minimum) * unit;
    };

    for (std::size_t point = 0; point < result.trail_points.size(); ++point) {
        if (pending_y_index.has_value()) {
            result.trail_points[*pending_y_index][1] = pending_y;
            result.point_component_write_masks[*pending_y_index] |= 0x02U;
            ++result.complete_points_written;
            pending_y_index.reset();
        }
        const auto draw = [&](const float minimum, const float maximum,
                              float& value) -> bool {
            if (result.random_samples_consumed >= input.random_samples.size()) return false;
            value = random_range(result.random_samples_consumed, minimum, maximum);
            ++result.random_samples_consumed;
            return true;
        };

        float x_jitter{};
        if (!draw(-0.6F, 0.6F, x_jitter)) {
            result.status = SceneCarEffectTagTwoStatus::random_observation_exhausted;
            return result;
        }
        result.trail_points[point][0] = vx
            + x_jitter * result.vertical_scale;
        result.point_component_write_masks[point] |= 0x01U;

        float z_jitter{};
        if (!draw(-0.6F, 0.6F, z_jitter)) {
            result.status = SceneCarEffectTagTwoStatus::random_observation_exhausted;
            return result;
        }
        result.trail_points[point][2] = vz
            + z_jitter * result.vertical_scale;
        result.point_component_write_masks[point] |= 0x04U;

        if (!draw(0.8F, 1.2F, pending_y)) {
            result.status = SceneCarEffectTagTwoStatus::random_observation_exhausted;
            return result;
        }
        pending_y *= result.vertical_scale;
        pending_y_index = point;
    }

    if (pending_y_index.has_value()) {
        result.trail_points[*pending_y_index][1] = pending_y;
        result.point_component_write_masks[*pending_y_index] |= 0x02U;
        ++result.complete_points_written;
    }
    result.status = SceneCarEffectTagTwoStatus::generated;
    return result;
}

SceneCarEffectChildInitPlan plan_scene_car_effect_child_initializer(
    const SceneCarEffectChildInitInput& input) noexcept
{
    SceneCarEffectChildInitPlan result{};
    result.stored_mode = input.mode;
    result.registration_id_0x34 = input.registration_id_0x34;
    result.registration_data_0x38 = input.registration_data_0x38;
    result.active_0x3c_written = true;
    result.stored_position = input.position;
    result.stored_position[1] += 0.2F;
    result.position_written = true;

    constexpr float random_scale = 0.000015259022F;
    const auto random_range = [&](const std::size_t sample_index, const float minimum,
                                  const float maximum) {
        const float unit = static_cast<float>(input.random_samples[sample_index]) * random_scale;
        return minimum + (maximum - minimum) * unit;
    };
    if (result.random_samples_consumed >= input.random_samples.size()) {
        result.status = SceneCarEffectChildInitStatus::random_observation_exhausted;
        return result;
    }
    result.lifetime_0x50 = random_range(result.random_samples_consumed, 0.75F, 1.75F);
    ++result.random_samples_consumed;
    const float current_time = static_cast<float>(input.timer_counter_0x508774) * 0.016F;
    result.expires_at_0x4c = current_time + result.lifetime_0x50;
    result.timer_0x54 = 0.0F;
    result.lifetime_fields_written = true;

    std::array<float, 3> noise{};
    constexpr std::array<float, 3> minimum_noise{-0.01F, 0.01F, -0.01F};
    constexpr std::array<float, 3> maximum_noise{0.01F, 0.03F, 0.01F};
    for (std::size_t draw = 0; draw < noise.size(); ++draw) {
        if (result.random_samples_consumed >= input.random_samples.size()) {
            result.status = SceneCarEffectChildInitStatus::random_observation_exhausted;
            return result;
        }
        noise[draw] = random_range(
            result.random_samples_consumed, minimum_noise[draw], maximum_noise[draw]);
        ++result.random_samples_consumed;
    }
    result.stored_velocity = {
        input.velocity[0] * 0.3F + noise[2],
        input.velocity[1] * 0.3F + noise[1],
        input.velocity[2] * 0.3F + noise[0],
    };
    result.velocity_fields_written = true;
    result.registration_0x461650_called = true;
    result.status = SceneCarEffectChildInitStatus::initialized;
    return result;
}

SceneCarBillboardQuadPlan plan_scene_car_billboard_quad(
    const SceneCarBillboardQuadInput& input) noexcept
{
    SceneCarBillboardQuadPlan result{};
    result.opaque_argument_0x38 = input.opaque_argument_0x38;
    result.opaque_argument_0x34 = input.opaque_argument_0x34;
    if (!input.converted_frame_index.has_value()) {
        result.status = SceneCarBillboardQuadStatus::frame_conversion_observation_missing;
        return result;
    }
    const auto frame = *input.converted_frame_index;
    result.converted_frame_index = frame;
    if (frame < 0 || frame >= 8) {
        result.status = SceneCarBillboardQuadStatus::frame_out_of_range;
        return result;
    }

    constexpr float atlas_cell = 0.125F;
    constexpr float atlas_gutter = 0.0078125F;
    constexpr float top_v = 0.1171875F;
    constexpr float bottom_v = 0.0078125F;
    const float u_min = static_cast<float>(frame) * atlas_cell + atlas_gutter;
    const float u_max = static_cast<float>(frame + 1) * atlas_cell - atlas_gutter;
    const float v_offset = input.mode == 0 ? 0.0F : atlas_cell;
    result.half_extent = (input.frame_and_size * 1.3000001F + 0.4F) * 0.5F;
    result.vertices = {{
        {-result.half_extent, -result.half_extent, u_min, top_v + v_offset},
        { result.half_extent, -result.half_extent, u_max, top_v + v_offset},
        { result.half_extent,  result.half_extent, u_max, bottom_v + v_offset},
        {-result.half_extent,  result.half_extent, u_min, bottom_v + v_offset},
    }};
    for (std::size_t vertex = 0; vertex < result.vertex_record_offsets.size(); ++vertex) {
        result.vertex_record_offsets[vertex] = static_cast<std::uint32_t>(vertex * 0x20U);
    }
    result.status = SceneCarBillboardQuadStatus::generated;
    result.vertex_data_written = true;
    result.shared_quad_submission_called = true;
    return result;
}

SceneRendererRefreshPlan plan_scene_renderer_refresh(
    const SceneRendererRefreshInput& input)
{
    SceneRendererRefreshPlan result{};
    result.stored_descriptor_block_identity = input.descriptor_block_identity;
    result.stored_byte_0x15 = input.stored_byte_0x15;
    result.binding_nodes_after.assign(
        input.existing_binding_nodes.begin(), input.existing_binding_nodes.end());
    result.resource_handle_cells_after.reserve(input.resources.size());
    for (const auto& resource : input.resources) {
        result.resource_handle_cells_after.push_back(resource.handle_cell_value);
    }

    const auto set_partial_status = [&](const SceneRendererRefreshStatus status) {
        if (result.status == SceneRendererRefreshStatus::complete) result.status = status;
    };
    const auto c_string_view = [](const std::string& value) {
        const auto nul = value.find('\0');
        return value.substr(0, nul == std::string::npos ? value.size() : nul);
    };
    const auto names_equal = [&](const std::string& left, const std::string& right) {
        const auto a = c_string_view(left);
        const auto b = c_string_view(right);
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            auto ca = static_cast<unsigned char>(a[i]);
            auto cb = static_cast<unsigned char>(b[i]);
            if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + ('a' - 'A'));
            if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + ('a' - 'A'));
            if (ca != cb) return false;
        }
        return true;
    };
    const auto stored_name = [&](const std::string& name) {
        return c_string_view(name).substr(0, 0x10);
    };
    const std::size_t requested_count = input.descriptor_count > 0
        ? static_cast<std::size_t>(input.descriptor_count)
        : 0U;
    const std::size_t available_count = std::min(requested_count, input.descriptors.size());
    const bool descriptor_span_incomplete = requested_count > input.descriptors.size()
        && (input.flags & 0x03U) != 0;

    if ((input.flags & 0x01U) != 0) {
        for (std::size_t descriptor_index = 0; descriptor_index < available_count;
             ++descriptor_index) {
            const auto& descriptor = input.descriptors[descriptor_index];
            for (std::int32_t vertex = descriptor.first_vertex_0x1c;
                 vertex < descriptor.end_vertex_0x1e; ++vertex) {
                if (vertex < 0 || static_cast<std::size_t>(vertex) >= input.source_vertices.size()) {
                    set_partial_status(SceneRendererRefreshStatus::vertex_source_observation_exhausted);
                    continue;
                }
                if (static_cast<std::size_t>(vertex) >= input.destination_vertex_capacity) {
                    set_partial_status(
                        SceneRendererRefreshStatus::vertex_destination_observation_exhausted);
                    continue;
                }
                const auto& source = input.source_vertices[static_cast<std::size_t>(vertex)];
                const auto subtract_wrap = [](const std::int16_t value,
                                              const std::int16_t center) {
                    const std::uint16_t bits = static_cast<std::uint16_t>(
                        static_cast<std::uint16_t>(value)
                        - static_cast<std::uint16_t>(center));
                    return std::bit_cast<std::int16_t>(bits);
                };
                const std::array<std::int16_t, 3> rebased{
                    subtract_wrap(source[0], descriptor.center_0x18),
                    subtract_wrap(source[1], descriptor.center_0x18),
                    subtract_wrap(source[2], descriptor.center_0x18),
                };
                result.rebased_vertices.push_back({static_cast<std::size_t>(vertex), rebased});
                result.actions.push_back({SceneRendererRefreshActionKind::rebase_vertex,
                    descriptor_index, static_cast<std::size_t>(vertex), 0, 0, {}});
                ++result.source_vertices_rebased;
            }
        }
    }

    if ((input.flags & 0x02U) != 0) {
        std::vector<SceneRendererBindingNode> updated_nodes;
        updated_nodes.reserve(std::max(requested_count, input.existing_binding_nodes.size()));
        std::size_t old_node_index = 0;
        std::size_t allocation_index = 0;
        std::size_t load_index = 0;
        for (std::size_t descriptor_index = 0; descriptor_index < available_count;
             ++descriptor_index) {
            const auto& descriptor = input.descriptors[descriptor_index];
            SceneRendererBindingNode node{};
            const bool reusing_node = old_node_index < input.existing_binding_nodes.size();
            if (reusing_node) {
                node = input.existing_binding_nodes[old_node_index++];
            } else {
                if (allocation_index >= input.node_allocation_results.size()) {
                    result.status = SceneRendererRefreshStatus::allocation_observation_exhausted;
                    break;
                }
                const bool allocated = input.node_allocation_results[allocation_index++] != 0;
                result.actions.push_back({SceneRendererRefreshActionKind::allocate_binding_node,
                    descriptor_index, allocation_index - 1U, 0,
                    allocated ? 1 : 0, {}});
                ++result.node_allocations_consumed;
                if (!allocated) {
                    result.status = SceneRendererRefreshStatus::binding_node_allocation_failed;
                    break;
                }
                node.handle = -1;
            }

            const std::string descriptor_name = c_string_view(descriptor.name_0x00);
            const std::string source_name = names_equal(descriptor_name, node.name)
                ? node.name
                : descriptor_name;
            std::optional<std::size_t> resource_index;
            for (std::size_t index = 0; index < input.resources.size(); ++index) {
                if (names_equal(source_name, input.resources[index].lookup_name_0x00)) {
                    resource_index = index;
                    break;
                }
            }
            const std::string canonical_name = resource_index.has_value()
                ? c_string_view(input.resources[*resource_index].canonical_name_0x10)
                : source_name;
            const std::int32_t resource_id = resource_index.has_value()
                ? input.resources[*resource_index].resource_id_0x20
                : 0;

            if (!names_equal(node.name, canonical_name) || node.resource_id != resource_id) {
                if (load_index >= input.resource_load_results.size()) {
                    result.actions.push_back({SceneRendererRefreshActionKind::load_resource,
                        descriptor_index, 0, resource_index.value_or(
                            std::numeric_limits<std::size_t>::max()), resource_id, canonical_name});
                    node.name = stored_name(canonical_name);
                    node.resource_id = resource_id;
                    updated_nodes.push_back(std::move(node));
                    result.status = SceneRendererRefreshStatus::resource_load_observation_exhausted;
                    break;
                }
                const std::int32_t loaded_handle = input.resource_load_results[load_index++];
                result.actions.push_back({SceneRendererRefreshActionKind::load_resource,
                    descriptor_index, 0, resource_index.value_or(
                        std::numeric_limits<std::size_t>::max()), loaded_handle, canonical_name});
                ++result.resource_loads_consumed;
                if (node.handle != -1) {
                    result.actions.push_back({SceneRendererRefreshActionKind::release_resource,
                        descriptor_index, 0, resource_index.value_or(
                            std::numeric_limits<std::size_t>::max()), node.handle, node.name});
                }
                node.name = stored_name(canonical_name);
                node.resource_id = resource_id;
                node.handle = loaded_handle;
                if (resource_index.has_value()
                    && input.resources[*resource_index].handle_cell_present_0x24) {
                    result.resource_handle_cells_after[*resource_index] = loaded_handle;
                    result.actions.push_back({SceneRendererRefreshActionKind::update_handle_cell,
                        descriptor_index, 0, *resource_index, loaded_handle, canonical_name});
                }
            }

            const auto first_vertex = static_cast<std::int32_t>(descriptor.first_vertex_0x1c);
            const auto end_vertex = static_cast<std::int32_t>(descriptor.end_vertex_0x1e);
            node.vertex_pointer_0x18 = input.destination_vertex_buffer_address
                + static_cast<std::uint32_t>(first_vertex * 6);
            node.vertex_component_count_0x1c = (end_vertex - first_vertex) * 3;
            node.texture_offset_0x20 = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(static_cast<std::int32_t>(descriptor.center_0x18) * 0x20)
                + static_cast<std::uint32_t>(input.texture_data_base_0x4d9fec));
            node.texture_component_count_0x24 =
                static_cast<std::int32_t>(descriptor.field_0x1a)
                - static_cast<std::int32_t>(descriptor.center_0x18);
            updated_nodes.push_back(std::move(node));
            ++result.descriptor_rows_processed;
        }

        if (result.status == SceneRendererRefreshStatus::complete && !descriptor_span_incomplete) {
            for (; old_node_index < input.existing_binding_nodes.size(); ++old_node_index) {
                const auto& stale = input.existing_binding_nodes[old_node_index];
                result.actions.push_back({SceneRendererRefreshActionKind::release_resource,
                    requested_count, old_node_index, 0, stale.handle, stale.name});
                result.actions.push_back({SceneRendererRefreshActionKind::free_binding_node,
                    requested_count, old_node_index, 0, 0, stale.name});
            }
        }
        result.binding_nodes_after = std::move(updated_nodes);
    }

    if (descriptor_span_incomplete) {
        set_partial_status(SceneRendererRefreshStatus::descriptor_observation_exhausted);
    }
    return result;
}

SceneCarEffectParticleGeneratorPlan plan_scene_car_effect_particle_generator(
    const SceneCarEffectParticleGeneratorInput& input) noexcept
{
    SceneCarEffectParticleGeneratorPlan result{};
    result.stored_effect_key = input.effect_key;
    result.stored_origin = input.origin;

    float x = input.velocity[0];
    float y = input.velocity[1];
    float z = input.velocity[2];
    const float speed = std::sqrt(x * x + y * y + z * z);
    if (5.0F < speed) {
        const float scale = static_cast<float>(divide_with_vc98_fdiv_compatibility(
            5.0L, static_cast<long double>(speed), input.fdiv_compatibility_enabled));
        x *= scale;
        y *= scale;
        z *= scale;
    }

    x *= 0.4F;
    z *= 0.4F;
    if (y < -4.905F) y = -4.905F;
    result.scaled_velocity = {x, y, z};

    const float horizontal_squared = x * x + z * z;
    if (horizontal_squared < 0.1F) {
        result.status = SceneCarEffectGeneratorStatus::inactive_horizontal_motion;
        return result;
    }

    const float horizontal_length = std::sqrt(horizontal_squared);
    const float lateral_z = static_cast<float>(divide_with_vc98_fdiv_compatibility(
        static_cast<long double>(z), static_cast<long double>(horizontal_length),
        input.fdiv_compatibility_enabled));
    const float lateral_x = static_cast<float>(divide_with_vc98_fdiv_compatibility(
        static_cast<long double>(-x), static_cast<long double>(horizontal_length),
        input.fdiv_compatibility_enabled));

    for (std::size_t group = 0; group < 3; ++group) {
        const float spread = static_cast<float>(group) * 0.33333334F * 3.0F - 1.5F;
        for (std::size_t column = 0; column < 5; ++column) {
            const auto index = column * 3U + group;
            result.control_points[index] = {
                input.origin[0] + lateral_z * spread,
                input.origin[1],
                input.origin[2] + spread * lateral_x};
            result.control_point_flags[index] = 0xffffffffU;
        }
    }
    result.control_points_written = true;

    constexpr std::array<float, 3> minimum_noise{-5.0F, -10.0F, -5.0F};
    constexpr std::array<float, 3> noise_range{10.0F, 20.0F, 10.0F};
    const float particle_velocity_x = x * 1.4F;
    const float particle_velocity_y = y * -1.2F;
    const float particle_velocity_z = z * 1.4F;
    for (std::size_t group = 0; group < 3; ++group) {
        for (std::size_t row = 0; row < 5; ++row) {
            if (result.random_samples_consumed + 3U > input.random_percent_samples.size()) {
                result.status = SceneCarEffectGeneratorStatus::random_observation_exhausted;
                return result;
            }

            std::array<float, 3> noise{};
            for (std::size_t axis = 0; axis < noise.size(); ++axis) {
                const auto sample = input.random_percent_samples[result.random_samples_consumed++];
                const long double scaled_sample = static_cast<long double>(sample) * 0.01L;
                noise[axis] = static_cast<float>(
                    static_cast<long double>(noise_range[axis]) * scaled_sample
                    + static_cast<long double>(minimum_noise[axis]));
            }

            const float attenuation = 1.0F - static_cast<float>(row) * 0.2F;
            const float position_scale = attenuation + 0.05F;
            const float fan = (static_cast<float>(group) * 0.33333334F * 2.0F - 1.0F)
                * (attenuation * 0.5F + 0.5F);
            const auto particle_index = row * 3U + group;
            auto& particle = result.cloud_particles[particle_index];
            particle[0] = particle_velocity_x * position_scale + noise[0]
                + fan * lateral_z * 10.0F;
            particle[1] = (1.0F - (1.0F - position_scale) * (1.0F - position_scale))
                * particle_velocity_y;
            particle[2] = particle_velocity_z * position_scale + noise[2]
                + fan * lateral_x * 10.0F;
            ++result.cloud_particles_written_count;
        }
    }

    result.status = SceneCarEffectGeneratorStatus::generated;
    result.duration_0x370 = static_cast<float>(
        divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(particle_velocity_y), 9.81L,
            input.fdiv_compatibility_enabled) * 2.0L);
    result.time_0x36c = input.now_0x45bb70;
    result.mode_word_0x378 = 0x3f333333U;
    result.lifetime_fields_written = true;
    return result;
}

SceneCarDispatchIndices select_scene_car_dispatch_indices(
    const SceneCarDispatchIndicesInput& input) noexcept
{
    const float delta_x = input.object_position[0] - input.camera_position[0];
    const float delta_y = input.object_position[1] - input.camera_position[1];
    const float delta_z = input.object_position[2] - input.camera_position[2];
    const float distance = std::sqrt(
        delta_x * delta_x + delta_z * delta_z + delta_y * delta_y);

    std::int32_t index_0x510 = 4;
    std::int32_t index_0x514 = 4;
    const std::size_t count = input.distance_thresholds_0x288.size();
    for (std::size_t index = 0; index < count; ++index) {
        if (!(input.distance_thresholds_0x288[index].distance <= distance)) {
            index_0x510 = static_cast<std::int32_t>(index);
            index_0x514 = static_cast<std::int32_t>(index);
            break;
        }
    }

    const auto wrap_add = [](const std::int32_t left, const std::int32_t right) {
        const auto left_bits = std::bit_cast<std::uint32_t>(left);
        const auto right_bits = std::bit_cast<std::uint32_t>(right);
        return std::bit_cast<std::int32_t>(left_bits + right_bits);
    };
    index_0x510 = wrap_add(index_0x510, input.global_index_adjustment);
    index_0x514 = wrap_add(index_0x514, input.global_index_adjustment / 2);

    const auto signed_count = static_cast<std::int32_t>(count);
    if (index_0x510 >= signed_count) {
        index_0x510 = signed_count - 1;
    }
    if (index_0x514 >= signed_count) {
        index_0x514 = signed_count - 1;
    }
    return {distance, index_0x510, index_0x514};
}

void copy_scene_car_pose(const SceneCarPose& source, SceneCarPose& destination) noexcept
{
    std::copy(source.begin(), source.end(), destination.begin());
}

SceneCarWheelTransformOutput transform_scene_car_wheel(
    const SceneCarWheelTransformInput& input) noexcept
{
    const float vertical_offset = input.wheel_index < 2
        ? input.front_vertical_offset_0x7ac
        : input.rear_vertical_offset_0x7b0;
    const float wheel_x = input.wheel_local_position[0];
    const float wheel_y = input.wheel_local_position[1] + vertical_offset;
    const float wheel_z = input.wheel_local_position[2];

    SceneCarWheelTransformOutput result{};
    result.basis = input.basis;
    result.position[0] = input.translation[0] + input.basis[3] * wheel_y
        + input.basis[0] * wheel_x + input.basis[6] * wheel_z;
    result.position[1] = input.translation[1] + input.basis[7] * wheel_z
        + input.basis[1] * wheel_x + input.basis[4] * wheel_y;
    result.position[2] = input.translation[2] + input.basis[8] * wheel_z
        + input.basis[2] * wheel_y + input.basis[5] * wheel_x;
    for (std::size_t axis = 0; axis < result.position.size(); ++axis) {
        result.position_delta[axis] = result.position[axis] - input.previous_wheel_position[axis];
    }
    for (std::size_t i = 0; i < result.basis.size(); ++i) {
        result.mirrored_transform[i] = result.basis[i];
    }
    for (std::size_t i = 0; i < result.position.size(); ++i) {
        result.mirrored_transform[result.basis.size() + i] = result.position[i];
    }
    return result;
}

SceneWheelStateDelta compare_scene_wheel_state(
    const SceneWheelStateWords& previous,
    const SceneWheelStateWords& incoming,
    const bool source_active,
    const bool state_reset) noexcept
{
    SceneWheelStateDelta result{};
    result.has_falling_edge = state_reset;
    result.has_rising_edge = state_reset;
    if (!source_active) {
        return result;
    }

    for (std::size_t i = 0; i < scene_wheel_state_word_count; ++i) {
        const auto prior = state_reset ? std::uint16_t{0} : previous[i];
        const auto rising = static_cast<std::uint16_t>(incoming[i] & ~prior);
        const auto falling = static_cast<std::uint16_t>(prior & ~incoming[i]);
        result.rising_edges[i] = rising;
        for (unsigned bit = 0; bit < 16; ++bit) {
            result.rising_bit_count += (rising >> bit) & 1U;
        }
        result.has_falling_edge = result.has_falling_edge || falling != 0;
        result.has_rising_edge = result.has_rising_edge || rising != 0;
    }
    return result;
}

SceneWheelCallbackPlan plan_scene_wheel_callback_operations(
    const SceneWheelStateDelta& delta,
    const std::int32_t id_0x308,
    const std::int32_t id_0x30c,
    const std::int32_t id_0x310,
    const float grid_scale_0x4c940c) noexcept
{
    SceneWheelCallbackPlan result{};
    const auto append_event = [&result](const std::int32_t first_id,
                               const std::int32_t second_id,
                               const std::array<float, 4>& rectangle) {
        result.operations[result.operation_count++] = {
            SceneWheelCallbackOperationKind::resource_event_0x447e50,
            first_id, second_id, rectangle};
    };
    const auto append_refresh = [&result](const std::int32_t id) {
        result.operations[result.operation_count++] = {
            SceneWheelCallbackOperationKind::resource_refresh_0x447ee0,
            id, 0, {}};
    };

    if (delta.has_falling_edge) {
        append_event(id_0x30c, id_0x308, {0.0F, 0.0F, 1.0F, 1.0F});
        append_refresh(id_0x308);
    }
    if (delta.has_rising_edge) {
        for (std::size_t word = 0; word < scene_wheel_state_word_count; ++word) {
            for (unsigned bit = 0; bit < 16; ++bit) {
                if (((delta.rising_edges[word] >> bit) & 1U) == 0) {
                    continue;
                }
                const float left = static_cast<float>(bit) * grid_scale_0x4c940c;
                const float top = static_cast<float>(word) * grid_scale_0x4c940c;
                const float right = static_cast<float>(bit + 1U) * grid_scale_0x4c940c;
                const float bottom = static_cast<float>(word + 1U) * grid_scale_0x4c940c;
                append_event(id_0x310, id_0x308, {left, top, right, bottom});
            }
        }
        append_refresh(id_0x308);
    }
    return result;
}

void initialize_scene_wheel_resource_allocator(
    SceneWheelResourceAllocatorState& allocator,
    const std::span<SceneWheelResourceSlot> allocated_slots,
    const std::int32_t skid_texture_handle) noexcept
{
    allocator.slots = allocated_slots;
    allocator.next_slot_index_0x0c = 0;
    allocator.texture_handle_0x10 = skid_texture_handle;
    for (auto& slot : allocator.slots) {
        slot.lifecycle_state_0x30 = scene_wheel_resource_slot_free;
    }
}

std::uint32_t scene_wheel_resource_pool_slot_count(
    const std::uint32_t feature_mode_0x45bac0,
    const bool selected_pcar_ordinal_0x45c230_matches_car) noexcept
{
    if (feature_mode_0x45bac0 == 0U) {
        return 0U;
    }

    const std::uint32_t active_wheel_groups =
        feature_mode_0x45bac0 < 2U ? 1U : 4U;
    const std::uint32_t slots_per_group =
        selected_pcar_ordinal_0x45c230_matches_car ? 40U : 16U;
    return active_wheel_groups * slots_per_group;
}

void reset_scene_wheel_resource_allocator(
    SceneWheelResourceAllocatorState& allocator) noexcept
{
    allocator.next_slot_index_0x0c = 0;
    for (auto& slot : allocator.slots) {
        if (slot.lifecycle_state_0x30 != scene_wheel_resource_slot_live) {
            slot.lifecycle_state_0x30 = scene_wheel_resource_slot_free;
        }
    }
}

SceneWheelResourceManagerDestroyPlan destroy_scene_wheel_resource_allocator(
    SceneWheelResourceAllocatorState& allocator) noexcept
{
    SceneWheelResourceManagerDestroyPlan result{};
    result.actions = {
        SceneWheelResourceManagerDestroyAction::release_texture_0x47d50,
        SceneWheelResourceManagerDestroyAction::free_slot_array_0x414420,
        SceneWheelResourceManagerDestroyAction::destroy_base_0x63370,
    };
    result.texture_handle = allocator.texture_handle_0x10;
    result.slot_count = allocator.slots.size();
    allocator.slots = {};
    allocator.next_slot_index_0x0c = 0;
    allocator.texture_handle_0x10 = 0;
    return result;
}

SceneRenderStateSnapshotRefreshPlan plan_scene_render_state_snapshot_refresh(
    const std::array<std::uint8_t, 24>& state_record,
    const std::array<std::uint8_t, 24>& cached_state,
    const bool dirty_flag_0x50d778)
{
    SceneRenderStateSnapshotRefreshPlan result{};
    result.state_pointer_0x50d74c = 0x00509B30U;
    result.cache_invalidation_applied = dirty_flag_0x50d778;
    result.cached_state_after = cached_state;
    if (!dirty_flag_0x50d778) {
        return result;
    }

    result.cached_state_after[0] = state_record[0] == 0 ? 1 : 0;
    result.cached_state_after[1] = state_record[1] == 0 ? 1 : 0;
    constexpr std::array<std::size_t, 11> inverted_offsets{
        8U, 9U, 10U, 11U, 12U, 13U, 14U, 0x14U, 0x15U, 0x16U, 0x17U};
    for (const std::size_t offset : inverted_offsets) {
        result.cached_state_after[offset] = state_record[offset] == 0 ? 1 : 0;
    }
    result.cached_state_after[0x10] = 0xFFU;
    result.cached_state_after[0x11] = 0xFFU;
    result.cached_state_after[0x12] = 0xFFU;
    result.cached_state_after[0x13] = 0xFFU;
    return result;
}

SceneRenderCallbackSelectionPlan plan_scene_render_callback_selection(
    const bool state_0x50d0a8,
    const SceneRenderCallbackSelectionObservation& observation)
{
    if (!state_0x50d0a8) {
        return {0x00455D90U, 0x00455D90U};
    }
    if (!observation.state_0x50d119) {
        if (observation.alternate_0x50d2dc) {
            return {0x00456660U, 0x00455F30U};
        }
        if (observation.alternate_0x50d118) {
            return {0x004563E0U, 0x00455F20U};
        }
        return {0x00456230U, 0x00455E00U};
    }
    if (observation.alternate_0x50d2dc) {
        return {0x00457090U, 0x00456220U};
    }
    if (observation.alternate_0x50d118) {
        return {0x00456D00U, 0x00456210U};
    }
    return {0x004569C0U, 0x00455F40U};
}

SceneRenderScanlineConversionPlan plan_scene_render_scanline_conversion(
    const std::uint32_t callback_address,
    const SceneRenderScanlineConversionObservation& observation)
{
    SceneRenderScanlineConversionPlan result{};
    result.callback_address = callback_address;
    result.destination_after.assign(observation.destination_before.begin(),
        observation.destination_before.end());
    const bool alpha_fill = callback_address == 0x00455D90U;
    const bool color_mask = callback_address == 0x00455E00U;
    const bool depth_fade = callback_address == 0x00455F40U;
    const bool palette_fade = callback_address == 0x004569C0U;
    const bool palette_lighting = callback_address == 0x00456230U;
    const bool dual_palette = callback_address == 0x004563E0U ||
        callback_address == 0x00456D00U;
    const bool distance_faded_dual_palette = callback_address == 0x00456D00U;
    const bool environment_pre_lit = callback_address == 0x00456660U;
    const bool palette_callback = palette_fade || palette_lighting || dual_palette ||
        environment_pre_lit;
    if (!alpha_fill && !color_mask && !depth_fade && !palette_callback) {
        result.status = SceneRenderScanlineConversionStatus::unsupported_callback;
        return result;
    }
    if ((color_mask || depth_fade || palette_callback) && observation.descriptor_mode > 2U) {
        result.status = SceneRenderScanlineConversionStatus::unsupported_descriptor_mode;
        return result;
    }

    if (observation.end_row <= observation.first_row) {
        return result;
    }
    if (observation.first_row < 0) {
        result.status = SceneRenderScanlineConversionStatus::negative_row_pointer;
        return result;
    }

    for (std::int32_t row = observation.first_row; row < observation.end_row; ++row) {
        const auto row_index = static_cast<std::size_t>(row);
        if (row_index >= observation.source_rows.size()) {
            result.status = SceneRenderScanlineConversionStatus::source_observation_exhausted;
            return result;
        }
        if (row_index >= result.destination_after.size()) {
            result.status = SceneRenderScanlineConversionStatus::destination_observation_exhausted;
            return result;
        }

        const auto& source = observation.source_rows[row_index];
        auto& destination = result.destination_after[row_index];
        const auto write = [&result, row_index, &destination](const std::uint8_t byte_offset,
            const std::uint32_t value) {
            result.writes.push_back({row_index, byte_offset, value});
            destination[byte_offset / sizeof(std::uint32_t)] = value;
        };

        // Both native loops copy words +0,+4,+8,+0x18,+0x1C in this order.
        write(0x00U, source[0]);
        write(0x04U, source[1]);
        write(0x08U, source[2]);
        write(0x18U, source[6]);
        write(0x1CU, source[7]);
        if (alpha_fill) {
            const std::uint32_t alpha_color =
                (static_cast<std::uint32_t>(observation.alpha_byte_0x50d110) << 24U) |
                0x00FFFFFFU;
            write(0x10U, alpha_color);
        } else if (palette_callback && observation.descriptor_mode == 2U) {
            write(0x10U, 0xFFFFFFFFU);
        } else if (palette_callback) {
            if (row_index >= observation.lighting_palette_indices.size()) {
                result.status = SceneRenderScanlineConversionStatus::lighting_index_observation_exhausted;
                return result;
            }
            const auto palette_index = static_cast<std::size_t>(
                observation.lighting_palette_indices[row_index]);
            if (palette_index >= observation.lighting_palette_words.size()) {
                result.status = SceneRenderScanlineConversionStatus::lighting_palette_observation_exhausted;
                return result;
            }
            write(0x10U, observation.lighting_palette_words[palette_index] |
                observation.color_or_mask_0x50d114);
        } else {
            write(0x10U, source[4] | observation.color_or_mask_0x50d114);
        }
        if (palette_lighting) {
            if (observation.descriptor_mode == 2U) {
                write(0x14U, 0xFF000000U);
            }
            ++result.rows_processed;
            ++result.row_counter_increments;
            continue;
        }
        if (dual_palette) {
            if (observation.descriptor_mode == 2U) {
                write(0x14U, 0xFF000000U);
            }
            if (row_index >= observation.secondary_lighting_indices.size()) {
                result.status = SceneRenderScanlineConversionStatus::secondary_lighting_index_observation_exhausted;
                return result;
            }
            const auto secondary_index = static_cast<std::size_t>(
                observation.secondary_lighting_indices[row_index]);
            if (secondary_index >= observation.secondary_lighting_words.size()) {
                result.status = SceneRenderScanlineConversionStatus::secondary_lighting_table_observation_exhausted;
                return result;
            }
            const auto secondary_word = observation.secondary_lighting_words[secondary_index];
            write(0x14U, secondary_word);
            if (distance_faded_dual_palette) {
                if (row_index >= observation.distance_fade_alpha_bytes.size()) {
                    result.status = SceneRenderScanlineConversionStatus::distance_fade_alpha_observation_exhausted;
                    return result;
                }
                const auto alpha_mask = static_cast<std::uint32_t>(
                    observation.distance_fade_alpha_bytes[row_index]) << 24U;
                write(0x14U, secondary_word | alpha_mask);
            }
            ++result.rows_processed;
            ++result.row_counter_increments;
            continue;
        }
        if (environment_pre_lit) {
            if (observation.descriptor_mode == 2U) {
                write(0x14U, 0xFF000000U);
            }
            if (!observation.environment_mapping_mode_2_enabled.has_value()) {
                result.status = SceneRenderScanlineConversionStatus::environment_mapping_mode_observation_exhausted;
                return result;
            }
            if (*observation.environment_mapping_mode_2_enabled) {
                if (row_index >= observation.transformed_tail_words.size()) {
                    result.status = SceneRenderScanlineConversionStatus::transformed_tail_observation_exhausted;
                    return result;
                }
                const auto& tail = observation.transformed_tail_words[row_index];
                write(0x18U, tail[0]);
                write(0x1CU, tail[1]);
                ++result.environment_mapping_transform_rows;
            }
            ++result.rows_processed;
            ++result.row_counter_increments;
            continue;
        }
        if ((depth_fade || palette_fade) && row_index >= observation.distance_fade_alpha_bytes.size()) {
            write(0x14U, 0U);
            result.status = SceneRenderScanlineConversionStatus::distance_fade_alpha_observation_exhausted;
            return result;
        }
        if (depth_fade || palette_fade) {
            write(0x14U, 0U);
            write(0x14U,
                static_cast<std::uint32_t>(observation.distance_fade_alpha_bytes[row_index]) << 24U);
            ++result.rows_processed;
            ++result.row_counter_increments;
            continue;
        }
        write(0x14U, 0xFF000000U);
        ++result.rows_processed;
        ++result.row_counter_increments;
    }
    return result;
}

std::optional<SceneRenderPanicForwarderPlan> plan_scene_render_panic_forwarder(
    const std::uint32_t callback_address) noexcept
{
    std::uint32_t format_pointer{};
    std::string_view format_text;
    switch (callback_address) {
    case 0x00455F20U:
        format_pointer = 0x004DB74CU;
        format_text = "light_pre_nofog_someenv: no environment mapping of prelit objs";
        break;
    case 0x00455F30U:
        format_pointer = 0x004DB78CU;
        format_text = "light_pre_nofog_env: no environment mapping of prelit objs";
        break;
    case 0x00456210U:
        format_pointer = 0x004DB7C8U;
        format_text = "light_pre_fog_someenv: no environment mapping of prelit objs";
        break;
    case 0x00456220U:
        format_pointer = 0x004DB808U;
        format_text = "light_pre_fog_env: no environment mapping of prelit objs";
        break;
    default: return std::nullopt;
    }
    return SceneRenderPanicForwarderPlan{callback_address, 0x00411000U, format_pointer,
        format_text};
}

SceneSharedRenderStateSyncPlan plan_scene_shared_render_state_sync(
    const SceneSharedRenderStateSyncObservation& observation)
{
    SceneSharedRenderStateSyncPlan result{};
    result.synchronization_gate_open = observation.synchronization_gate_0x44e400;
    result.cached_state_after = observation.cached_state;
    if (!observation.synchronization_gate_0x44e400) {
        return result;
    }

    const auto state_i32 = [](const std::array<std::uint8_t, 24>& state,
        const std::size_t offset) {
        const std::uint32_t raw = static_cast<std::uint32_t>(state[offset]) |
            (static_cast<std::uint32_t>(state[offset + 1]) << 8U) |
            (static_cast<std::uint32_t>(state[offset + 2]) << 16U) |
            (static_cast<std::uint32_t>(state[offset + 3]) << 24U);
        return std::bit_cast<std::int32_t>(raw);
    };
    const auto add_call = [&result](const std::uint32_t address,
        const std::int32_t argument_0 = 0, const std::int32_t argument_1 = 0,
        const std::int32_t argument_2 = 0, const std::uint8_t count = 1,
        const bool side_effect_gate = true) {
        result.calls.push_back({address, {argument_0, argument_1, argument_2}, count,
            side_effect_gate});
    };
    const auto& state = observation.desired_state;
    const auto& cached = observation.cached_state;
    bool changed = false;
    bool state_14_changed = false;

    if (state[0] != cached[0]) {
        changed = true;
        const std::int32_t mode = state[0] == 0 ? 1 : 2;
        add_call(0x00458040U, mode);
        const std::int32_t value = state[0] == 0 ? 1 :
            (observation.alternate_blend_factor_0x509b7d ? 4 : 2);
        add_call(0x00458180U, value);
    }
    if (state[0x14] != cached[0x14]) {
        changed = true;
        add_call(0x00457F60U, state[0x14] == 0 ? 3 : 2);
    }
    if (state[8] != cached[8]) {
        changed = true;
        add_call(0x00458080U, state[8] != 0 ? 1 : 0);
    }
    if (state[9] != cached[9]) {
        changed = true;
        add_call(0x004580C0U, state[9] != 0 ? 1 : 0);
    }
    if (state[10] != cached[10]) {
        changed = true;
        add_call(0x00458000U, state[10] != 0 ? 1 : 0);
    }
    if (state[11] != cached[11] || state[0x15] != cached[0x15]) {
        changed = true;
        const std::int32_t enabled = state[11] != 0 ? 1 : 0;
        const std::int32_t mode = state[0x15] != 0 ? 1 : 0;
        const std::int32_t first = enabled == 0 ? (mode == 0 ? 1 : 3) : (mode == 0 ? 2 : 4);
        add_call(0x00458120U, first, enabled == 0 ? 1 : 2, 0, 2);
        if (observation.alpha_reference_gate_0x509b7c) {
            const auto blend = mode == 0 ? 0 : std::bit_cast<std::int32_t>(0x3D4CCCCDU);
            add_call(0x00458220U, blend);
        }
        add_call(0x004581A0U, enabled, 0, 0, 1,
            observation.color_write_gate_0x509b7b);
    }
    if (state[0x0C] != cached[0x0C]) {
        changed = true;
        add_call(0x00458200U, state[0x0C] != 0 ? 1 : 0);
    }
    if (state[0x0D] != cached[0x0D]) {
        changed = true;
        add_call(0x00458060U, state[0x0D] != 0 ? 1 : 0);
    }
    if (state[0x0E] != cached[0x0E]) {
        changed = true;
        state_14_changed = true;
        const std::int32_t enabled = state[0x0E] != 0 ? 1 : 0;
        add_call(0x004581C0U, enabled);
        add_call(0x004581E0U, enabled);
    }

    const std::int32_t mode = state_i32(state, 0x10);
    if (mode != state_i32(cached, 0x10)) {
        changed = true;
        switch (mode) {
        case 0: add_call(0x00458150U, 5, 6, 0, 2); break;
        case 1: add_call(0x00458150U, 5, 2, 0, 2); break;
        case 2: add_call(0x00458150U, 9, 1, 0, 2); break;
        case 3: add_call(0x00458150U, 5, 1, 0, 2); break;
        case 4: add_call(0x00458150U, 2, 2, 0, 2); break;
        default:
            result.unsupported_mode_value = mode;
            add_call(0x00411000U, 0, 0, 0, 0);
            break;
        }
    }

    if (state[1] != cached[1] || (state[1] != 0 && state_14_changed)) {
        changed = true;
        const bool enabled = state[1] != 0 && state[0x0E] == 0;
        add_call(0x004580E0U, enabled ? 1 : 0);
    }
    if (state_i32(state, 4) != state_i32(cached, 4)) {
        changed = true;
        add_call(0x00458100U, state_i32(state, 4));
    }

    if (state[0x16] != cached[0x16] || state[0x17] != cached[0x17]) {
        changed = true;
        const bool primary = state[0x16] != 0;
        const bool secondary = state[0x17] != 0;
        if (!primary) {
            add_call(0x00457F80U, secondary ? 1 : 3, 0, 0, 3);
            add_call(0x00457FC0U, 3);
            add_call(0x00457FE0U, secondary ? 1 : 3);
        } else if (secondary) {
            add_call(0x00457F80U, 1, 0, 0, 3);
        } else {
            add_call(0x00457F80U, 1, 0, 0, 3);
            add_call(0x00457FC0U, 1);
            add_call(0x00457FE0U, 3);
        }
    }

    result.dirty_flag_cleared = true;
    if (changed) {
        result.cached_state_after = state;
        result.cache_snapshot_updated = true;
    }
    return result;
}

SceneWheelTextureBindingPlan plan_scene_wheel_texture_binding(
    const SceneWheelTextureBindingObservation& observation,
    const std::optional<SceneSharedRenderStateSyncObservation>& render_state_sync)
{
    SceneWheelTextureBindingPlan result{};
    result.cached_handle_after = observation.cached_handle;
    result.cache_dirty_after = observation.cache_dirty;
    result.texture_enabled_after = observation.texture_enabled_before;

    const auto add_action = [&result](const SceneWheelTextureBindingActionKind kind,
        const std::uint32_t address, const std::int32_t argument_0 = 0,
        const std::int32_t argument_1 = 0, const std::int32_t result_value = 0) {
        result.actions.push_back({kind, address, argument_0, argument_1, result_value});
    };
    const auto synchronize = [&]() {
        add_action(SceneWheelTextureBindingActionKind::synchronize_render_state, 0x00457AF0U);
        result.render_state_synchronization_called = true;
        if (render_state_sync.has_value()) {
            result.synchronized_states = plan_scene_shared_render_state_sync(*render_state_sync);
        }
    };
    const auto set_texture_enabled = [&result](const bool enabled) {
        result.texture_enabled_after = enabled;
    };

    const bool same_handle = observation.requested_handle == observation.cached_handle;
    if (same_handle && !observation.cache_dirty) {
        return result;
    }
    if (same_handle) {
        if (observation.force_texture_enabled || observation.requested_handle != -1) {
            if (observation.requested_handle == -1) {
                set_texture_enabled(true);
            } else if (!observation.surface_available.has_value()) {
                result.status = SceneWheelTextureBindingStatus::surface_observation_missing;
                result.texture_enabled_after.reset();
            } else {
                set_texture_enabled(*observation.surface_available);
            }
        } else {
            set_texture_enabled(false);
        }
        if (observation.requested_handle != -1 && !observation.force_texture_enabled) {
            add_action(SceneWheelTextureBindingActionKind::check_surface, 0x004540E0U,
                observation.requested_handle, 0,
                observation.surface_available.has_value()
                    ? (*observation.surface_available ? 1 : 0) : -1);
        }
        synchronize();
        result.cache_dirty_after = false;
        return result;
    }

    result.cached_handle_after = observation.requested_handle;
    if (observation.requested_handle == -1) {
        add_action(SceneWheelTextureBindingActionKind::set_render_state, 0x00458BC0U,
            1, 0, observation.set_render_state_hresult);
        if (observation.set_render_state_hresult != 0) {
            add_action(SceneWheelTextureBindingActionKind::report_device_error, 0x00452770U,
                observation.set_render_state_hresult);
        }
        set_texture_enabled(observation.force_texture_enabled);
        synchronize();
        return result;
    }

    if (!observation.resource_access_ready.has_value()) {
        result.status = SceneWheelTextureBindingStatus::resource_access_observation_missing;
        result.texture_enabled_after.reset();
        add_action(SceneWheelTextureBindingActionKind::ensure_resource, 0x00454030U,
            observation.requested_handle);
        synchronize();
        return result;
    }
    add_action(SceneWheelTextureBindingActionKind::ensure_resource, 0x00454030U,
        observation.requested_handle, 0, *observation.resource_access_ready ? 1 : 0);
    if (!*observation.resource_access_ready) {
        synchronize();
        return result;
    }
    if (!observation.force_texture_enabled) {
        if (!observation.surface_available.has_value()) {
            result.status = SceneWheelTextureBindingStatus::surface_observation_missing;
            result.texture_enabled_after.reset();
            add_action(SceneWheelTextureBindingActionKind::check_surface, 0x004540E0U,
                observation.requested_handle);
            synchronize();
            return result;
        }
        add_action(SceneWheelTextureBindingActionKind::check_surface, 0x004540E0U,
            observation.requested_handle, 0, *observation.surface_available ? 1 : 0);
        if (!*observation.surface_available) {
            set_texture_enabled(false);
            synchronize();
            return result;
        }
    }
    set_texture_enabled(true);
    synchronize();
    return result;
}

SceneWheelResourceFramePlan plan_scene_wheel_resource_frame(
    SceneWheelResourceAllocatorState& allocator,
    const bool frame_gate_0x45c380,
    const std::array<std::uint32_t, 4>& vertex_word_0x0c_observations,
    const std::optional<SceneWheelTextureBindingObservation>& texture_binding,
    const std::optional<SceneSharedRenderStateSyncObservation>& render_state_sync,
    const std::optional<SceneWheelResourceStateMaskObservation>& state_mask)
{
    SceneWheelResourceFramePlan result{};
    if (!frame_gate_0x45c380) {
        reset_scene_wheel_resource_allocator(allocator);
        result.reset_path = true;
        return result;
    }

    const auto append_call = [&result](
        const SceneWheelResourceFrameOperationKind kind,
        const std::uint32_t argument) {
        SceneWheelResourceFrameOperation operation{};
        operation.kind = kind;
        operation.argument = argument;
        result.operations.push_back(operation);
    };
    std::uint32_t current_state_mask = state_mask.has_value()
        ? state_mask->mask_at_0x509f50_plus_stride : 0U;
    auto state_record = state_mask.has_value()
        ? state_mask->state_record_0x509b30 : std::array<std::uint8_t, 24>{};
    auto cached_render_state = state_mask.has_value()
        ? state_mask->cached_render_state_0x50d760 : std::array<std::uint8_t, 24>{};
    bool texture_stage_enabled = state_mask.has_value() ? state_record[9] != 0 : false;
    const auto append_mask_call = [&](const SceneWheelResourceFrameOperationKind kind,
        const std::uint32_t argument) {
        append_call(kind, argument);
        if (!state_mask.has_value()) {
            return;
        }
        auto& operation = result.operations.back();
        operation.state_mask_index = state_mask->state_index_0x4da120;
        operation.state_mask_before = current_state_mask;
        operation.texture_stage_enabled_before = texture_stage_enabled;
        current_state_mask = kind == SceneWheelResourceFrameOperationKind::call_0x513a0
            ? current_state_mask | argument : current_state_mask & ~argument;
        if (argument == 0x100U) {
            const bool next_enabled = kind == SceneWheelResourceFrameOperationKind::call_0x513a0;
            if (next_enabled != texture_stage_enabled) {
                texture_stage_enabled = next_enabled;
                state_record[9] = next_enabled ? 1U : 0U;
                operation.state_snapshot_refresh_called = true;
                operation.state_toggle_notification_value = next_enabled;
                operation.state_snapshot_refresh = plan_scene_render_state_snapshot_refresh(
                    state_record, cached_render_state, state_mask->dirty_flag_0x50d778);
                cached_render_state = operation.state_snapshot_refresh->cached_state_after;
                if (state_mask->callback_selection_context.has_value()) {
                    operation.callback_selection = plan_scene_render_callback_selection(
                        next_enabled, *state_mask->callback_selection_context);
                }
            }
        }
        operation.state_mask_after = current_state_mask;
        operation.texture_stage_enabled_after = texture_stage_enabled;
    };
    append_mask_call(SceneWheelResourceFrameOperationKind::call_0x513a0, 8U);
    append_mask_call(SceneWheelResourceFrameOperationKind::call_0x51610, 0x100U);
    append_call(SceneWheelResourceFrameOperationKind::bind_texture_0x448e30,
        std::bit_cast<std::uint32_t>(allocator.texture_handle_0x10));
    auto& texture_bind = result.operations.back();
    texture_bind.texture_transform_state = 1;
    texture_bind.texture_transform_matrix_words = {
        0x3F800000U, 0U, 0U, 0U,
        0U, 0x3F800000U, 0U, 0U,
        0U, 0U, 0x3F800000U, 0U,
        0U, 0U, 0U, 0x3F800000U,
    };
    if (texture_binding.has_value()) {
        auto effective_render_state_sync = render_state_sync;
        if (state_mask.has_value() && effective_render_state_sync.has_value()) {
            effective_render_state_sync->desired_state = state_record;
            effective_render_state_sync->cached_state = cached_render_state;
        }
        result.texture_binding = plan_scene_wheel_texture_binding(
            *texture_binding, effective_render_state_sync);
    }

    constexpr std::array<std::array<std::uint32_t, 2>, 4> uv_bits{{
        {0x00000000U, 0x00000000U},
        {0x3F800000U, 0x00000000U},
        {0x00000000U, 0x3F800000U},
        {0x3F800000U, 0x3F800000U},
    }};
    constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 1, 3, 2};
    for (std::size_t slot_index = 0; slot_index < allocator.slots.size(); ++slot_index) {
        const auto& slot = allocator.slots[slot_index];
        if (slot.lifecycle_state_0x30 == scene_wheel_resource_slot_free) {
            continue;
        }
        SceneWheelResourceFrameOperation operation{};
        operation.kind = SceneWheelResourceFrameOperationKind::draw_indexed_0x448ed0;
        operation.slot_index = slot_index;
        operation.slot_state = slot.lifecycle_state_0x30;
        operation.index_order = indices;
        operation.primitive_type = 4;
        operation.vertex_type = 2;
        operation.vertex_count = 4;
        operation.index_count = 6;
        operation.flags = 8;
        operation.forwarding_wrapper = 0x00448ED0U;
        operation.device_wrapper = 0x00452700U;
        operation.device_vtable_offset = 0x78U;
        operation.device_return_ignored = true;
        for (std::size_t vertex = 0; vertex < 4; ++vertex) {
            for (std::size_t axis = 0; axis < 3; ++axis) {
                operation.vertex_words[vertex][axis] =
                    std::bit_cast<std::uint32_t>(slot.positions_0x00[vertex * 3 + axis]);
            }
            operation.vertex_words[vertex][3] = vertex_word_0x0c_observations[vertex];
            operation.vertex_words[vertex][4] = 0xFFFFFFFFU;
            operation.vertex_words[vertex][5] = 0xFFFFFFFFU;
            operation.vertex_words[vertex][6] = uv_bits[vertex][0];
            operation.vertex_words[vertex][7] = uv_bits[vertex][1];
        }
        result.operations.push_back(operation);
    }

    append_call(SceneWheelResourceFrameOperationKind::end_batch_0x448ef0, 0U);
    append_mask_call(SceneWheelResourceFrameOperationKind::call_0x513a0, 0x100U);
    append_mask_call(SceneWheelResourceFrameOperationKind::call_0x51610, 8U);
    return result;
}

SceneWheelResourceAllocation allocate_scene_wheel_resource(
    SceneWheelResourceAllocatorState& allocator,
    const std::uint32_t allocation_size) noexcept
{
    const auto slot_count = static_cast<std::int32_t>(allocator.slots.size());
    if (slot_count <= 0 || allocator.next_slot_index_0x0c < 0) {
        return {};
    }

    std::int32_t candidate = allocator.next_slot_index_0x0c;
    for (std::int32_t scanned = 0; scanned < slot_count; ++scanned) {
        if (candidate >= slot_count) {
            candidate -= slot_count;
        }
        auto& slot = allocator.slots[static_cast<std::size_t>(candidate)];
        if (slot.lifecycle_state_0x30 != scene_wheel_resource_slot_live) {
            allocator.next_slot_index_0x0c = candidate + 1;
            slot.lifecycle_state_0x30 = scene_wheel_resource_slot_live;
            slot.allocation_size_0x34 = allocation_size;
            return {true, static_cast<std::size_t>(candidate)};
        }
        ++candidate;
    }
    return {};
}

SceneWheelAuxiliaryUpdateResult update_scene_wheel_auxiliary(
    SceneWheelAuxiliaryState& state,
    const SceneWheelAuxiliaryUpdateInput& input) noexcept
{
    constexpr float timer_step = 0.033333335F; // 0x4C9194
    constexpr float timer_release_threshold = std::bit_cast<float>(std::uint32_t{0x3E124925});
    constexpr float movement_threshold = 0.48999998F; // 0x4C919C
    constexpr float cross_product_epsilon = 1.1920929e-7F; // 0x4C91A0
    constexpr float offset_scale = 0.1F; // 0x4C9140
    SceneWheelAuxiliaryUpdateResult result{};
    const auto store_resource_positions = [&]() {
        if (input.allocator != nullptr && state.resource_slot_known
            && state.resource_slot_index < input.allocator->slots.size()) {
            std::copy(state.resource_positions.begin(), state.resource_positions.end(),
                input.allocator->slots[state.resource_slot_index].positions_0x00.begin());
        }
    };

    if (state.resource_present_0x18 && input.allocator != nullptr
        && state.resource_slot_known
        && state.resource_slot_index < input.allocator->slots.size()) {
        const auto& slot = input.allocator->slots[state.resource_slot_index];
        std::copy(slot.positions_0x00.begin(), slot.positions_0x00.end(),
            state.resource_positions.begin());
        state.resource_lifecycle_state_0x30 = slot.lifecycle_state_0x30;
    }

    if (!state.resource_present_0x18) {
        state.transition_active_0x28 = true;
        state.elapsed_0x20 = 0.0F;
        state.start_0x00 = input.target_position;
        state.end_0x0c = input.target_position;
    } else {
        state.elapsed_0x20 += timer_step;
        std::array<float, 3> target_delta{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            target_delta[axis] = state.start_0x00[axis] - input.target_position[axis];
        }
        const long double distance_squared =
            static_cast<long double>(target_delta[2]) * target_delta[2]
            + static_cast<long double>(target_delta[1]) * target_delta[1]
            + static_cast<long double>(target_delta[0]) * target_delta[0];
        if (state.elapsed_0x20 > timer_release_threshold
            && distance_squared > static_cast<long double>(movement_threshold)) {
            state.transition_active_0x28 = false;
            state.elapsed_0x20 = 0.0F;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                state.start_0x00[axis] = state.resource_positions[6 + axis];
                state.end_0x0c[axis] = state.resource_positions[9 + axis];
            }
            state.resource_lifecycle_state_0x30 = scene_wheel_resource_slot_retired;
            if (input.allocator != nullptr && state.resource_slot_known
                && state.resource_slot_index < input.allocator->slots.size()) {
                input.allocator->slots[state.resource_slot_index].lifecycle_state_0x30 =
                    scene_wheel_resource_slot_retired;
            }
            state.resource_present_0x18 = false;
            result.resource_released = true;
        }
    }

    if (!state.resource_present_0x18) {
        result.allocation_attempted = true;
        bool allocation_succeeded = input.allocation_returns_nonnull;
        if (input.allocator != nullptr) {
            const auto allocation = allocate_scene_wheel_resource(
                *input.allocator, state.allocation_size_0x24);
            allocation_succeeded = allocation.allocated;
            state.resource_slot_index = allocation.slot_index;
            state.resource_slot_known = allocation.allocated;
        } else {
            state.resource_slot_known = false;
        }
        if (!allocation_succeeded) {
            return result;
        }
        state.resource_present_0x18 = true;
        state.resource_lifecycle_state_0x30 = scene_wheel_resource_slot_live;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            state.resource_positions[axis] = state.start_0x00[axis];
            state.resource_positions[3 + axis] = state.end_0x0c[axis];
            state.resource_positions[6 + axis] = state.start_0x00[axis];
            state.resource_positions[9 + axis] = state.end_0x0c[axis];
        }
        store_resource_positions();
        return result;
    }

    const std::array<float, 3> delta{
        input.target_position[0] - state.resource_positions[0],
        input.target_position[1] - state.resource_positions[1],
        input.target_position[2] - state.resource_positions[2],
    };
    const auto& axis = input.orientation_vector;
    const std::array<float, 3> cross{
        static_cast<float>(static_cast<long double>(axis[2]) * delta[1]
            - static_cast<long double>(axis[1]) * delta[2]),
        static_cast<float>(static_cast<long double>(axis[0]) * delta[2]
            - static_cast<long double>(axis[2]) * delta[0]),
        static_cast<float>(static_cast<long double>(axis[1]) * delta[0]
            - static_cast<long double>(axis[0]) * delta[1]),
    };
    const long double magnitude_wide = std::sqrt(
        static_cast<long double>(cross[1]) * cross[1]
        + static_cast<long double>(cross[2]) * cross[2]
        + static_cast<long double>(cross[0]) * cross[0]);
    const float magnitude = static_cast<float>(magnitude_wide);
    if (!(std::fabs(magnitude) > cross_product_epsilon)) {
        return result;
    }

    result.normalized_cross_product_used = true;
    const auto normalized_component = [&](const float component) {
        const long double quotient = divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(component),
            static_cast<long double>(magnitude),
            input.fdiv_compatibility_enabled);
        return static_cast<float>(quotient * static_cast<long double>(offset_scale));
    };
    const std::array<float, 3> offset{
        normalized_component(cross[0]),
        normalized_component(cross[1]),
        normalized_component(cross[2]),
    };
    if (state.transition_active_0x28) {
        state.transition_active_0x28 = false;
        for (std::size_t component = 0; component < 3; ++component) {
            state.start_0x00[component] = state.resource_positions[component] - offset[component];
            state.end_0x0c[component] = state.resource_positions[component] + offset[component];
            state.resource_positions[component] = state.start_0x00[component];
            state.resource_positions[3 + component] = state.end_0x0c[component];
        }
    }
    for (std::size_t component = 0; component < 3; ++component) {
        state.resource_positions[6 + component] = input.target_position[component] - offset[component];
        state.resource_positions[9 + component] = input.target_position[component] + offset[component];
    }
    store_resource_positions();
    return result;
}

SceneWheelAuxiliaryDispatchResult dispatch_scene_wheel_auxiliary(
    SceneWheelAuxiliaryState& state,
    const SceneWheelAuxiliaryDispatchInput& input) noexcept
{
    SceneWheelAuxiliaryDispatchResult result{};
    if (!input.world_gate_0x45bac0 || input.scene_car_ordinal_0x1c8 < 0) {
        return result;
    }

    if (!input.effect_gate_0x45c380) {
        result.call = SceneWheelAuxiliaryCall::release_0x462960;
    } else {
        if (input.suppress_gate_0x425830) {
            return result;
        }
        if (input.wheel_record_flag_0x34_bit0) {
            result.call = SceneWheelAuxiliaryCall::update_0x462600;
            result.update = update_scene_wheel_auxiliary(state, input.update);
            result.resource_released = result.update.resource_released;
            return result;
        }
        result.call = SceneWheelAuxiliaryCall::release_0x462940;
    }

    if (state.resource_present_0x18) {
        state.resource_lifecycle_state_0x30 = scene_wheel_resource_slot_retired;
        if (input.update.allocator != nullptr && state.resource_slot_known
            && state.resource_slot_index < input.update.allocator->slots.size()) {
            input.update.allocator->slots[state.resource_slot_index].lifecycle_state_0x30 =
                scene_wheel_resource_slot_retired;
        }
        state.resource_present_0x18 = false;
        result.resource_released = true;
    }
    return result;
}

SceneCarWheelFramePlan plan_scene_car_wheel_frame(
    const SceneCarWheelFrameInput& input,
    const std::array<SceneWheelAuxiliaryState, scene_car_wheel_count>& initial_state) noexcept
{
    SceneCarWheelFramePlan result{};
    result.resulting_auxiliary_state = initial_state;
    for (std::size_t wheel = 0; wheel < scene_car_wheel_count; ++wheel) {
        auto transform = input.transforms[wheel];
        transform.wheel_index = wheel;
        result.transforms[wheel] = transform_scene_car_wheel(transform);
        result.auxiliary[wheel] = dispatch_scene_wheel_auxiliary(
            result.resulting_auxiliary_state[wheel], input.auxiliary[wheel]);
    }
    return result;
}

SceneCarCalloutPlan plan_scene_car_callout(const SceneCarCalloutInput& input)
{
    SceneCarCalloutPlan result{};
    if (input.object_ordinal == input.selected_car_ordinal) {
        result.status = SceneCarCalloutStatus::selected_car_suppressed;
        return result;
    }

    const double delta_x = static_cast<double>(input.camera_position[0])
        - input.object_position[0];
    const double delta_y = static_cast<double>(input.camera_position[1])
        - input.object_position[1];
    const double delta_z = static_cast<double>(input.camera_position[2])
        - input.object_position[2];
    result.squared_distance = delta_y * delta_y + delta_z * delta_z + delta_x * delta_x;
    // The native C0 test skips when C0 is clear. X87 unordered comparisons set
    // C0, so NaN follows the projection path just like the original branch.
    if (result.squared_distance >= input.squared_distance_threshold) {
        result.status = SceneCarCalloutStatus::outside_distance_threshold;
        return result;
    }

    result.operations.push_back({
        SceneCarCalloutOperationKind::project_world_position_0x448cd0,
        0x00448CD0U});
    if (!input.projection_visible_0x448cd0) {
        result.status = SceneCarCalloutStatus::projection_rejected;
        return result;
    }

    result.operations.push_back({
        SceneCarCalloutOperationKind::set_text_context_0x449900,
        0x00449900U});
    if (!input.record_observed) {
        result.status = SceneCarCalloutStatus::record_observation_missing;
        return result;
    }

    if (input.primary_text_gate_0x49be40) {
        result.operations.push_back({
            SceneCarCalloutOperationKind::draw_primary_text_0x4d710,
            0x0044D710U, 0x004DCDA4U, 5, 0x140,
            input.primary_text_x_after_ftol, input.primary_text_y_after_ftol});
    }

    result.operations.push_back({
        SceneCarCalloutOperationKind::check_secondary_text_gate_0x40d250,
        0x0040D250U});
    if (input.secondary_global_a_0x4d10fc
        && input.secondary_global_b_0x4ecc98
        && input.secondary_record_text_present) {
        result.operations.push_back({
            SceneCarCalloutOperationKind::draw_secondary_resource_0x44d0c0,
            0x0044D0C0U, 0U, 0x7B4, 0,
            input.secondary_text_x_after_ftol,
            input.secondary_text_y_after_ftol});
        result.operations.push_back({
            SceneCarCalloutOperationKind::draw_secondary_text_0x4d710,
            0x0044D710U, 0x0053F194U, 0x32, 0,
            input.secondary_text_x_after_ftol,
            input.secondary_text_y_after_ftol});
    }
    return result;
}

} // namespace viper
