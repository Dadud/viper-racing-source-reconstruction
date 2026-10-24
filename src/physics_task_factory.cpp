#include "viper/physics_task_factory.hpp"
#include "viper/legacy_fdiv.hpp"

#include <array>
#include <bit>
#include <cmath>

namespace viper {
namespace {

constexpr std::array<PhysicsTaskObjectFactoryDescriptor, 9> descriptors{{
    {0x42414c4cU, 0x0480U, 0x0043abc0U}, // BALL
    {0x41434152U, 0x1080U, 0x004285d0U}, // ACAR
    {0x47434152U, 0x0f00U, 0x004278f0U}, // GCAR
    {0x43484b50U, 0x0084U, 0x0043a620U}, // CHKP
    {0x4f425354U, 0x04b0U, 0x00436860U}, // OBST
    {0x4e434152U, 0x0f38U, 0x00438e40U}, // NCAR
    {0x50434152U, 0x0ef0U, 0x004381e0U}, // PCAR
    {0x53544154U, 0x006cU, 0x00437c60U}, // STAT
    {0x574f424cU, 0x04b4U, 0x00436ed0U}, // WOBL
}};

std::optional<std::uint32_t> known_physics_command_payload_bytes(
    const std::uint8_t command_id) noexcept
{
    switch (command_id) {
    case 1U: return 20U;
    case 3U: return 28U;
    case 4U: return 44U;
    case 5U: return 14U;
    case 7U: return 2U;
    default: return std::nullopt;
    }
}

std::uint32_t read_little_endian_u32(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

} // namespace

PhysicsCommandHandlerRegistryPlan plan_physics_command_handler_registry(
    const PhysicsCommandHandlerRegistryObservation& observation)
{
    PhysicsCommandHandlerRegistryPlan plan{};
    plan.handlers = observation.initial_handlers;

    for (const auto& registration : observation.registrations) {
        if (registration.command_id >= plan.handlers.size()) {
            plan.out_of_range_registration_write = true;
            continue;
        }
        auto& handler = plan.handlers[registration.command_id];
        if (handler == 1U) ++plan.existing_value_one_observations;
        handler = registration.handler_address;
        ++plan.registrations_applied;
    }

    for (const auto handler_address : observation.unregistrations) {
        bool found = false;
        for (std::size_t command_id = 0; command_id < plan.handlers.size();
             ++command_id) {
            if (plan.handlers[command_id] != handler_address) continue;
            plan.handlers[command_id] = 0U;
            plan.unregistered_command_ids.push_back(
                static_cast<std::uint32_t>(command_id));
            ++plan.removals_applied;
            found = true;
            break;
        }
        if (!found) ++plan.missing_removal_reporters_called;
    }
    return plan;
}

std::array<std::uint32_t, 8>
physics_command_handlers_registered_by_track_initializers() noexcept
{
    std::array<std::uint32_t, 8> handlers{};
    handlers[1] = 0x00436190U;
    handlers[3] = 0x00436170U;
    handlers[5] = 0x00430fb0U;
    handlers[7] = 0x00431110U;
    return handlers;
}

PhysicsTimeCommandPayloadPlan plan_physics_time_command_payload(
    const PhysicsTimeCommandPayloadObservation& observation) noexcept
{
    PhysicsTimeCommandPayloadPlan plan{};
    const auto consumed_bytes =
        known_physics_command_payload_bytes(observation.command_id);
    if (!consumed_bytes.has_value()) return plan;
    plan.consumed_payload_bytes = *consumed_bytes;
    if (observation.payload.size() < *consumed_bytes) {
        plan.status = PhysicsTimeCommandPayloadStatus::incomplete_payload;
        return plan;
    }
    plan.status = PhysicsTimeCommandPayloadStatus::complete;

    switch (observation.command_id) {
    case 1U:
        plan.record_ring_index_used = observation.record_ring_cursor;
        plan.record_ring_index_out_of_range =
            std::bit_cast<std::int32_t>(observation.record_ring_cursor) < 0 ||
            observation.record_ring_cursor >= 8U;
        plan.record_ring_cursor_after = std::bit_cast<std::uint32_t>(
            std::bit_cast<std::int32_t>(observation.record_ring_cursor + 1U) % 8);
        plan.record_ring_update_callback_called = true;
        if (plan.record_ring_update_callback_called) {
            plan.record_vector_words = {
                read_little_endian_u32(observation.payload, 4U),
                read_little_endian_u32(observation.payload, 8U),
                read_little_endian_u32(observation.payload, 12U)};
            plan.record_scalar_bits =
                read_little_endian_u32(observation.payload, 16U);
            if (observation.record_ring_slot_nested_target_nonnull) {
                plan.record_ring_entry_vector_write_applied = true;
                plan.record_ring_target_flag_0x29_set = true;
                plan.record_ring_target_flag_0x2b_cleared = true;
                plan.record_ring_target_flag_0x2a_cleared = true;
            }
            if (observation.record_ring_slot_nested_target_nonnull &&
                observation.record_ring_target_scalar_before_bits_0x08) {
                const auto before = std::bit_cast<float>(
                    *observation.record_ring_target_scalar_before_bits_0x08);
                const auto incoming = std::bit_cast<float>(plan.record_scalar_bits);
                const bool changed = before != incoming &&
                    !std::isunordered(before, incoming);
                plan.record_ring_target_scalar_changed = changed;
                plan.record_ring_target_change_flag_0x2c_set = changed;
                plan.record_ring_target_scalar_after_bits_0x08 = changed
                    ? plan.record_scalar_bits
                    : *observation.record_ring_target_scalar_before_bits_0x08;
            }
        }
        break;
    case 3U: {
        plan.event_buffer_append_attempted = true;
        plan.event_buffer_record_type = 0U;
        const auto new_size = observation.event_buffer_size + 29U;
        plan.event_buffer_append_succeeded = new_size < 0x400U;
        if (plan.event_buffer_append_succeeded) {
            plan.event_buffer_size_after = new_size;
            for (std::size_t index = 0; index <
                 plan.event_buffer_payload.size(); ++index) {
                plan.event_buffer_payload[index] = observation.payload[index];
            }
            plan.event_buffer_payload_known = true;
        } else {
            plan.event_buffer_size_after = observation.event_buffer_size;
        }
        break;
    }
    case 4U: {
        const auto field = read_little_endian_u32(observation.payload, 0U);
        plan.id4_validation_panic_called = field != 0U && field != 1U;
        break;
    }
    case 5U:
    case 7U: {
        const auto payload_index = observation.command_id == 5U ? 13U : 0U;
        const auto raw_index = observation.payload[payload_index];
        const auto signed_index = static_cast<std::int32_t>(
            static_cast<std::int8_t>(raw_index));
        plan.registered_object_index = signed_index;
        if (signed_index < 0 || signed_index >= static_cast<std::int32_t>(
                observation.registered_object_addresses_0x507d70.size())) {
            plan.status = PhysicsTimeCommandPayloadStatus::
                registered_object_index_out_of_range;
            plan.unguarded_registered_object_call_would_fault = true;
            return plan;
        }
        plan.registered_object_address =
            observation.registered_object_addresses_0x507d70[
            static_cast<std::size_t>(signed_index)];
        if (plan.registered_object_address == 0U) {
            plan.status = PhysicsTimeCommandPayloadStatus::null_registered_object;
            plan.unguarded_registered_object_call_would_fault = true;
            plan.reset_callback_0x433920_called =
                observation.command_id == 7U ||
                observation.secondary_context == 2;
            return plan;
        }
        if (observation.command_id == 5U && observation.secondary_context == 2) {
            plan.reset_callback_0x433920_called = true;
        } else if (observation.command_id == 5U) {
            plan.virtual_0x3c_called = true;
            plan.command5_vectors_decoded = true;
            for (std::size_t index = 0; index < 3U; ++index) {
                plan.command5_byte_components[index] = static_cast<float>(
                    static_cast<long double>(observation.payload[index])
                        * 10.0L / 128.0L - 5.0L);
                const auto offset = 3U + index * 2U;
                const auto encoded = static_cast<std::uint16_t>(
                    observation.payload[offset]
                    | (static_cast<std::uint16_t>(
                           observation.payload[offset + 1U]) << 8U));
                plan.command5_word_components[index] = static_cast<float>(
                    static_cast<long double>(encoded) * 30000.0L / 32768.0L
                    - 15000.0L);
            }
        } else {
            plan.reset_callback_0x433920_called = true;
        }
        break;
    }
    default:
        break;
    }
    return plan;
}

PhysicsAuxiliaryPairPlan plan_physics_auxiliary_pair(
    const PhysicsAuxiliaryPairObservation& observation) noexcept
{
    PhysicsAuxiliaryPairPlan plan{};
    plan.receiver_accumulator_after = observation.receiver_accumulator_0xdfc;
    if (!observation.target_object_nonnull) {
        plan.status = PhysicsAuxiliaryPairStatus::target_object_null_dereference;
        return plan;
    }

    plan.target_virtual_0x30_called = true;
    if (!observation.target_virtual_0x30_result) {
        plan.status = PhysicsAuxiliaryPairStatus::inactive_target;
        return plan;
    }

    for (std::size_t axis = 0; axis < plan.delta.size(); ++axis) {
        plan.delta[axis] = observation.target_position_0xe08[axis]
            - observation.receiver_position_0x5c[axis];
    }

    const auto x = static_cast<long double>(plan.delta[0]);
    const auto y = static_cast<long double>(plan.delta[1]);
    const auto z = static_cast<long double>(plan.delta[2]);
    const auto distance_squared_extended = y * y + z * z + x * x;
    plan.distance_squared = static_cast<float>(distance_squared_extended);
    const auto radius = static_cast<long double>(
        observation.target_radius_0xe14);
    const auto radius_squared_extended = radius * radius;
    plan.radius_squared = static_cast<float>(radius_squared_extended);
    if (!(radius_squared_extended
          > static_cast<long double>(plan.distance_squared))) {
        plan.status = PhysicsAuxiliaryPairStatus::outside_radius;
        return plan;
    }

    plan.proximity_gate_passed = true;
    plan.status = PhysicsAuxiliaryPairStatus::updated;
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;
    const auto numerator = static_cast<long double>(plan.radius_squared)
        - static_cast<long double>(plan.distance_squared);
    plan.proximity_factor = divide_with_vc98_fdiv_compatibility(
        numerator, static_cast<long double>(plan.radius_squared),
        observation.fdiv_compatibility_enabled);
    for (std::size_t axis = 0; axis < plan.receiver_accumulator_after.size();
         ++axis) {
        plan.receiver_accumulator_after[axis] = static_cast<float>(
            static_cast<long double>(observation.target_force_0x234[axis])
                * plan.proximity_factor
            + static_cast<long double>(
                observation.receiver_accumulator_0xdfc[axis]));
    }
    return plan;
}

PhysicsAuxiliaryPassPlan plan_physics_auxiliary_pass(
    const PhysicsAuxiliaryPassObservation& observation)
{
    PhysicsAuxiliaryPassPlan plan{};
    plan.receiver_accumulators_after = observation.receiver_accumulators_0xdfc;
    if (observation.alternate_time_path) {
        plan.alternate_time_path_delegated = true;
        plan.unused_pair_observations = static_cast<std::uint32_t>(
            observation.pair_calls.size());
        plan.returned_normally = true;
        return plan;
    }

    std::vector<bool> matched_observations(observation.pair_calls.size(), false);
    for (std::size_t receiver = 0; receiver <
         observation.object_pointers_0x507d70.size(); ++receiver) {
        if (observation.object_pointers_0x507d70[receiver] == 0U) continue;
        for (std::size_t target = 0; target <
             observation.object_pointers_0x507d70.size(); ++target) {
            if (target == receiver ||
                observation.object_pointers_0x507d70[target] == 0U) {
                continue;
            }

            ++plan.expected_pair_calls;
            PhysicsAuxiliaryPairDispatchStep step{};
            step.receiver_index = receiver;
            step.target_index = target;

            const PhysicsAuxiliaryPairCallObservation* matched_call = nullptr;
            std::size_t match_count = 0U;
            for (std::size_t call_index = 0U;
                 call_index < observation.pair_calls.size(); ++call_index) {
                const auto& candidate = observation.pair_calls[call_index];
                if (candidate.receiver_index == receiver &&
                    candidate.target_index == target) {
                    matched_call = &candidate;
                    matched_observations[call_index] = true;
                    ++match_count;
                }
            }

            if (match_count == 0U) {
                step.observation_missing = true;
                ++plan.missing_pair_observations;
                plan.receiver_accumulators_after[receiver].reset();
                plan.accumulator_dependency_unknown = true;
                plan.ordered_pair_calls.push_back(step);
                continue;
            }
            plan.pair_observations_matched += static_cast<std::uint32_t>(
                match_count);
            if (match_count != 1U) {
                step.observation_duplicated = true;
                ++plan.duplicate_pair_observations;
                plan.receiver_accumulators_after[receiver].reset();
                plan.accumulator_dependency_unknown = true;
                plan.ordered_pair_calls.push_back(step);
                continue;
            }
            if (!plan.receiver_accumulators_after[receiver].has_value()) {
                plan.accumulator_dependency_unknown = true;
                plan.ordered_pair_calls.push_back(step);
                continue;
            }

            step.receiver_accumulator_known = true;
            auto pair = matched_call->pair;
            pair.receiver_accumulator_0xdfc =
                *plan.receiver_accumulators_after[receiver];
            step.pair_effect = plan_physics_auxiliary_pair(pair);
            if (step.pair_effect->status ==
                PhysicsAuxiliaryPairStatus::target_object_null_dereference) {
                plan.target_dereference_fault = true;
                plan.ordered_pair_calls.push_back(step);
                for (const bool matched : matched_observations) {
                    if (!matched) ++plan.unused_pair_observations;
                }
                return plan;
            }

            plan.receiver_accumulators_after[receiver] =
                step.pair_effect->receiver_accumulator_after;
            plan.ordered_pair_calls.push_back(step);
        }
    }

    for (const bool matched : matched_observations) {
        if (!matched) ++plan.unused_pair_observations;
    }
    plan.returned_normally = true;
    return plan;
}

std::vector<PhysicsTaskGlobalStore>
physics_task_initialization_global_stores()
{
    std::vector<PhysicsTaskGlobalStore> stores;
    stores.reserve(61);
    const auto zero_range = [&stores](const std::uint32_t address,
                                      const std::uint32_t count) {
        for (std::uint32_t index = 0; index < count; ++index) {
            stores.push_back({address + index * 4U, 0U});
        }
    };
    const auto identity_3x4 = [&stores](const std::uint32_t address) {
        for (std::uint32_t index = 0; index < 12U; ++index) {
            const bool diagonal = index == 0U || index == 4U || index == 8U;
            stores.push_back({address + index * 4U,
                              diagonal ? 0x3f800000U : 0U});
        }
    };

    stores.push_back({0x005081e4U, 0U});
    zero_range(0x00507d70U, 16U);
    stores.push_back({0x004d4880U, 0U});
    stores.push_back({0x005081e8U, 0U});
    stores.push_back({0x00508238U, 0U});
    identity_3x4(0x00507dc8U);
    zero_range(0x00508640U, 3U);
    identity_3x4(0x00507d10U);
    identity_3x4(0x005081f8U);
    stores.push_back({0x00508199U, 1U});
    stores.push_back({0x00508198U, 0U});
    return stores;
}

std::vector<PhysicsTaskDispatchEvent> physics_task_fixed_step_dispatch_order(
    const std::span<const PhysicsTaskEntityObservation> objects,
    const std::array<bool, 16>& auxiliary_objects,
    const bool alternate_time_path)
{
    std::vector<PhysicsTaskDispatchEvent> events;
    events.reserve(objects.size() * 3U + 1U + 256U);
    if (!alternate_time_path) {
        for (std::size_t index = 0; index < objects.size(); ++index) {
            if (!objects[index].nonnull) continue;
            events.push_back({PhysicsTaskDispatchEventKind::virtual_0x2c,
                              index, 0U});
            if (objects[index].virtual_0x2c_result) {
                events.push_back({PhysicsTaskDispatchEventKind::virtual_0x08,
                                  index, 0U});
            }
        }
        events.push_back({PhysicsTaskDispatchEventKind::time_slice_helper_0x427080,
                          0U, 0U});
        for (std::size_t first = 0; first < auxiliary_objects.size(); ++first) {
            if (!auxiliary_objects[first]) continue;
            for (std::size_t second = 0; second < auxiliary_objects.size(); ++second) {
                if (second == first || !auxiliary_objects[second]) continue;
                events.push_back({
                    PhysicsTaskDispatchEventKind::auxiliary_pair_helper_0x432300,
                    first,
                    second});
            }
        }
    } else {
        events.push_back({PhysicsTaskDispatchEventKind::alternate_time_helper_0x426a10,
                          0U, 0U});
    }

    for (std::size_t index = 0; index < objects.size(); ++index) {
        if (!objects[index].nonnull) continue;
        events.push_back({PhysicsTaskDispatchEventKind::virtual_0x0c,
                          index, 0U});
    }
    return events;
}

PhysicsTimeRingTransitionPlan plan_physics_time_ring_transition(
    const PhysicsTimeRingTransitionObservation& observation)
{
    PhysicsTimeRingTransitionPlan plan{};
    plan.slot_offsets.assign(observation.slot_offsets.begin(),
                             observation.slot_offsets.end());
    plan.cursor_after = observation.current_cursor;
    plan.high_water_mark_after = observation.high_water_mark;
    plan.generation_after = observation.generation;
    plan.overflow_generation_after = observation.overflow_generation;

    if (observation.next_slot < 0
        || static_cast<std::size_t>(observation.next_slot) >= plan.slot_offsets.size()
        || (observation.current_slot != -1
            && (observation.current_slot < 0
                || static_cast<std::size_t>(observation.current_slot)
                    >= plan.slot_offsets.size()))) {
        plan.status = PhysicsTimeRingTransitionStatus::invalid_slot_index;
        return plan;
    }

    const auto next_index = static_cast<std::size_t>(observation.next_slot);
    if (observation.current_slot == -1) {
        plan.slot_offsets[next_index] = 0U;
        plan.cursor_after = observation.chunk_size;
        return plan;
    }

    const auto current_index = static_cast<std::size_t>(observation.current_slot);
    const auto current_offset = plan.slot_offsets[current_index];
    plan.cleanup_byte_address = observation.storage_base + current_offset
        + observation.current_cursor;
    plan.cleanup_byte_written = true;
    const auto cursor_after_marker = observation.current_cursor + 1U;
    plan.cursor_after = cursor_after_marker;

    const auto end_offset_bits = current_offset + cursor_after_marker;
    const auto overflow_check_bits = end_offset_bits + 0x600U;
    plan.overflow_detected = std::bit_cast<std::int32_t>(overflow_check_bits)
        > 0x1fffff;
    const bool generation_advance = observation.next_slot == 0;
    if (!generation_advance && !plan.overflow_detected) {
        plan.slot_offsets[next_index] = end_offset_bits;
        plan.cursor_after = observation.chunk_size;
        return plan;
    }

    plan.position_update_flag_0x508a54_set = true;
    const auto end_offset_signed = std::bit_cast<std::int32_t>(end_offset_bits);
    if (end_offset_signed > plan.high_water_mark_after) {
        plan.high_water_mark_after = end_offset_signed;
    }

    if (plan.overflow_detected) {
        plan.overflow_generation_after = observation.generation;
        plan.slot_offsets[next_index] = 0U;
    } else if (!generation_advance
               || observation.overflow_generation == observation.generation) {
        plan.slot_offsets[next_index] = end_offset_bits;
    } else {
        plan.slot_offsets[next_index] = 0U;
    }

    if (generation_advance) ++plan.generation_after;
    plan.cursor_after = observation.chunk_size;
    return plan;
}

PhysicsTimeSlicePlan plan_physics_time_slice(
    const PhysicsTimeSliceObservation& observation)
{
    PhysicsTimeSlicePlan plan{};
    if (observation.slot_count == 0) {
        plan.status = PhysicsTimeSliceStatus::division_by_zero;
        return plan;
    }

    const auto divided_counter = observation.simulation_counter / 6;
    plan.selected_slot = divided_counter % observation.slot_count;
    if (observation.slot_count < 0 || plan.selected_slot < 0
        || static_cast<std::size_t>(plan.selected_slot) >= observation.slot_offsets.size()) {
        plan.status = PhysicsTimeSliceStatus::invalid_slot_index;
        return plan;
    }

    std::uint32_t selected_offset{};
    if (plan.selected_slot != observation.current_slot) {
        plan.transition_helper_0x4272c0_called = true;
        ++plan.sync_helper_calls;
        if (!observation.updated_selected_slot_offset.has_value()) {
            plan.status = PhysicsTimeSliceStatus::missing_slot_offset;
            return plan;
        }
        selected_offset = *observation.updated_selected_slot_offset;
        for (std::size_t index = 0; index < observation.objects.size(); ++index) {
            const auto& object = observation.objects[index];
            if (!object.nonnull) {
                plan.status = PhysicsTimeSliceStatus::null_object_dereference;
                return plan;
            }
            if (object.virtual_0x2c_result) {
                const auto object_offset = object.object_offset_0x34;
                const auto slice_address = observation.storage_base + selected_offset + 0x0bU;
                plan.callbacks_0x20.push_back({
                    index, object_offset + slice_address});
            }
        }
    } else {
        selected_offset = observation.slot_offsets[
            static_cast<std::size_t>(plan.selected_slot)];
    }

    ++plan.sync_helper_calls;
    plan.captured_record_address = observation.storage_base + selected_offset;
    plan.capture_helper_0x427150_called = true;
    if (!observation.capture_source.has_value()) {
        plan.status = PhysicsTimeSliceStatus::missing_capture_source;
        return plan;
    }
    plan.captured_record = plan_physics_time_capture(*observation.capture_source);
    return plan;
}

PhysicsTimeCapturePlan plan_physics_time_capture(
    const PhysicsTimeCaptureObservation& observation) noexcept
{
    PhysicsTimeCapturePlan plan{};
    plan.fdiv_compatibility_path_used = observation.fdiv_compatibility_enabled;
    plan.record_bytes[0] = static_cast<std::uint8_t>(observation.source_word);
    plan.record_bytes[1] = static_cast<std::uint8_t>(observation.source_word >> 8U);
    plan.record_bytes[2] = static_cast<std::uint8_t>(observation.source_word >> 16U);
    plan.record_bytes[3] = static_cast<std::uint8_t>(observation.source_word >> 24U);
    plan.record_bytes[4] = observation.source_byte;

    constexpr std::array<long double, 3> lower{-300.0L, -4.0L, -4.0L};
    constexpr std::array<long double, 3> upper{300.0L, 4.0L, 4.0L};
    constexpr std::array<long double, 3> spans{600.0L, 8.0L, 8.0L};
    constexpr long double output_scale = 32768.0L;
    for (std::size_t index = 0; index < observation.source_components.size(); ++index) {
        long double value = static_cast<long double>(observation.source_components[index]);
        if (!(value >= lower[index])) value = lower[index];
        else if (value > upper[index]) value = upper[index];

        const auto normalized = divide_with_vc98_fdiv_compatibility(
            value - lower[index], spans[index],
            observation.fdiv_compatibility_enabled) * output_scale;
        const auto truncated = static_cast<std::int32_t>(std::trunc(normalized));
        const auto encoded = static_cast<std::uint16_t>(truncated);
        const auto quantized = std::bit_cast<std::int16_t>(encoded);
        plan.quantized_components[index] = quantized;
        const auto byte_offset = 5U + index * 2U;
        plan.record_bytes[byte_offset] = static_cast<std::uint8_t>(encoded);
        plan.record_bytes[byte_offset + 1U] = static_cast<std::uint8_t>(encoded >> 8U);
    }
    return plan;
}

PhysicsAlternateTimePlan plan_physics_alternate_time_path(
    const PhysicsAlternateTimeObservation& observation)
{
    PhysicsAlternateTimePlan plan{};
    if (!observation.path_enabled) return plan;
    plan.status = PhysicsAlternateTimeStatus::complete;

    if (observation.slot_count == 0) {
        plan.status = PhysicsAlternateTimeStatus::division_by_zero;
        return plan;
    }
    if (observation.slot_count < 0) {
        plan.status = PhysicsAlternateTimeStatus::invalid_slot_index;
        return plan;
    }

    plan.selected_slot = (observation.simulation_counter / 6)
        % observation.slot_count;
    if (plan.selected_slot < 0
        || static_cast<std::size_t>(plan.selected_slot) >= observation.slot_offsets.size()) {
        plan.status = PhysicsAlternateTimeStatus::invalid_slot_index;
        return plan;
    }
    const bool same_slot = observation.previous_slot == plan.selected_slot;
    plan.next_slot = same_slot
        ? plan.selected_slot
        : (plan.selected_slot + 1) % observation.slot_count;
    if (static_cast<std::size_t>(plan.next_slot) >= observation.slot_offsets.size()) {
        plan.status = PhysicsAlternateTimeStatus::missing_slot_offset;
        return plan;
    }
    if (!observation.current_record.has_value()
        || !observation.next_record.has_value()) {
        plan.status = PhysicsAlternateTimeStatus::missing_record;
        return plan;
    }

    constexpr float one_sixth = std::bit_cast<float>(std::uint32_t{0x3e2aaaabU});
    const auto remainder = observation.simulation_counter % 6;
    plan.interpolation_fraction = static_cast<float>(remainder * one_sixth);

    const auto current_offset = observation.slot_offsets[
        static_cast<std::size_t>(plan.selected_slot)];
    const auto next_offset = observation.slot_offsets[
        static_cast<std::size_t>(plan.next_slot)];
    for (std::size_t index = 0; index < observation.objects.size(); ++index) {
        const auto& object = observation.objects[index];
        if (!object.nonnull) continue;
        ++plan.active_object_gates_called;
        if (!object.virtual_0x2c_result) continue;
        const auto object_bias = object.object_offset_0x34 + 0x0bU;
        plan.object_calls.push_back({
            index,
            observation.storage_base + current_offset + object_bias,
            observation.storage_base + next_offset + object_bias,
            plan.interpolation_fraction});
    }

    const auto& current = *observation.current_record;
    const auto& next = *observation.next_record;
    const auto read_u16 = [](const PhysicsTimeRecordBytes& record,
                             const std::size_t offset) noexcept {
        return static_cast<std::uint16_t>(record[offset])
            | (static_cast<std::uint16_t>(record[offset + 1U]) << 8U);
    };
    const auto read_u32 = [](const PhysicsTimeRecordBytes& record) noexcept {
        return static_cast<std::uint32_t>(record[0])
            | (static_cast<std::uint32_t>(record[1]) << 8U)
            | (static_cast<std::uint32_t>(record[2]) << 16U)
            | (static_cast<std::uint32_t>(record[3]) << 24U);
    };

    PhysicsTimeInterpolatedSample sample{};
    sample.source_word = read_u32(current);
    sample.source_byte = current[4];
    constexpr long double quantization_scale = 1.0L / 32768.0L;
    constexpr std::array<long double, 3> ranges{600.0L, 8.0L, 8.0L};
    constexpr std::array<long double, 3> lower{-300.0L, -4.0L, -4.0L};
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        const auto byte_offset = 5U + index * 2U;
        const auto current_component = static_cast<long double>(
            read_u16(current, byte_offset)) * quantization_scale;
        const auto next_component = static_cast<long double>(
            read_u16(next, byte_offset)) * quantization_scale;
        const auto interpolated = current_component * ranges[index]
            + (next_component - current_component) * ranges[index]
                * static_cast<long double>(plan.interpolation_fraction)
            + lower[index];
        sample.components[index] = static_cast<float>(interpolated);
    }
    plan.interpolated_sample = sample;

    if (!same_slot) {
        plan.record_command_parser_reached = true;
        bool parser_terminated = false;
        for (const auto& command : observation.command_stream) {
            if (command.command_byte >= 8U || !command.handler_present) {
                parser_terminated = true;
                break;
            }
            const auto known_consumed_bytes =
                known_physics_command_payload_bytes(command.command_byte);
            const auto consumed_bytes = known_consumed_bytes.has_value()
                ? known_consumed_bytes
                : command.handler_consumed_bytes;
            if (!consumed_bytes.has_value()) {
                plan.parser_input_exhausted = true;
                plan.status = PhysicsAlternateTimeStatus::command_observation_exhausted;
                return plan;
            }
            ++plan.command_handlers_called;
            plan.command_bytes_advanced += *consumed_bytes + 1U;
        }
        if (!parser_terminated) {
            plan.parser_input_exhausted = true;
            plan.status = PhysicsAlternateTimeStatus::command_observation_exhausted;
            return plan;
        }
    } else {
        plan.sync_flag_0x508760_set = true;
    }

    plan.previous_slot_after = plan.selected_slot;
    plan.previous_slot_global_written = true;
    return plan;
}

std::optional<PhysicsTaskObjectFactoryDescriptor>
physics_task_object_factory_descriptor(const std::uint32_t fourcc) noexcept
{
    for (const auto& descriptor : descriptors) {
        if (descriptor.fourcc == fourcc) return descriptor;
    }
    return std::nullopt;
}

PhysicsTaskFactoryPlan plan_physics_task_object_factory(
    const std::uint32_t fourcc,
    const bool allocation_succeeded,
    const bool constructor_returned_object,
    const bool panic_reporter_returned) noexcept
{
    PhysicsTaskFactoryPlan plan{};
    plan.descriptor = physics_task_object_factory_descriptor(fourcc);
    if (!plan.descriptor.has_value()) {
        plan.status = PhysicsTaskFactoryStatus::unknown_type;
        plan.panic_reporter_called = true;
        plan.continued_after_panic = panic_reporter_returned;
        plan.factory_call_returns = panic_reporter_returned;
        return plan;
    }

    plan.allocation_attempted = true;
    if (!allocation_succeeded) {
        plan.status = PhysicsTaskFactoryStatus::allocation_failed;
        return plan;
    }

    plan.constructor_called = true;
    plan.returns_object = constructor_returned_object;
    plan.status = constructor_returned_object
        ? PhysicsTaskFactoryStatus::constructed
        : PhysicsTaskFactoryStatus::constructor_returned_null;
    return plan;
}

PhysicsTaskRecordLoadPlan plan_physics_task_record_load(
    const PhysicsTaskRecordLoadObservation& observation) noexcept
{
    PhysicsTaskRecordLoadPlan plan{};
    if (observation.record_length < 0) {
        plan.status = PhysicsTaskRecordLoadStatus::end_of_records;
        return plan;
    }

    plan.payload_read = true;
    if (!observation.payload_complete) {
        plan.status = PhysicsTaskRecordLoadStatus::incomplete_payload;
        plan.binary_read_exceeds_payload = true;
        return plan;
    }

    plan.factory = plan_physics_task_object_factory(
        observation.fourcc,
        observation.allocation_succeeded,
        observation.constructor_returned_object,
        observation.panic_reporter_returned);
    if (!plan.factory.factory_call_returns) {
        plan.status = PhysicsTaskRecordLoadStatus::factory_did_not_return;
        return plan;
    }

    // 0x4224D0 does not guard the virtual call with a null check. Preserve
    // that behavior as an observation without dereferencing null here.
    plan.virtual_0x2c_called = true;
    if (!plan.factory.returns_object) {
        plan.status = PhysicsTaskRecordLoadStatus::factory_returned_null;
        return plan;
    }

    plan.post_construct_0x426f20_called =
        observation.object_virtual_0x2c_result;
    if (plan.post_construct_0x426f20_called &&
        observation.task_data_cursor_before.has_value()) {
        plan.object_task_data_offset_0x34 =
            *observation.task_data_cursor_before;
        if (observation.object_virtual_0x18_size.has_value()) {
            plan.task_data_cursor_after =
                *observation.task_data_cursor_before +
                *observation.object_virtual_0x18_size;
        }
    }
    plan.output_slot_written = true;
    plan.returns_success = true;
    plan.status = PhysicsTaskRecordLoadStatus::object_loaded;
    return plan;
}

PhysicsTaskArrayPopulationPlan plan_physics_task_array_population(
    const std::span<const std::uint8_t> reader_success,
    const bool array_allocation_succeeded) noexcept
{
    PhysicsTaskArrayPopulationPlan plan{};
    plan.allocation_failed = !array_allocation_succeeded;
    if (!array_allocation_succeeded) {
        // 0x420480 does not branch on the allocator result before its first
        // record-reader call, which receives the global pointer as an output.
        plan.reader_calls = 1;
        plan.array_pointer_used_after_allocation_failure = true;
        return plan;
    }

    for (const std::uint8_t success : reader_success) {
        ++plan.reader_calls;
        if (success == 0) {
            plan.terminated_by_reader = true;
            break;
        }
        ++plan.successful_records;
    }

    plan.observations_exhausted = !plan.terminated_by_reader;
    constexpr std::uint32_t pointer_slots = 0x800U / sizeof(std::uint32_t);
    plan.writes_past_allocated_array = plan.successful_records > pointer_slots;
    return plan;
}

PhysicsTrackServicePlan plan_physics_track_service(
    const std::uint32_t selector,
    const bool allocation_succeeded,
    const bool constructor_returned_object,
    const bool panic_reporter_returned) noexcept
{
    PhysicsTrackServicePlan plan{};
    plan.selector = selector;
    switch (selector) {
    case 0:
        plan.allocation_bytes = 0x7dcU;
        plan.constructor_address = 0x0043c870U;
        plan.vtable_address = 0x004c84d8U;
        plan.post_load_virtual_0x0c_target = 0x0043cb20U;
        break;
    case 1:
        plan.allocation_bytes = 0x68U;
        plan.constructor_address = 0x0043bd10U;
        plan.vtable_address = 0x004c7810U;
        plan.post_load_virtual_0x0c_target = 0x0043bee0U;
        break;
    case 3:
        plan.allocation_bytes = 0x68U;
        plan.constructor_address = 0x0043bd10U;
        plan.vtable_address = 0x004c7880U;
        plan.post_load_virtual_0x0c_target = 0x0043bee0U;
        break;
    case 5:
        plan.allocation_bytes = 0x48U;
        plan.constructor_address = 0x0043c360U;
        plan.vtable_address = 0x004c8420U;
        plan.post_load_virtual_0x0c_target = 0x0043c3c0U;
        break;
    default:
        plan.panic_reporter_called = true;
        plan.continued_after_panic = panic_reporter_returned;
        plan.virtual_0x14_called = panic_reporter_returned;
        plan.virtual_call_has_null_object = panic_reporter_returned;
        plan.status = panic_reporter_returned
            ? PhysicsTrackServiceStatus::unknown_type_null_virtual_call
            : PhysicsTrackServiceStatus::unknown_type_panic_did_not_return;
        return plan;
    }

    plan.allocation_attempted = true;
    if (!allocation_succeeded) {
        plan.virtual_0x14_called = true;
        plan.virtual_call_has_null_object = true;
        plan.status = PhysicsTrackServiceStatus::allocation_failed_then_virtual_call;
        return plan;
    }

    plan.constructor_called = true;
    const bool object_ready = (selector == 1U || selector == 3U)
        || constructor_returned_object;
    if (!object_ready) {
        plan.virtual_0x14_called = true;
        plan.virtual_call_has_null_object = true;
        plan.status = PhysicsTrackServiceStatus::constructor_returned_null_then_virtual_call;
        return plan;
    }

    plan.virtual_0x14_called = true;
    plan.status = PhysicsTrackServiceStatus::constructed;
    return plan;
}

PhysicsTrackPostLoadPlan plan_physics_track_post_load(
    const PhysicsTrackPostLoadObservation& observation) noexcept
{
    PhysicsTrackPostLoadPlan plan{};
    switch (observation.selector) {
    case 0:
        plan.callback_address = 0x0043cb20U;
        break;
    case 1:
    case 3:
        plan.callback_address = 0x0043bee0U;
        break;
    case 5:
        plan.callback_address = 0x0043c3c0U;
        break;
    default:
        break;
    }

    if (!observation.receiver_nonnull) {
        plan.status = PhysicsTrackPostLoadStatus::null_receiver_fault;
        return plan;
    }
    plan.virtual_0x0c_called = true;

    if (observation.selector == 0U) {
        plan.object_writes.reserve(32U);
        for (std::uint32_t index = 0; index < 16U; ++index) {
            plan.object_writes.push_back({0x78cU + index * 4U, 0U});
            plan.object_writes.push_back({0x74U * (index + 1U), index + 1U});
        }
        plan.callback_result = true;
        plan.status = PhysicsTrackPostLoadStatus::evaluated;
        return plan;
    }

    if (observation.selector == 5U) {
        plan.callback_result = true;
        plan.status = PhysicsTrackPostLoadStatus::evaluated;
        return plan;
    }
    if (observation.selector != 1U && observation.selector != 3U) {
        plan.status = PhysicsTrackPostLoadStatus::callback_body_unrecovered;
        return plan;
    }
    if (!observation.fields_0x44_0x48_0x4c.has_value()) {
        plan.status = PhysicsTrackPostLoadStatus::missing_input;
        return plan;
    }

    const auto& fields = *observation.fields_0x44_0x48_0x4c;
    plan.callback_result = fields[0] != 0U
        && fields[1] != 0U
        && fields[2] != 0U;
    plan.status = PhysicsTrackPostLoadStatus::evaluated;
    return plan;
}

PhysicsTaskTableInitPlan plan_physics_task_table_init(
    const PhysicsTaskTableInitObservation& observation) noexcept
{
    PhysicsTaskTableInitPlan plan{};
    plan.primary_allocation_failed = !observation.primary_allocation_succeeded;
    plan.primary_null_pointer_stored = plan.primary_allocation_failed;
    plan.entry_stride = observation.previous_stride + 0x0bU;
    if (plan.entry_stride == 0) {
        plan.status = PhysicsTaskTableInitStatus::division_by_zero;
        return plan;
    }

    plan.entry_count = 0x200000U / plan.entry_stride;
    if (observation.maximum_entries <= std::bit_cast<std::int32_t>(plan.entry_count)) {
        plan.entry_count = static_cast<std::uint32_t>(observation.maximum_entries);
    }
    plan.sentinel_allocation_bytes = plan.entry_count * 4U;
    plan.sentinel_allocation_attempted = true;
    plan.sentinel_allocation_failed = !observation.sentinel_allocation_succeeded;
    plan.sentinel_fill_iterations = plan.entry_count & 0x3fffffffU;
    plan.zero_length_fill_loop_present = true;
    plan.sentinel_fill_dereferences_null = plan.sentinel_allocation_failed
        && plan.sentinel_fill_iterations != 0;
    if (plan.sentinel_fill_dereferences_null) {
        plan.status = PhysicsTaskTableInitStatus::sentinel_fill_dereferences_null;
    }
    return plan;
}

} // namespace viper
