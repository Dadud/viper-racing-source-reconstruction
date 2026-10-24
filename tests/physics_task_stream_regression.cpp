#include "viper/physics_task_stream.hpp"
#include "viper/physics_task_factory.hpp"
#include "viper/world_track_objects.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

viper::TrackObjectTextTable make_track_object_table(
    const std::initializer_list<std::string_view> lines)
{
    viper::TrackObjectTextTable table{};
    table.line_count = static_cast<std::uint32_t>(lines.size());
    table.line_stride = 64U;
    table.line_offsets.fill(0U);
    table.raw_payload.resize(viper::TrackObjectTextTable::header_size +
        lines.size() * table.line_stride);
    std::size_t index = 0U;
    for (const auto line : lines) {
        const auto offset = viper::TrackObjectTextTable::header_size +
            index * table.line_stride;
        const auto length = std::min(line.size(),
            static_cast<std::size_t>(table.line_stride - 1U));
        std::memcpy(table.raw_payload.data() +
            static_cast<std::ptrdiff_t>(offset), line.data(), length);
        ++index;
    }
    return table;
}

std::vector<std::byte> make_physics_record(
    const std::uint32_t tag, const std::size_t size)
{
    std::vector<std::byte> bytes(size);
    const auto write_u32 = [&bytes](const std::size_t offset,
                                    const std::uint32_t value) {
        for (std::size_t index = 0U; index < 4U; ++index) {
            bytes[offset + index] = static_cast<std::byte>(
                (value >> (index * 8U)) & 0xFFU);
        }
    };
    write_u32(0U, tag);
    write_u32(4U, static_cast<std::uint32_t>(size));
    for (std::size_t index = 8U; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>(index & 0xFFU);
    }
    return bytes;
}

std::uint32_t read_u32_le(const std::span<const std::byte> bytes,
                          const std::size_t offset)
{
    return std::to_integer<std::uint32_t>(bytes[offset])
        | (std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U)
        | (std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U)
        | (std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

} // namespace

int main()
{
    try {
        constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 10> sizes{{
            {0x00434152U, 0x18CU}, // RAC\0
            {0x47434152U, 0x190U}, // GCAR
            {0x50434152U, 0x18CU}, // PCAR
            {0x41434152U, 0x18CU}, // ACAR
            {0x4E434152U, 0x18CU}, // NCAR
            {0x53544154U, 0U},     // STAT
            {0x4F425354U, 0x34U},  // OBST
            {0x4C424F57U, 0x34U},  // WOBL
            {0x43484B50U, 0x34U},  // CHKP
            {0x42414C4CU, 0x34U},  // BALL
        }};
        for (const auto [tag, expected] : sizes) {
            const auto actual = viper::physics_task_stream_advance_for_tag(tag);
            require(actual.has_value() && *actual == expected,
                    "resolver tag advance differs from recovered table");
        }
        require(!viper::physics_task_stream_advance_for_tag(0x12345678U),
                "unknown record tag was assigned a cursor advance");

        constexpr std::array<std::uint32_t, 5> tags{
            0x50434152U, 0x53544154U, 0x4F425354U, 0x47434152U, 0x42414C4CU};
        const auto plan = viper::plan_physics_task_stream_offsets(tags);
        require(plan.status == viper::PhysicsTaskStreamPlanStatus::complete &&
                    plan.initial_cursor == 0U && plan.records.size() == tags.size() &&
                    plan.final_cursor == 0x384U,
                "default-zero stream cursor fold is incorrect");
        require(plan.records[0].stream_offset == 0U &&
                    plan.records[1].stream_offset == 0x18CU &&
                    plan.records[2].stream_offset == 0x18CU &&
                    plan.records[3].stream_offset == 0x1C0U &&
                    plan.records[4].stream_offset == 0x350U,
                "record offsets do not follow native class-size advances");

        auto car_record = make_physics_record(0x50434152U, 0x1E4U);
        auto static_record = make_physics_record(0x53544154U, 0x48U);
        auto obstacle_record = make_physics_record(0x4F425354U, 0x38U);
        const std::array<viper::PhysicsTaskRecordInput, 3> input_records{{
            {car_record}, {static_record}, {obstacle_record}}};
        const auto serialized = viper::serialize_physics_task_records(
            input_records, 0x100U);
        constexpr std::size_t static_entry = 4U + 0x1E4U;
        constexpr std::size_t obstacle_entry = static_entry + 4U + 0x48U;
        require(serialized.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    serialized.records.size() == 3U &&
                    serialized.records[0].cursor_offset == 0x100U &&
                    serialized.records[1].cursor_offset == 0x28CU &&
                    serialized.records[2].cursor_offset == 0x28CU &&
                    serialized.records[2].cursor_advance == 0x34U &&
                    serialized.final_cursor == 0x2C0U,
                "record serializer must use native cursor advances, not payload sizes");
        require(read_u32_le(serialized.bytes, 0U) == 0x100U &&
                    read_u32_le(serialized.bytes, 4U) == 0x50434152U &&
                    read_u32_le(serialized.bytes, static_entry) == 0x28CU &&
                    read_u32_le(serialized.bytes, static_entry + 4U) == 0x53544154U &&
                    read_u32_le(serialized.bytes, obstacle_entry) == 0x28CU &&
                    read_u32_le(serialized.bytes, obstacle_entry + 4U) == 0x4F425354U &&
                    serialized.bytes.size() == obstacle_entry + 4U + 0x38U &&
                    std::equal(car_record.begin(), car_record.end(),
                        serialized.bytes.begin() + 4),
                "serializer must frame each complete record with a little-endian cursor");

        const std::array<std::byte, 7> short_record{};
        const std::array<viper::PhysicsTaskRecordInput, 1> short_input{{
            {short_record}}};
        const auto malformed = viper::serialize_physics_task_records(short_input);
        require(malformed.status == viper::PhysicsTaskRecordSerializationStatus::
                    malformed_record_header && malformed.bytes.empty(),
                "short serialized record must be rejected without output bytes");

        auto mismatched_record = make_physics_record(0x43484B50U, 8U);
        mismatched_record[4] = std::byte{9U};
        const std::array<viper::PhysicsTaskRecordInput, 1> mismatch_input{{
            {mismatched_record}}};
        const auto mismatch = viper::serialize_physics_task_records(mismatch_input);
        require(mismatch.status == viper::PhysicsTaskRecordSerializationStatus::
                    declared_size_mismatch && mismatch.bytes.empty(),
                "record size mismatch must be rejected before serialization");

        auto unknown_record = make_physics_record(0x12345678U, 8U);
        const std::array<viper::PhysicsTaskRecordInput, 1> unknown_input{{
            {unknown_record}}};
        const auto unknown_serialized =
            viper::serialize_physics_task_records(unknown_input);
        require(unknown_serialized.status ==
                    viper::PhysicsTaskRecordSerializationStatus::unknown_record_tag &&
                    unknown_serialized.bytes.empty(),
                "unknown class must not receive a fabricated cursor advance");

        const auto serialization_overflow = viper::serialize_physics_task_records(
            std::span<const viper::PhysicsTaskRecordInput>(input_records.data(), 1U),
            std::numeric_limits<std::uint32_t>::max() - 0x100U);
        require(serialization_overflow.status ==
                    viper::PhysicsTaskRecordSerializationStatus::cursor_overflow &&
                    serialization_overflow.bytes.empty(),
                "cursor overflow must reject a record before appending its bytes");

        const auto bemidji_prefix = make_track_object_table({
            "out\\track.txt", "map test.map", "ai test.vf",
            "obj car 1, 1", "obj car 2, 2", "obj car 3, 3", "obj car 4, 4",
            "obj car 5, 5", "obj car 6, 6", "obj car 7, 7", "obj car 8, 8",
            "obj checkpoint finish 1,1 2,2"});
        const auto player_cursor =
            viper::plan_track_player_physics_cursor(bemidji_prefix, 7U);
        require(player_cursor.status == viper::TrackPhysicsCursorStatus::complete &&
                    player_cursor.serialized_object_stream_offset == 0xAD4U &&
                    player_cursor.preceding_car_count == 7U,
                "Bemidji player cursor must fold seven preceding ACAR records to 0xAD4");

        const auto interleaved_object = make_track_object_table({
            "obj car 1, 1", "obj unknown_physics_object 2, 2", "obj car 3, 3"});
        const auto unresolved_cursor =
            viper::plan_track_player_physics_cursor(interleaved_object, 1U);
        require(unresolved_cursor.status ==
                    viper::TrackPhysicsCursorStatus::unsupported_prior_object &&
                    unresolved_cursor.unsupported_object_row == 1U &&
                    !unresolved_cursor.serialized_object_stream_offset.has_value(),
                "unmapped prior object must leave the player stream cursor unresolved");

        const auto mapped_interleaving = make_track_object_table({
            "obj wobble pole 0", "obj car 1, 1", "obj static flag 0 0 0 0 0 0 0 0 0",
            "obj car 2, 2"});
        const auto mapped_cursor =
            viper::plan_track_player_physics_cursor(mapped_interleaving, 1U);
        require(mapped_cursor.status == viper::TrackPhysicsCursorStatus::complete &&
                    mapped_cursor.serialized_object_stream_offset == 0x1C0U &&
                    mapped_cursor.preceding_car_count == 1U,
                "wobble and static rows must contribute their recovered cursor advances");

        const auto known_scene_objects = make_track_object_table({
            "obj obstacle wall", "obj wobble pole 0", "obj checkpoint finish",
            "obj ball ball", "obj static flag", "obj car 1, 1"});
        const auto known_scene_cursor =
            viper::plan_track_player_physics_cursor(known_scene_objects, 0U);
        require(known_scene_cursor.status ==
                    viper::TrackPhysicsCursorStatus::complete &&
                    known_scene_cursor.serialized_object_stream_offset == 0xD0U,
                "known obstacle, wobble, checkpoint, ball, and static prefixes must fold safely");

        const auto short_car_table = make_track_object_table({
            "obj car 1, 1", "obj car 2, 2"});
        const auto missing_cursor =
            viper::plan_track_player_physics_cursor(short_car_table, 2U);
        require(missing_cursor.status ==
                    viper::TrackPhysicsCursorStatus::player_car_not_found &&
                    !missing_cursor.serialized_object_stream_offset.has_value(),
                "missing player directive must not fabricate a stream cursor");

        constexpr std::array<std::uint32_t, 3> unknown_tail{
            0x50434152U, 0x12345678U, 0x4F425354U};
        const auto partial = viper::plan_physics_task_stream_offsets(unknown_tail);
        require(partial.status ==
                    viper::PhysicsTaskStreamPlanStatus::unknown_record_tag &&
                    partial.failed_record_index == 1U && partial.records.size() == 1U &&
                    partial.final_cursor == 0x18CU,
                "unknown tag did not stop the offset walk at its record");

        viper::PhysicsTaskRecordLoadObservation loaded_record{};
        loaded_record.serialized_object_stream_offset = 0xAD4;
        loaded_record.payload_complete = true;
        loaded_record.fourcc = 0x50434152U;
        loaded_record.allocation_succeeded = true;
        loaded_record.constructor_returned_object = true;
        loaded_record.panic_reporter_returned = true;
        const auto load_plan = viper::plan_physics_task_record_load(loaded_record);
        require(load_plan.status == viper::PhysicsTaskRecordLoadStatus::object_loaded &&
                    load_plan.constructor_argument_2_stream_offset == 0xAD4U,
                "record loader must propagate the signed cursor as constructor argument 2");

        loaded_record.serialized_object_stream_offset = -1;
        const auto end_plan = viper::plan_physics_task_record_load(loaded_record);
        require(end_plan.status == viper::PhysicsTaskRecordLoadStatus::end_of_records &&
                    !end_plan.constructor_argument_2_stream_offset.has_value() &&
                    !end_plan.payload_read,
                "negative serialized cursor is the native end-of-records sentinel");

        loaded_record.serialized_object_stream_offset = 0xAD4;
        loaded_record.payload_complete = false;
        const auto truncated_load =
            viper::plan_physics_task_record_load(loaded_record);
        require(truncated_load.status ==
                    viper::PhysicsTaskRecordLoadStatus::incomplete_payload &&
                    truncated_load.constructor_argument_2_stream_offset == 0xAD4U &&
                    truncated_load.factory_input_indeterminate,
                "failed record reads retain the already-read constructor cursor");

        constexpr std::array<std::uint32_t, 1> car_tag{0x50434152U};
        const auto overflow = viper::plan_physics_task_stream_offsets(
            car_tag, std::numeric_limits<std::uint32_t>::max() - 0x100U);
        require(overflow.status ==
                    viper::PhysicsTaskStreamPlanStatus::cursor_overflow &&
                    overflow.failed_record_index == 0U && overflow.records.empty(),
                "cursor overflow fabricated a record offset");
        std::cout << "physics task stream regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
