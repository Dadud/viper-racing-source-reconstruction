#include "viper/scene_car_update.hpp"

#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

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

SceneWheelResourceFramePlan plan_scene_wheel_resource_frame(
    SceneWheelResourceAllocatorState& allocator,
    const bool frame_gate_0x45c380,
    const std::array<std::uint32_t, 4>& vertex_word_0x0c_observations)
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
    append_call(SceneWheelResourceFrameOperationKind::call_0x513a0, 8U);
    append_call(SceneWheelResourceFrameOperationKind::call_0x51610, 0x100U);
    append_call(SceneWheelResourceFrameOperationKind::bind_texture_0x48e30,
        std::bit_cast<std::uint32_t>(allocator.texture_handle_0x10));

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
    append_call(SceneWheelResourceFrameOperationKind::call_0x513a0, 0x100U);
    append_call(SceneWheelResourceFrameOperationKind::call_0x51610, 8U);
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

} // namespace viper
