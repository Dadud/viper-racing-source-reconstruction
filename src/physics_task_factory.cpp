#include "viper/physics_task_factory.hpp"
#include "viper/legacy_fdiv.hpp"

#include <algorithm>
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

PhysicsStatTaskConstructorPlan plan_physics_stat_task_constructor(
    const PhysicsStatTaskConstructorObservation& observation)
{
    PhysicsStatTaskConstructorPlan plan{};
    plan.record_copy_writes.reserve(
        observation.record_words_0x08_to_0x34.size());
    for (std::size_t index = 0U;
         index < observation.record_words_0x08_to_0x34.size(); ++index) {
        plan.record_copy_writes.push_back({
            0x38U + static_cast<std::uint32_t>(index * sizeof(std::uint32_t)),
            observation.record_words_0x08_to_0x34[index]});
    }

    if (observation.collision_volume_flag_0x38 != 0U) {
        plan.status = PhysicsStatTaskConstructorStatus::
            unsupported_collision_volume;
        plan.collision_volume_reporter_called = true;
        return plan;
    }

    plan.child_allocation_attempted = true;
    plan.child_pointer_stored = true;
    plan.child_count_incremented = true;
    plan.child_count_after = 1U;
    if (!observation.child_allocation_succeeded) {
        plan.status = PhysicsStatTaskConstructorStatus::child_allocation_failed;
        plan.child_address = 0U;
        return plan;
    }

    plan.status = PhysicsStatTaskConstructorStatus::child_constructed;
    plan.child_box_constructor_0x42d460_called = true;
    plan.child_address = observation.child_address;
    plan.child_center = observation.child_center_0x3c_to_0x44;
    if (std::isfinite(plan.child_center[0]) &&
        std::isfinite(plan.child_center[1]) &&
        std::isfinite(plan.child_center[2])) {
        plan.child_bound_0x28 = std::max({plan.child_center[0],
                                          plan.child_center[1],
                                          plan.child_center[2]});
    }
    return plan;
}

PhysicsStatTaskRuntimeMaterialization
materialize_inactive_stat_task_runtime_state(
    const std::span<const std::byte> record_bytes,
    const std::uint32_t serialized_stream_cursor)
{
    PhysicsStatTaskRuntimeMaterialization result;
    const auto read_u32 = [&](const std::size_t offset) {
        return std::to_integer<std::uint32_t>(record_bytes[offset]) |
            (std::to_integer<std::uint32_t>(record_bytes[offset + 1U]) << 8U) |
            (std::to_integer<std::uint32_t>(record_bytes[offset + 2U]) << 16U) |
            (std::to_integer<std::uint32_t>(record_bytes[offset + 3U]) << 24U);
    };
    if (record_bytes.size() != 0x48U || read_u32(0U) != 0x53544154U ||
        read_u32(4U) != record_bytes.size()) {
        result.status = PhysicsStatTaskRuntimeMaterializationStatus::malformed_record;
        return result;
    }

    PhysicsStatTaskConstructorObservation observation{};
    for (std::size_t index = 0U;
         index < observation.record_words_0x08_to_0x34.size(); ++index) {
        observation.record_words_0x08_to_0x34[index] =
            read_u32(0x08U + index * sizeof(std::uint32_t));
    }
    observation.collision_volume_flag_0x38 = read_u32(0x38U);
    for (std::size_t axis = 0U; axis < observation.child_center_0x3c_to_0x44.size();
         ++axis) {
        observation.child_center_0x3c_to_0x44[axis] =
            std::bit_cast<float>(read_u32(0x3cU + axis * sizeof(std::uint32_t)));
    }
    if (observation.collision_volume_flag_0x38 != 0U) {
        result.status = PhysicsStatTaskRuntimeMaterializationStatus::
            unsupported_collision_volume;
        return result;
    }

    // The host representation owns the child inline; native virtual behavior
    // for this class is inactive and its +0x0C callback is a no-op.
    observation.child_allocation_succeeded = true;
    const auto constructor = plan_physics_stat_task_constructor(observation);
    if (constructor.status != PhysicsStatTaskConstructorStatus::child_constructed ||
        !constructor.child_bound_0x28.has_value()) {
        result.status = PhysicsStatTaskRuntimeMaterializationStatus::
            incomplete_constructor_state;
        return result;
    }

    PhysicsStatTaskRuntimeState state{};
    state.serialized_stream_cursor = serialized_stream_cursor;
    for (const auto& write : constructor.record_copy_writes) {
        if (write.byte_offset < 0x38U || write.byte_offset > 0x64U ||
            ((write.byte_offset - 0x38U) % sizeof(std::uint32_t)) != 0U) {
            result.status = PhysicsStatTaskRuntimeMaterializationStatus::
                incomplete_constructor_state;
            return result;
        }
        state.copied_record_words_0x38_to_0x64[
            (write.byte_offset - 0x38U) / sizeof(std::uint32_t)] = write.value;
    }
    state.child_center = constructor.child_center;
    state.child_bound_0x28 = constructor.child_bound_0x28;
    state.child_count = constructor.child_count_after;
    state.active_gate = false;
    state.update_callback_is_noop = true;
    result.status = PhysicsStatTaskRuntimeMaterializationStatus::complete;
    result.state = std::move(state);
    return result;
}

PhysicsCheckpointTaskRuntimeMaterialization
materialize_checkpoint_task_runtime_state(
    const std::span<const std::byte> record_bytes,
    const std::size_t stream_record_index,
    const std::uint32_t serialized_stream_cursor,
    const bool fdiv_compatibility_enabled)
{
    PhysicsCheckpointTaskRuntimeMaterialization result{};
    const auto read_u32 = [&](const std::size_t offset) {
        return std::to_integer<std::uint32_t>(record_bytes[offset]) |
            (std::to_integer<std::uint32_t>(record_bytes[offset + 1U]) << 8U) |
            (std::to_integer<std::uint32_t>(record_bytes[offset + 2U]) << 16U) |
            (std::to_integer<std::uint32_t>(record_bytes[offset + 3U]) << 24U);
    };
    if (record_bytes.size() != 0x20U || read_u32(0U) != 0x43484b50U ||
        read_u32(4U) != record_bytes.size()) {
        result.status =
            PhysicsCheckpointTaskRuntimeMaterializationStatus::malformed_record;
        return result;
    }

    PhysicsCheckpointTaskConstructorObservation observation{};
    for (std::size_t axis = 0U; axis < observation.first_vertex.size(); ++axis) {
        observation.first_vertex[axis] = std::bit_cast<float>(
            read_u32(0x08U + axis * sizeof(std::uint32_t)));
        observation.second_vertex[axis] = std::bit_cast<float>(
            read_u32(0x14U + axis * sizeof(std::uint32_t)));
        if (!std::isfinite(observation.first_vertex[axis]) ||
            !std::isfinite(observation.second_vertex[axis])) {
            result.status = PhysicsCheckpointTaskRuntimeMaterializationStatus::
                non_finite_geometry;
            return result;
        }
    }

    // The serialized CHKP contains endpoints only. The shipped constructor
    // derives its basis from them; it has no independent yaw input. The
    // retained semantic collection supplies its own registration owner.
    observation.yaw_degrees = 0.0F;
    observation.fdiv_compatibility_enabled = fdiv_compatibility_enabled;
    observation.constructor_backend_nonnull = true;
    const auto constructor = plan_physics_checkpoint_task_constructor(observation);
    if (constructor.status != PhysicsCheckpointTaskConstructorStatus::constructed) {
        result.status =
            PhysicsCheckpointTaskRuntimeMaterializationStatus::constructor_failed;
        return result;
    }
    const auto finite = [](const float value) { return std::isfinite(value); };
    if (!std::all_of(constructor.rotated_basis.begin(),
                     constructor.rotated_basis.end(), finite) ||
        !std::all_of(constructor.center.begin(), constructor.center.end(), finite) ||
        !std::all_of(constructor.transformed_vertices.begin(),
                     constructor.transformed_vertices.end(),
                     [&](const auto& vertex) {
                         return vertex.has_value() &&
                             std::all_of(vertex->begin(), vertex->end(), finite);
                     })) {
        result.status = PhysicsCheckpointTaskRuntimeMaterializationStatus::
            non_finite_geometry;
        return result;
    }

    PhysicsCheckpointTaskRuntimeState state{};
    state.stream_record_index = stream_record_index;
    state.serialized_stream_cursor = serialized_stream_cursor;
    state.first_vertex = observation.first_vertex;
    state.second_vertex = observation.second_vertex;
    state.constructor = constructor;
    state.active_gate = false;
    state.update_callback_is_noop = true;
    result.status = PhysicsCheckpointTaskRuntimeMaterializationStatus::complete;
    result.state = std::move(state);
    return result;
}

PhysicsWoblTaskConstructorPlan plan_physics_wobl_task_constructor(
    const PhysicsWoblTaskConstructorObservation& observation)
{
    PhysicsWoblTaskConstructorPlan plan{};
    plan.object_writes.push_back({0x4acU, observation.record_value_0x1c});
    plan.resource_lookup_lock_entered = true;
    plan.resource_lookup_lock_left = true;

    for (std::size_t index = 0U;
         index < observation.resource_entries.size(); ++index) {
        const auto& entry = observation.resource_entries[index];
        if (entry.resource_id_0x30 != observation.record_resource_id_0x18) {
            continue;
        }
        plan.matched_resource_entry_index = index;
        plan.resource_entry_found = true;
        break;
    }

    if (!plan.resource_entry_found) {
        plan.status = PhysicsWoblTaskConstructorStatus::
            resource_not_found_null_dereference;
        plan.lookup_null_result_dereferenced = true;
        return plan;
    }

    const auto& entry = observation.resource_entries[
        *plan.matched_resource_entry_index];
    for (std::size_t index = 0U; index < entry.words_0x00_to_0x2c.size(); ++index) {
        const auto value = entry.words_0x00_to_0x2c[index];
        const auto word_offset = static_cast<std::uint32_t>(
            index * sizeof(std::uint32_t));
        plan.object_writes.push_back({0x47cU + word_offset, value});
    }

    plan.intrusive_node_dereferenced = true;
    if (entry.intrusive_node_address_0x38 == 0U) {
        plan.status = PhysicsWoblTaskConstructorStatus::
            intrusive_node_null_dereference;
        return plan;
    }

    plan.pointer_writes.push_back({
        entry.intrusive_node_address_0x38 + 8U,
        observation.object_subobject_address_0x38});
    plan.pointer_writes.push_back({
        entry.intrusive_node_address_0x38 + 4U,
        observation.object_address});
    plan.object_writes.push_back({0x4b0U, 0U});
    for (std::size_t index = 0U; index < entry.words_0x00_to_0x2c.size(); ++index) {
        const auto value = entry.words_0x00_to_0x2c[index];
        const auto word_offset = static_cast<std::uint32_t>(
            index * sizeof(std::uint32_t));
        plan.object_writes.push_back({0x38U + word_offset, value});
    }
    plan.status = PhysicsWoblTaskConstructorStatus::linked;
    return plan;
}

PhysicsGcarTaskConstructorPlan plan_physics_gcar_task_constructor(
    const PhysicsGcarTaskConstructorObservation& observation)
{
    PhysicsGcarTaskConstructorPlan plan{};
    plan.derived_vtable_installed = true;
    plan.global_singleton_written = true;
    plan.singleton_global_value = observation.object_address;
    plan.object_writes.push_back({0xec8U, observation.record_word_0x1e0});
    plan.object_writes.push_back({0xeccU, observation.base_car_identity_0x510});

    plan.sample_buffer_allocation_attempted = true;
    plan.sample_buffer_pointer_stored = true;
    if (observation.sample_buffer_allocation_succeeded) {
        plan.sample_buffer_address = observation.sample_buffer_address;
    }
    if (!observation.sample_buffer_allocation_succeeded) {
        plan.object_writes.push_back({0xef4U, 0U});
    } else if (observation.sample_buffer_address.has_value()) {
        plan.object_writes.push_back({
            0xef4U, *observation.sample_buffer_address});
    }
    plan.identity_helper_calls_are_no_write = true;
    plan.identity_helper_call_count =
        observation.sample_buffer_allocation_succeeded ? 0x960U : 0U;
    plan.allocated_sample_buffer_contents_left_uninitialized =
        observation.sample_buffer_allocation_succeeded;
    plan.object_writes.push_back({0xefcU, 0U});

    // 0x427A40's unconditional local resets precede the sample-source call.
    plan.object_writes.push_back({0x50cU, 0U});
    plan.object_writes.push_back({0xed0U, 0U});
    plan.object_writes.push_back({0x004U, 0U});
    if (!observation.sample_source_helper_succeeded) {
        plan.status = PhysicsGcarTaskConstructorStatus::source_helper_failed;
        plan.object_writes.push_back({0xed8U, 0x0012d687U});
        plan.object_writes.push_back({0xef8U, 0U});
        plan.object_writes.push_back({0xee0U, 0U});
    } else {
        const auto record_count = observation.sample_header[2];
        const auto copy_bytes = record_count * 0x3cU;
        plan.sample_copy_words_requested = copy_bytes / sizeof(std::uint32_t);
        constexpr std::uint32_t allocated_words = 0x23280U /
            sizeof(std::uint32_t);
        plan.sample_copy_exceeds_allocation =
            plan.sample_copy_words_requested > allocated_words;
        plan.sample_source_copy_exceeds_capture =
            observation.sample_source_words.size() <
                plan.sample_copy_words_requested;

        plan.object_writes.push_back({
            0xeccU, observation.sample_source_cursor});
        plan.object_writes.push_back({0xee0U, observation.sample_header[1]});
        plan.object_writes.push_back({0xed8U, observation.sample_header[0]});
        plan.object_writes.push_back({0xef8U, record_count});

        if (plan.sample_copy_words_requested != 0U &&
            !observation.sample_buffer_allocation_succeeded) {
            plan.status = PhysicsGcarTaskConstructorStatus::
                null_sample_buffer_copy_fault;
            plan.sample_copy_faults_on_null_buffer = true;
            return plan;
        }

        plan.sample_copy_words_from_observation = static_cast<std::uint32_t>(
            std::min<std::size_t>(plan.sample_copy_words_requested,
                                  observation.sample_source_words.size()));
        plan.sample_buffer_words_written.assign(
            observation.sample_source_words.begin(),
            observation.sample_source_words.begin() +
                plan.sample_copy_words_from_observation);
        plan.object_writes.push_back({0xee4U, observation.sample_header[0]});
        if (plan.sample_source_copy_exceeds_capture) {
            plan.status = PhysicsGcarTaskConstructorStatus::
                incomplete_sample_source;
        } else if (plan.sample_copy_exceeds_allocation) {
            plan.status = PhysicsGcarTaskConstructorStatus::
                sample_buffer_overflow;
        } else {
            plan.status = PhysicsGcarTaskConstructorStatus::constructed;
        }
    }

    plan.object_writes.push_back({0xeecU, 0U});
    plan.object_writes.push_back({0xef0U, 0U});
    plan.object_writes.push_back({0xed4U, 0xffffffffU});
    const auto final_start_time = observation.sample_source_helper_succeeded
        ? observation.sample_header[0]
        : 0x0012d687U;
    plan.object_writes.push_back({0xee4U, final_start_time});
    return plan;
}

PhysicsCheckpointTaskConstructorPlan
plan_physics_checkpoint_task_constructor(
    const PhysicsCheckpointTaskConstructorObservation& observation)
{
    PhysicsCheckpointTaskConstructorPlan plan{};
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;
    const auto write_float = [&plan](const std::uint32_t offset,
                                     const float value) {
        plan.object_writes.push_back({offset, std::bit_cast<std::uint32_t>(value)});
    };

    for (std::size_t axis = 0U; axis < 3U; ++axis) {
        write_float(0x6cU + static_cast<std::uint32_t>(axis * 4U),
                    observation.first_vertex[axis]);
        write_float(0x78U + static_cast<std::uint32_t>(axis * 4U),
                    observation.second_vertex[axis]);
    }

    std::array<float, 3> delta{};
    for (std::size_t axis = 0U; axis < delta.size(); ++axis) {
        delta[axis] = observation.second_vertex[axis]
            - observation.first_vertex[axis];
        write_float(0x38U + static_cast<std::uint32_t>(axis * 4U),
                    delta[axis]);
    }
    const auto raw_delta = delta;

    std::array<float, 3> cross{
        -delta[0] * delta[1],
        delta[0] * delta[0] + delta[2] * delta[2],
        -delta[2] * delta[1]};
    std::array<float, 3> perpendicular{-delta[2], 0.0F, delta[0]};
    plan.cross_product_helper_0x434bf0_called = true;

    const auto normalize = [&observation](std::array<float, 3>& vector) {
        const auto x = static_cast<long double>(vector[0]);
        const auto y = static_cast<long double>(vector[1]);
        const auto z = static_cast<long double>(vector[2]);
        const auto length = std::sqrt(x * x + y * y + z * z);
        const auto inverse = divide_with_vc98_fdiv_compatibility(
            1.0L, length, observation.fdiv_compatibility_enabled);
        for (auto& component : vector) {
            component = static_cast<float>(
                static_cast<long double>(component) * inverse);
        }
    };
    normalize(delta);
    normalize(cross);
    normalize(perpendicular);

    std::array<float, 9> basis{
        delta[0], delta[1], delta[2],
        cross[0], cross[1], cross[2],
        perpendicular[0], perpendicular[1], perpendicular[2]};

    constexpr float degrees_to_radians = 0.017453292F;
    const float angle_radians = observation.yaw_degrees * degrees_to_radians;
    const auto angle = static_cast<long double>(angle_radians);
    const float cosine = static_cast<float>(std::cos(angle));
    const float sine = static_cast<float>(std::sin(angle));
    const std::array<float, 9> yaw_matrix{
        cosine, 0.0F, -sine,
        0.0F, 1.0F, 0.0F,
        sine, 0.0F, cosine};

    // The following zero-angle X/Z rotations multiply by identity matrices.
    for (std::size_t row = 0U; row < 3U; ++row) {
        for (std::size_t column = 0U; column < 3U; ++column) {
            const auto value =
                static_cast<long double>(yaw_matrix[row * 3U])
                    * static_cast<long double>(basis[column])
                + static_cast<long double>(yaw_matrix[row * 3U + 1U])
                    * static_cast<long double>(basis[3U + column])
                + static_cast<long double>(yaw_matrix[row * 3U + 2U])
                    * static_cast<long double>(basis[6U + column]);
            plan.rotated_basis[row * 3U + column] =
                static_cast<float>(value);
        }
    }
    for (std::size_t index = 0U; index < plan.rotated_basis.size(); ++index) {
        write_float(0x38U + static_cast<std::uint32_t>(index * 4U),
                    plan.rotated_basis[index]);
    }

    for (std::size_t axis = 0U; axis < plan.center.size(); ++axis) {
        const auto half_delta = raw_delta[axis] * 0.5F;
        plan.center[axis] = observation.first_vertex[axis] + half_delta;
        write_float(0x5cU + static_cast<std::uint32_t>(axis * 4U),
                    plan.center[axis]);
    }

    for (std::size_t vertex = 0U; vertex < 2U; ++vertex) {
        const auto& point = vertex == 0U
            ? observation.first_vertex : observation.second_vertex;
        std::array<float, 3> relative{};
        for (std::size_t axis = 0U; axis < relative.size(); ++axis) {
            relative[axis] = point[axis] - plan.center[axis];
        }
        std::array<float, 3> transformed{};
        for (std::size_t row = 0U; row < 3U; ++row) {
            const auto value =
                static_cast<long double>(plan.rotated_basis[row * 3U])
                    * relative[0]
                + static_cast<long double>(
                      plan.rotated_basis[row * 3U + 1U]) * relative[1]
                + static_cast<long double>(
                      plan.rotated_basis[row * 3U + 2U]) * relative[2];
            transformed[row] = static_cast<float>(value);
        }
        plan.transformed_vertices[vertex] = transformed;
    }

    plan.backend_virtual_0x34_called = true;
    if (!observation.constructor_backend_nonnull) {
        plan.status = PhysicsCheckpointTaskConstructorStatus::
            null_backend_virtual_call_fault;
        plan.backend_null_receiver_fault = true;
    }
    return plan;
}

PhysicsBallTaskConstructorPlan plan_physics_ball_task_constructor(
    const PhysicsBallTaskConstructorObservation& observation)
{
    PhysicsBallTaskConstructorPlan plan{};
    plan.derived_vtable_installed = true;
    plan.object_writes.push_back({0U, 0x004c8300U});
    plan.object_writes.push_back({0x47cU, observation.registration_index_0x34});
    for (std::size_t axis = 0U; axis < observation.position_0x18.size(); ++axis) {
        plan.object_writes.push_back({
            0x5cU + static_cast<std::uint32_t>(axis * sizeof(std::uint32_t)),
            std::bit_cast<std::uint32_t>(observation.position_0x18[axis])});
    }

    plan.child_allocation_attempted = true;
    plan.child_pointer_stored = true;
    plan.child_count_incremented = true;
    plan.child_registration_value = observation.object_address;
    plan.child_registration_address = 0x005092c0U
        + observation.registration_index_0x34 * sizeof(std::uint32_t);
    plan.child_base_matrix_address = observation.object_address + 0x38U;
    plan.registration_table_written = true;
    if (!observation.child_allocation_succeeded) {
        plan.object_writes.push_back({0x14U, 0U});
    } else if (observation.child_address.has_value()) {
        plan.object_writes.push_back({0x14U, *observation.child_address});
    }
    plan.object_writes.push_back({0x24U, plan.child_count_after});
    plan.object_writes.push_back({0x474U, 1U});
    plan.object_writes.push_back({0x478U, 0U});

    if (!observation.child_allocation_succeeded) {
        plan.status = PhysicsBallTaskConstructorStatus::child_allocation_failed;
        plan.child_address = 0U;
    } else {
        plan.status = PhysicsBallTaskConstructorStatus::child_constructed;
        plan.child_constructor_0x42be10_called = true;
        plan.child_address = observation.child_address;
        plan.child_resource_id = observation.child_resource_id_0x30;
        constexpr auto length_scale = std::bit_cast<float>(0x3cd013a9U);
        constexpr auto radius_scale = std::bit_cast<float>(0x432f3264U);
        plan.child_length = static_cast<float>(
            static_cast<long double>(observation.child_length_source_0x24)
                * static_cast<long double>(length_scale));
        const auto factor_scaled = static_cast<long double>(
            observation.child_radius_factor_source_0x28)
            * static_cast<long double>(radius_scale);
        const auto radius_x87 = static_cast<long double>(
            observation.child_radius_source_0x2c)
            * static_cast<long double>(observation.child_radius_source_0x2c)
            * factor_scaled;
        plan.child_radius_parameter = static_cast<float>(radius_x87);
        plan.child_writes.push_back({0U, 0x004c7c48U});
        plan.child_writes.push_back({0x08U, plan.child_base_matrix_address});
        plan.child_writes.push_back({0x04U, observation.object_address});
        plan.child_writes.push_back({0x14U, 0x53504852U});
        plan.child_writes.push_back({0x0cU,
            std::bit_cast<std::uint32_t>(plan.child_radius_parameter)});
        plan.child_writes.push_back({0x18U, 100U});
        plan.child_writes.push_back({0x10U,
            observation.child_resource_id_0x30});
        plan.child_writes.push_back({0U, 0x004c7c68U});
        plan.child_writes.push_back({0x20U, 0U});
        plan.child_writes.push_back({0x24U, 0U});
        plan.child_writes.push_back({0x28U, 0U});
        plan.child_writes.push_back({0x2cU, 0U});
        plan.child_writes.push_back({0x30U, 0U});
        plan.child_writes.push_back({0x1cU,
            std::bit_cast<std::uint32_t>(plan.child_length)});
        plan.child_writes.push_back({0x34U, 0U});
        plan.child_writes.push_back({0x38U, 0U});
        plan.child_writes.push_back({0x3cU, 0U});
    }
    return plan;
}

PhysicsObstacleTaskConstructorPlan plan_physics_obstacle_task_constructor(
    const PhysicsObstacleTaskConstructorObservation& observation)
{
    PhysicsObstacleTaskConstructorPlan plan{};
    plan.shape_selector = observation.shape_selector_0x34;
    plan.child_count_after = observation.child_count_before;
    plan.geometry_record_count_after =
        observation.geometry_record_count_before;
    plan.derived_vtable_installed = true;
    plan.object_writes.push_back({0U, 0x004c8018U});
    plan.object_writes.push_back({0x4acU, 0U});
    plan.object_writes.push_back({0x4a8U, 0U});
    // 0x436eb0 sets the active byte and replaces the timer value. The
    // projection records the observed byte effect; timer output is runtime
    // dependent and is intentionally not synthesized here.
    plan.object_writes.push_back({0x4acU, 1U});
    if (observation.timer_value_0x4a8.has_value()) {
        plan.object_writes.push_back({0x4a8U,
            *observation.timer_value_0x4a8});
    }

    constexpr float degrees_to_radians = 0.017453292F;
    const auto radians = static_cast<long double>(observation.yaw_degrees_0x24)
        * static_cast<long double>(degrees_to_radians);
    const auto cosine = static_cast<float>(std::cos(radians));
    const auto sine = static_cast<float>(std::sin(radians));
    plan.rotated_basis = {cosine, 0.0F, -sine,
                          0.0F, 1.0F, 0.0F,
                          sine, 0.0F, cosine};
    for (std::size_t index = 0U; index < plan.rotated_basis.size(); ++index) {
        plan.object_writes.push_back({
            0x38U + static_cast<std::uint32_t>(index * 4U),
            std::bit_cast<std::uint32_t>(plan.rotated_basis[index])});
    }
    for (std::size_t axis = 0U; axis < observation.position_words_0x18.size();
         ++axis) {
        plan.object_writes.push_back({
            0x5cU + static_cast<std::uint32_t>(axis * 4U),
            observation.position_words_0x18[axis]});
    }
    // The constructor clears this center before it selects a shape branch.
    plan.object_writes.push_back({0x28U, 0U});
    plan.object_writes.push_back({0x2cU, 0U});
    plan.object_writes.push_back({0x30U, 0U});

    const auto record_scale = std::bit_cast<float>(
        observation.record_scale_word_0x08);
    plan.sphere_scale = static_cast<float>(
        static_cast<long double>(record_scale) * 16.35L);

    if (observation.shape_selector_0x34 == 0U
        || observation.shape_selector_0x34 == 1U) {
        const bool derived_shape = observation.shape_selector_0x34 == 1U;
        plan.status = PhysicsObstacleTaskConstructorStatus::
            child_allocation_failed;
        plan.allocation_bytes = derived_shape ? 0x44U : 0x40U;
        plan.child_constructor_address = derived_shape
            ? 0x0042c400U : 0x0042be10U;
        plan.child_slot_offset = 0x14U + observation.child_count_before * 4U;
        plan.child_count_after = observation.child_count_before + 1U;
        plan.child_allocation_attempted = true;
        plan.child_pointer_stored = true;
        plan.child_count_incremented = true;
        if (!observation.child_allocation_succeeded) {
            plan.object_writes.push_back({plan.child_slot_offset, 0U});
        } else if (observation.child_address.has_value()) {
            plan.object_writes.push_back({plan.child_slot_offset,
                *observation.child_address});
        }
        plan.object_writes.push_back({0x474U, 1U});
        plan.object_writes.push_back({0x24U, plan.child_count_after});
        if (observation.child_allocation_succeeded) {
            plan.status = PhysicsObstacleTaskConstructorStatus::child_constructed;
            plan.child_address = observation.child_address;
            plan.child_constructor_called = true;
        } else {
            plan.child_address = 0U;
        }
    } else if (observation.shape_selector_0x34 == 2U) {
        plan.status = PhysicsObstacleTaskConstructorStatus::
            child_allocation_failed;
        plan.allocation_bytes = 0x40U;
        plan.child_constructor_address = 0x0042be10U;
        plan.child_slot_offset = 0x14U + observation.child_count_before * 4U;
        plan.child_count_after = observation.child_count_before + 1U;
        plan.child_allocation_attempted = true;
        plan.child_pointer_stored = true;
        plan.child_count_incremented = true;

        const float half_x = observation.dimension_x_0x28 * -0.5F;
        const float half_y = observation.dimension_y_0x2c * 0.5F;
        const float half_z = observation.dimension_z_0x30 * 0.5F;
        const std::array<std::array<float, 3>, 6> positions{{
            {-half_x, -half_y, -half_z},
            { half_x, -half_y, -half_z},
            {-half_x, -half_y,  half_z},
            { half_x, -half_y,  half_z},
            {0.0F,     half_y, -half_z},
            {0.0F,     half_y,  half_z}}};
        constexpr std::array<bool, 6> helper_written{
            false, false, true, false, true, true};
        for (std::size_t index = 0U; index < positions.size(); ++index) {
            PhysicsObstacleGeometryRecord record{};
            record.object_offset = 0x6cU
                + (observation.geometry_record_count_before
                   + static_cast<std::uint32_t>(index)) * 0x30U;
            // The recovered writes define words 0..10. The final DWORD in
            // each 0x30-byte record is not written by either observed path.
            record.known_word_mask = 0x07ffU;
            record.initialized_by_helper_0x4372c0 = helper_written[index];
            for (std::size_t axis = 0U; axis < 3U; ++axis) {
                record.words[axis] =
                    std::bit_cast<std::uint32_t>(positions[index][axis]);
            }
            record.words[6] = std::bit_cast<std::uint32_t>(plan.sphere_scale);
            plan.geometry_records.push_back(record);
        }
        plan.geometry_record_count_after =
            observation.geometry_record_count_before + 6U;
        plan.geometry_count_incremented = true;
        plan.object_writes.push_back({0x1ecU,
            plan.geometry_record_count_after});
        if (!observation.child_allocation_succeeded) {
            plan.object_writes.push_back({plan.child_slot_offset, 0U});
        } else if (observation.child_address.has_value()) {
            plan.object_writes.push_back({plan.child_slot_offset,
                *observation.child_address});
        }
        plan.object_writes.push_back({0x474U, 0U});
        plan.object_writes.push_back({0x24U, plan.child_count_after});
        if (observation.child_allocation_succeeded) {
            plan.status = PhysicsObstacleTaskConstructorStatus::child_constructed;
            plan.child_address = observation.child_address;
            plan.child_constructor_called = true;
        } else {
            plan.child_address = 0U;
        }
    }

    for (std::size_t index = 0U; index < plan.rotated_basis.size(); ++index) {
        plan.object_writes.push_back({
            0x478U + static_cast<std::uint32_t>(index * 4U),
            std::bit_cast<std::uint32_t>(plan.rotated_basis[index])});
    }
    for (std::size_t axis = 0U; axis < observation.position_words_0x18.size();
         ++axis) {
        plan.object_writes.push_back({
            0x49cU + static_cast<std::uint32_t>(axis * 4U),
            observation.position_words_0x18[axis]});
    }
    plan.cached_basis_copied = true;
    return plan;
}

PhysicsAcarTaskConstructorPlan plan_physics_acar_task_constructor(
    const PhysicsAcarTaskConstructorObservation& observation)
{
    PhysicsAcarTaskConstructorPlan plan{};
    plan.derived_vtable_installed = true;
    plan.first_instance_panic_handler_registered =
        observation.registration_count_before == 0U;
    plan.registration_count_after = observation.registration_count_before + 1U;
    plan.profile_name_mismatch_reported = !observation.profile_name_matches;
    plan.model_data_present = observation.model_data_present;
    plan.object_writes.push_back({0U, 0x004c7b78U});
    for (std::uint32_t index = 0U; index < 7U; ++index) {
        plan.object_writes.push_back({0x1054U + index * 4U, 0U});
    }
    plan.object_writes.push_back({0x4f4U, 2U});
    plan.object_writes.push_back({0xf58U, observation.ai_profile_address});
    plan.object_writes.push_back({0xf50U, observation.selected_profile_word});
    // This is a byte store in the binary; the generic field-write record
    // preserves its low-byte value without claiming semantics for neighbors.
    plan.object_writes.push_back({0xf54U,
        static_cast<std::uint32_t>(observation.selected_driver_flag)});
    if (observation.model_data_object_address.has_value()) {
        plan.object_writes.push_back({0xfe8U,
            *observation.model_data_object_address});
        if (*observation.model_data_object_address == 0U) {
            plan.model_lookup_null_pointer_fault = true;
            return plan;
        }
    } else if (observation.model_data_present) {
        plan.model_helper_input_capture_incomplete = true;
    }

    if (!observation.model_data_present) {
        plan.object_writes.push_back({0xfe8U, 0U});
    }

    if (observation.model_data_present) {
        plan.model_descriptor_allocation_attempted = true;
        plan.model_descriptor_allocation_bytes =
            observation.model_descriptor_count * 0x18U;
        plan.object_writes.push_back({0x1058U,
            observation.model_descriptor_count});
        if (!observation.model_descriptor_allocation_succeeded) {
            plan.object_writes.push_back({0x1054U, 0U});
        } else if (observation.model_descriptor_buffer_address.has_value()) {
            plan.model_descriptor_buffer_address =
                observation.model_descriptor_buffer_address;
            plan.object_writes.push_back({0x1054U,
                *observation.model_descriptor_buffer_address});
        }
        // The native constructor calls this helper even when allocation
        // returns null; a nonempty descriptor loop then faults on that buffer.
        plan.model_helper_0x429440_called = true;
        if (!observation.model_descriptor_allocation_succeeded
            && observation.model_descriptor_count > 0U) {
            plan.model_helper_null_buffer_fault = true;
        } else if (observation.model_descriptor_allocation_succeeded) {
            const bool blend_lookup_attempted =
                !observation.helper_0x41b020_nonzero
                && !observation.helper_0x426e50_nonzero;
            const bool blend_data_available = blend_lookup_attempted
                && observation.blend_source_result_available;
            for (std::uint32_t index = 0U;
                 index < observation.model_descriptor_count; ++index) {
                PhysicsAcarModelDescriptorProjection descriptor{};
                constexpr std::uint32_t base = 0U;
                if (observation.model_descriptor_buffer_address.has_value()) {
                    descriptor.descriptor_address =
                        *observation.model_descriptor_buffer_address
                        + index * 0x18U;
                }
                descriptor.writes.push_back({base, 0U});
                descriptor.writes.push_back({base + 2U, 0U});
                if (blend_data_available
                    && index < observation.blend_source_pairs.size()) {
                    descriptor.copied_blend_pair = true;
                    descriptor.writes.push_back({base + 4U,
                        observation.blend_source_pairs[index][0]});
                    descriptor.writes.push_back({base + 8U,
                        observation.blend_source_pairs[index][1]});
                } else if (!blend_data_available) {
                    descriptor.writes.push_back({base + 4U, 0U});
                    descriptor.writes.push_back({base + 8U, 0U});
                } else {
                    plan.model_helper_input_capture_incomplete = true;
                }
                descriptor.writes.push_back({base + 0x0cU, 0U});
                if (index > 0U
                    && index - 1U < observation.model_nodes.size()
                    && observation.model_nodes[index - 1U].next_address == 0U) {
                    plan.model_helper_null_node_fault = true;
                    plan.model_descriptors.push_back(std::move(descriptor));
                    break;
                }
                if (index >= observation.model_nodes.size()) {
                    plan.model_helper_input_capture_incomplete = true;
                    plan.model_descriptors.push_back(std::move(descriptor));
                    break;
                }
                const auto& node = observation.model_nodes[index];
                if (node.address == 0U
                    || (index > 0U
                        && observation.model_nodes[index - 1U].next_address
                            != node.address)) {
                    plan.model_helper_input_capture_incomplete = true;
                    plan.model_helper_null_node_fault = node.address == 0U
                        || observation.model_nodes[index - 1U].next_address == 0U;
                    plan.model_descriptors.push_back(std::move(descriptor));
                    break;
                }
                descriptor.source_node_address = node.address;
                auto mapped = 1.0L
                    - static_cast<long double>(node.scalar_at_0x1c) * 20.0L;
                const float rounded = static_cast<float>(mapped);
                if (!(rounded >= 0.0F)) {
                    mapped = 0.0L;
                } else if (rounded > 1.0F) {
                    mapped = 1.0L;
                } else {
                    mapped = static_cast<long double>(rounded);
                }
                descriptor.mapped_scalar = static_cast<float>(
                    mapped * 0.39999998L + 0.3L);
                descriptor.writes.push_back({base + 0x10U,
                    std::bit_cast<std::uint32_t>(descriptor.mapped_scalar)});
                descriptor.writes.push_back({base + 0x14U, node.address});
                plan.model_descriptors.push_back(std::move(descriptor));
            }
            if (blend_data_available
                && observation.blend_source_pairs.size()
                    < observation.model_descriptor_count) {
                plan.model_helper_input_capture_incomplete = true;
            }
        }
        if (plan.model_helper_null_buffer_fault
            || plan.model_helper_null_node_fault) {
            return plan;
        }
    } else {
        plan.object_writes.push_back({0xfe8U, 0U});
    }

    plan.object_writes.push_back({0x104cU,
        observation.runtime_entry_count});
    plan.runtime_entry_allocation_bytes =
        observation.runtime_entry_count * 8U;
    plan.runtime_entry_allocation_attempted = true;
    if (!observation.runtime_entry_allocation_succeeded) {
        plan.object_writes.push_back({0x1048U, 0U});
    } else {
        plan.runtime_entry_initializers_called =
            observation.runtime_entry_count;
        if (observation.runtime_entry_buffer_address.has_value()) {
            plan.runtime_entry_buffer_address =
                observation.runtime_entry_buffer_address;
            plan.object_writes.push_back({0x1048U,
                *observation.runtime_entry_buffer_address});
        }
    }

    plan.animation_helpers_called = true;
    plan.timer_threshold_10_selected =
        static_cast<long double>(observation.timer_threshold_0xf0c)
            - static_cast<long double>(observation.timer_baseline_0xfcc)
            >= 10.0L;
    plan.object_writes.push_back({0x4f8U,
        plan.timer_threshold_10_selected ? 1U : 0U});

    const auto speed_word_as_signed = std::bit_cast<std::int32_t>(
        observation.speed_threshold_word_0xf10);
    plan.speed_or_driver_gate_selected = speed_word_as_signed < 0x42055555
        || observation.selected_driver_flag != 0U;
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        plan.object_writes.push_back({0x6b0U + index * 0x1a4U,
            plan.speed_or_driver_gate_selected ? 1U : 0U});
    }
    for (std::uint32_t index = 0U; index < 4U; ++index) {
        plan.object_writes.push_back({0x6b8U + index * 0x1a4U, 2U});
    }
    plan.object_writes.push_back({0x534U, 1U});
    plan.final_setup_helper_0x428790_called = true;
    if (observation.curve_source_record.has_value()) {
        plan.configured_curve_controller = configure_acar_update_curve(
            observation.object_address, *observation.curve_source_record,
            observation.fdiv_compatibility_enabled);
    }
    return plan;
}

PhysicsAcarRuntimeInitializationPlan plan_physics_acar_runtime_initialization(
    const PhysicsAcarRuntimeInitializationObservation& observation)
{
    PhysicsAcarRuntimeInitializationPlan plan{};
    plan.fdiv_compatibility_path_used =
        observation.fdiv_compatibility_enabled;
    const auto write = [&plan](const std::uint32_t offset,
                               const std::uint32_t value) {
        plan.object_writes.push_back({offset, value});
    };
    const auto write_float = [&write](const std::uint32_t offset,
                                      const float value) {
        write(offset, std::bit_cast<std::uint32_t>(value));
    };

    write(0x107cU, 0xc2c80000U);
    write(0xfe0U, 0U);
    write(0x1070U, 0U);
    const auto config_value = static_cast<float>(
        static_cast<long double>(observation.selected_config_value_0x00)
        + static_cast<long double>(observation.selected_config_value_0x04)
            * static_cast<long double>(observation.car_index_0x510));
    write_float(0x1078U, config_value);
    const auto index_bits = static_cast<std::uint32_t>(
        observation.car_index_0x510);
    const auto shared_state = observation.shared_state_array_base
        + index_bits * 0x74U + 0x78U;
    write(0x1020U, 0U);
    write(0x1038U, shared_state);
    write(0x1034U, 0U);
    write(0x1040U, 0U);
    write(0x1024U, 0U);
    write(0x103cU, 0x3f800000U);
    write(0x1028U, observation.profile_value_0x98);
    write(0x102cU, observation.profile_value_0xa4);

    const bool have_last_descriptor_data =
        observation.linked_object_value_0x18.has_value()
        && observation.last_model_node_value_0x18.has_value()
        && observation.last_model_node_value_0x20.has_value()
        && observation.last_model_node_value_0x38.has_value();
    std::uint32_t selected_profile_value = observation.profile_value_0x98;
    if (have_last_descriptor_data) {
        const auto denominator =
            static_cast<long double>(*observation.linked_object_value_0x18)
            + static_cast<long double>(
                *observation.last_model_node_value_0x18) * 0.5L;
        const auto ratio = divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(*observation.last_model_node_value_0x20),
            denominator, observation.fdiv_compatibility_enabled);
        const auto threshold = static_cast<float>(
            ratio + static_cast<long double>(
                *observation.last_model_node_value_0x38));
        plan.derived_threshold_0x1044 = threshold;
        write_float(0x1044U, threshold);
        // The original uses a raw unsigned bit threshold to decide whether
        // to replace profile +0x98 with the calculated limit.
        plan.derived_value_selected = observation.profile_value_0x98
            > 0x80000000U;
        if (plan.derived_value_selected) {
            selected_profile_value = std::bit_cast<std::uint32_t>(threshold);
            write(0x1028U, selected_profile_value);
        }
        const auto normalized = divide_with_vc98_fdiv_compatibility(
            static_cast<long double>(std::bit_cast<float>(selected_profile_value)),
            static_cast<long double>(threshold),
            observation.fdiv_compatibility_enabled);
        plan.normalized_value_0x1030 = static_cast<float>(normalized);
        write_float(0x1030U, *plan.normalized_value_0x1030);
    } else {
        plan.derived_value_inputs_missing = true;
    }

    write(0xfd4U, 0U);
    write(0xf00U, 0U);
    write(0xfd0U, 0xc47a0000U);
    write(0xfc0U, 0xc47a0000U);
    if (observation.timer_value_0xfb8.has_value()) {
        write(0xfb8U, *observation.timer_value_0xfb8);
    }
    write(0xfc8U, 0xc47a0000U);
    write(0xfbcU, 0U);
    write(0xfc4U, 0xc47a0000U);
    write(0xf38U, 0U);
    write(0x1050U, 0U);
    write(0xf48U, 0U);
    write(0x1069U, 0U);
    write(0xfe4U, 0xc2c80000U);
    write(0xf4cU, 0U);
    write(0xef0U, 0U);
    write(0xf34U, 0U);
    write(0xfe4U, observation.current_value_0xf0c);

    plan.model_sync_helper_0x41d170_called = true;
    if (!observation.model_object_pointer_nonnull) {
        plan.model_sync_null_receiver_fault = true;
        return plan;
    }
    plan.model_object_address = observation.model_object_address;
    auto model_fraction = observation.model_fraction_0x08;
    const auto call_curve_seek = [&]() {
        plan.model_reset_helper_call_count += 1U;
        plan.model_object_helper_0x41d1d0_called = true;
        plan.model_curve_seek_0x41d3a0_call_count += 1U;
        // 0x41D1D0 clears the linked-node cursor before it enters 0x41D3A0.
        plan.model_object_writes.push_back({0x04U, 0U});
        if (!observation.model_curve_seek_observation.has_value()) {
            plan.model_curve_seek_input_incomplete = true;
            return false;
        }
        auto seek_observation = *observation.model_curve_seek_observation;
        seek_observation.cursor_node_address = 0U;
        seek_observation.fraction_0x08 = model_fraction;
        seek_observation.maximum_distance = plan.model_curve_seek_limit;
        auto seek_plan = plan_acar_curve_distance(seek_observation);
        plan.model_curve_seek_plans.push_back(seek_plan);
        if (!seek_plan.output_written) {
            if (seek_plan.status == AcarCurveDistanceStatus::
                    null_node_dereference) {
                plan.model_curve_seek_null_node_fault = true;
            } else {
                plan.model_curve_seek_input_incomplete = true;
            }
            return false;
        }
        plan.model_object_writes.push_back({0x04U,
            seek_plan.cursor_node_address});
        plan.model_object_writes.push_back({0x08U,
            std::bit_cast<std::uint32_t>(seek_plan.fraction_0x08)});
        model_fraction = seek_plan.fraction_0x08;
        return true;
    };
    const auto record_unmodeled_curve_seek = [&]() {
        plan.model_reset_helper_call_count += 1U;
        plan.model_object_helper_0x41d1d0_called = true;
        plan.model_curve_seek_0x41d3a0_call_count += 1U;
        plan.model_object_writes.push_back({0x04U, 0U});
        plan.model_curve_seek_input_incomplete = true;
    };

    if (observation.model_object_has_records) {
        plan.model_object_writes.push_back({0x18U,
            observation.object_position_xz[0]});
        plan.model_object_writes.push_back({0x1cU,
            observation.object_position_xz[1]});
        plan.model_object_writes.push_back({0x20U,
            observation.object_vector_0x234_0x23c[0]});
        plan.model_object_writes.push_back({0x24U,
            observation.object_vector_0x234_0x23c[1]});
        if (observation.model_object_reset_flag_0x28) {
            if (!call_curve_seek()) {
                return plan;
            }
            plan.model_object_writes.push_back({0x28U, 0U});
            plan.model_object_reset_flag_cleared = true;
        } else {
            plan.model_curve_update_0x41d4f0_called = true;
            if (observation.model_curve_observation.has_value()) {
                plan.model_curve_update_plan = plan_acar_curve_tracker_update(
                    *observation.model_curve_observation);
                if (!plan.model_curve_update_plan->output_written) {
                    if (plan.model_curve_update_plan->status
                        == AcarCurveTrackerStatus::null_node_dereference) {
                        plan.model_curve_update_null_node_fault = true;
                        return plan;
                    }
                    plan.model_curve_update_input_incomplete = true;
                    // The caller would still run its unconditional reset helper,
                    // but its fraction input depends on this missing result.
                    record_unmodeled_curve_seek();
                    return plan;
                }
                plan.model_object_writes.push_back({0x04U,
                    plan.model_curve_update_plan->segment_node_address});
                plan.model_object_writes.push_back({0x08U,
                    std::bit_cast<std::uint32_t>(
                        plan.model_curve_update_plan->fraction_0x08)});
                plan.model_object_writes.push_back({0x30U,
                    plan.model_curve_update_plan->segment_valid ? 1U : 0U});
                model_fraction =
                    plan.model_curve_update_plan->fraction_0x08;
            } else {
                plan.model_curve_update_input_incomplete = true;
                record_unmodeled_curve_seek();
                return plan;
            }
        }
    }

    // 0x428790 invokes 0x41d1d0 again after 0x429690 returns, regardless of
    // whether 0x41d170 selected its reset or curve-update branch.
    if (!call_curve_seek()) {
        return plan;
    }

    if (observation.runtime_entry_count > 0U
        && !observation.runtime_entry_buffer_available) {
        plan.runtime_entry_clear_fault = true;
    } else if (observation.runtime_entry_buffer_address.has_value()) {
        for (std::uint32_t index = 0U;
             index < observation.runtime_entry_count; ++index) {
            plan.runtime_entry_writes.push_back({
                *observation.runtime_entry_buffer_address + index * 8U, 0U});
        }
        plan.cleared_runtime_entries = observation.runtime_entry_count;
    } else if (observation.runtime_entry_buffer_available) {
        // The initialized entries are known to be cleared even though their
        // captured base address is unavailable.
        plan.cleared_runtime_entries = observation.runtime_entry_count;
    }
    if (!plan.runtime_entry_clear_fault) {
        write(0xec8U, 0x0042b7b0U);
        write(0xeccU, 0x0042ad80U);
        plan.final_helper_0x428590_called = true;
    }
    return plan;
}

PhysicsTaskRecordLoadPlan plan_physics_task_record_load(
    const PhysicsTaskRecordLoadObservation& observation) noexcept
{
    PhysicsTaskRecordLoadPlan plan{};
    if (observation.serialized_object_stream_offset < 0) {
        plan.status = PhysicsTaskRecordLoadStatus::end_of_records;
        return plan;
    }

    plan.constructor_argument_2_stream_offset = static_cast<std::uint32_t>(
        observation.serialized_object_stream_offset);

    plan.payload_read = true;
    if (!observation.payload_complete) {
        plan.status = PhysicsTaskRecordLoadStatus::incomplete_payload;
        plan.stream_error_flag_set = true;
        // 0x4C5EA0 rejects the incomplete span and sets the stream error byte,
        // but 0x4224D0 never checks it before attempting factory dispatch.
        plan.factory_call_attempted_after_read_error = true;
        plan.factory_input_indeterminate = true;
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
