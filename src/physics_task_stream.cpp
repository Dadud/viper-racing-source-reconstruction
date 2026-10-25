#include "viper/physics_task_stream.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace viper {

std::optional<std::uint32_t> physics_task_stream_advance_for_tag(
    const std::uint32_t record_tag) noexcept {
    switch (record_tag) {
    // Entries recovered from the lookup table at 0x4D4890, read by 0x4209E0.
    case 0x00434152U: // RAC\0
        return 0x18CU;
    case 0x47434152U: // GCAR
        return 0x190U;
    case 0x50434152U: // PCAR
    case 0x41434152U: // ACAR
    case 0x4E434152U: // NCAR
        return 0x18CU;
    case 0x53544154U: // STAT: scene record has no physics-task cursor advance
        return 0U;
    case 0x4F425354U: // OBST
    case 0x574F424CU: // WOBL, immediate stored by 0x45D5D0
    case 0x43484B50U: // CHKP
    case 0x42414C4CU: // BALL
        return 0x34U;
    default:
        return std::nullopt;
    }
}

PhysicsTaskStreamPlan plan_physics_task_stream_offsets(
    const std::span<const std::uint32_t> ordered_record_tags,
    const std::uint32_t initial_cursor) {
    PhysicsTaskStreamPlan plan;
    plan.initial_cursor = initial_cursor;
    plan.final_cursor = initial_cursor;
    plan.records.reserve(ordered_record_tags.size());

    for (std::size_t index = 0U; index < ordered_record_tags.size(); ++index) {
        const auto tag = ordered_record_tags[index];
        const auto advance = physics_task_stream_advance_for_tag(tag);
        if (!advance.has_value()) {
            plan.status = PhysicsTaskStreamPlanStatus::unknown_record_tag;
            plan.failed_record_index = index;
            return plan;
        }
        if (*advance > std::numeric_limits<std::uint32_t>::max()
                - plan.final_cursor) {
            plan.status = PhysicsTaskStreamPlanStatus::cursor_overflow;
            plan.failed_record_index = index;
            return plan;
        }
        plan.records.push_back({tag, plan.final_cursor, *advance});
        plan.final_cursor += *advance;
    }

    return plan;
}

PhysicsTaskRecordSerialization serialize_physics_task_records(
    const std::span<const PhysicsTaskRecordInput> records,
    const std::uint32_t initial_cursor) {
    PhysicsTaskRecordSerialization result{};
    result.initial_cursor = initial_cursor;
    result.final_cursor = initial_cursor;
    result.records.reserve(records.size());

    const auto read_u32_le = [](const std::span<const std::byte> bytes,
                                const std::size_t offset) noexcept {
        return std::to_integer<std::uint32_t>(bytes[offset])
            | (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U)
            | (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U)
            | (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
    };
    const auto append_u32_le = [&result](const std::uint32_t value) {
        result.bytes.push_back(static_cast<std::byte>(value & 0xFFU));
        result.bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
        result.bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
        result.bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
    };

    for (std::size_t index = 0U; index < records.size(); ++index) {
        const auto bytes = records[index].record_bytes;
        if (result.final_cursor > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max())) {
            result.status = PhysicsTaskRecordSerializationStatus::cursor_overflow;
            result.failed_record_index = index;
            return result;
        }
        if (bytes.size() < 8U) {
            result.status = PhysicsTaskRecordSerializationStatus::malformed_record_header;
            result.failed_record_index = index;
            return result;
        }

        const auto tag = read_u32_le(bytes, 0U);
        const auto declared_size = read_u32_le(bytes, 4U);
        if (declared_size != bytes.size()) {
            result.status = PhysicsTaskRecordSerializationStatus::declared_size_mismatch;
            result.failed_record_index = index;
            return result;
        }

        const auto advance = physics_task_stream_advance_for_tag(tag);
        if (!advance.has_value()) {
            result.status = PhysicsTaskRecordSerializationStatus::unknown_record_tag;
            result.failed_record_index = index;
            return result;
        }
        if (*advance > std::numeric_limits<std::uint32_t>::max() - result.final_cursor) {
            result.status = PhysicsTaskRecordSerializationStatus::cursor_overflow;
            result.failed_record_index = index;
            return result;
        }
        if (result.bytes.size() > result.bytes.max_size() - 4U ||
            bytes.size() > result.bytes.max_size() - result.bytes.size() - 4U) {
            result.status = PhysicsTaskRecordSerializationStatus::output_size_overflow;
            result.failed_record_index = index;
            return result;
        }

        result.records.push_back({tag, result.final_cursor, declared_size, *advance});
        append_u32_le(result.final_cursor);
        result.bytes.insert(result.bytes.end(), bytes.begin(), bytes.end());
        result.final_cursor += *advance;
    }
    return result;
}

PhysicsTaskRecordSerialization PhysicsTaskRecordStreamSession::append_record(
    const std::span<const std::byte> record_bytes) {
    const PhysicsTaskRecordInput input{record_bytes};
    auto appended = serialize_physics_task_records(
        std::span<const PhysicsTaskRecordInput>(&input, 1U), final_cursor_);
    appended.initial_cursor = final_cursor_;
    if (appended.status != PhysicsTaskRecordSerializationStatus::complete) {
        appended.records.clear();
        appended.bytes.clear();
        appended.final_cursor = final_cursor_;
        appended.failed_record_index = records_.size();
        return appended;
    }

    if (records_.size() == records_.max_size() ||
        appended.bytes.size() > bytes_.max_size() - bytes_.size()) {
        appended.status = PhysicsTaskRecordSerializationStatus::output_size_overflow;
        appended.records.clear();
        appended.bytes.clear();
        appended.final_cursor = final_cursor_;
        appended.failed_record_index = records_.size();
        return appended;
    }

    bytes_.insert(bytes_.end(), appended.bytes.begin(), appended.bytes.end());
    records_.push_back(appended.records.front());
    final_cursor_ = appended.final_cursor;
    return appended;
}

PhysicsTaskRecordSerialization PhysicsTaskRecordStreamSession::snapshot() const {
    PhysicsTaskRecordSerialization result{};
    result.initial_cursor = initial_cursor_;
    result.final_cursor = final_cursor_;
    result.records = records_;
    result.bytes = bytes_;
    return result;
}

PhysicsTaskRecordStreamRead read_physics_task_record_stream(
    const std::span<const std::byte> bytes) {
    PhysicsTaskRecordStreamRead result{};
    std::size_t input_offset = 0U;
    std::optional<std::uint32_t> expected_cursor;
    const auto read_u32_le = [bytes](const std::size_t offset) noexcept {
        return std::to_integer<std::uint32_t>(bytes[offset])
            | (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U)
            | (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U)
            | (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
    };

    while (input_offset < bytes.size()) {
        if (bytes.size() - input_offset < sizeof(std::uint32_t)) {
            result.status = PhysicsTaskRecordStreamReadStatus::incomplete_cursor;
            result.consumed_bytes = input_offset;
            result.failed_record_index = result.records.size();
            return result;
        }
        const auto signed_cursor = std::bit_cast<std::int32_t>(
            read_u32_le(input_offset));
        if (signed_cursor < 0) {
            result.status = input_offset + sizeof(std::uint32_t) == bytes.size()
                ? PhysicsTaskRecordStreamReadStatus::end_of_stream
                : PhysicsTaskRecordStreamReadStatus::trailing_bytes_after_end_of_stream;
            result.consumed_bytes = input_offset + sizeof(std::uint32_t);
            result.final_cursor = expected_cursor.value_or(0U);
            return result;
        }
        const auto cursor = static_cast<std::uint32_t>(signed_cursor);
        if (expected_cursor.has_value() && cursor != *expected_cursor) {
            result.status = PhysicsTaskRecordStreamReadStatus::cursor_mismatch;
            result.consumed_bytes = input_offset;
            result.final_cursor = *expected_cursor;
            result.failed_record_index = result.records.size();
            return result;
        }

        const auto record_offset = input_offset + sizeof(std::uint32_t);
        if (bytes.size() - record_offset < 8U) {
            result.status = PhysicsTaskRecordStreamReadStatus::incomplete_record_header;
            result.consumed_bytes = input_offset;
            result.final_cursor = cursor;
            result.failed_record_index = result.records.size();
            return result;
        }
        const auto tag = read_u32_le(record_offset);
        const auto record_size = read_u32_le(record_offset + 4U);
        const auto available_record_bytes = bytes.size() - record_offset;
        if (record_size < 8U || record_size > available_record_bytes) {
            result.status = PhysicsTaskRecordStreamReadStatus::incomplete_record_payload;
            result.consumed_bytes = input_offset;
            result.final_cursor = cursor;
            result.failed_record_index = result.records.size();
            return result;
        }
        const auto advance = physics_task_stream_advance_for_tag(tag);
        if (!advance.has_value()) {
            result.status = PhysicsTaskRecordStreamReadStatus::unknown_record_tag;
            result.consumed_bytes = input_offset;
            result.final_cursor = cursor;
            result.failed_record_index = result.records.size();
            return result;
        }
        if (*advance > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()) - cursor) {
            result.status = PhysicsTaskRecordStreamReadStatus::cursor_not_representable;
            result.consumed_bytes = input_offset;
            result.final_cursor = cursor;
            result.failed_record_index = result.records.size();
            return result;
        }

        result.records.push_back({tag, cursor, record_size, *advance, record_offset});
        expected_cursor = cursor + *advance;
        result.final_cursor = *expected_cursor;
        input_offset = record_offset + record_size;
    }
    result.status = result.records.empty()
        ? PhysicsTaskRecordStreamReadStatus::end_of_stream
        : PhysicsTaskRecordStreamReadStatus::complete;
    result.consumed_bytes = input_offset;
    return result;
}

PlayerPcarTaskBindingResult bind_player_pcar_task_record(
    const PhysicsTaskRecordStreamRead& decoded_stream,
    const std::span<const std::byte> serialized_stream_bytes,
    const std::span<const std::byte> expected_player_record,
    const std::uint32_t expected_cursor)
{
    PlayerPcarTaskBindingResult result{};
    if (decoded_stream.status != PhysicsTaskRecordStreamReadStatus::complete ||
        decoded_stream.consumed_bytes != serialized_stream_bytes.size() ||
        expected_player_record.size() != 0x1e4U) {
        result.status = PlayerPcarTaskBindingStatus::invalid_stream;
        return result;
    }

    std::optional<std::size_t> player_record_index;
    for (std::size_t index = 0U; index < decoded_stream.records.size(); ++index) {
        if (decoded_stream.records[index].record_tag != 0x50434152U) continue;
        if (player_record_index.has_value()) {
            result.status = PlayerPcarTaskBindingStatus::duplicate_player_records;
            return result;
        }
        player_record_index = index;
    }
    if (!player_record_index.has_value()) {
        result.status = PlayerPcarTaskBindingStatus::missing_player_record;
        return result;
    }

    const auto& record = decoded_stream.records[*player_record_index];
    if (record.cursor_offset != expected_cursor) {
        result.status = PlayerPcarTaskBindingStatus::cursor_mismatch;
        return result;
    }
    if (record.record_size != expected_player_record.size()) {
        result.status = PlayerPcarTaskBindingStatus::record_size_mismatch;
        return result;
    }
    if (record.record_bytes_offset > serialized_stream_bytes.size() ||
        record.record_size > serialized_stream_bytes.size() - record.record_bytes_offset ||
        !std::equal(expected_player_record.begin(), expected_player_record.end(),
            serialized_stream_bytes.begin() + static_cast<std::ptrdiff_t>(
                record.record_bytes_offset))) {
        result.status = PlayerPcarTaskBindingStatus::record_bytes_mismatch;
        return result;
    }

    result.status = PlayerPcarTaskBindingStatus::complete;
    result.binding = PlayerPcarTaskBinding{
        *player_record_index, record.cursor_offset,
        record.record_bytes_offset, record.record_size};
    return result;
}

} // namespace viper
