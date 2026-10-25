#include "viper/physics_task_stream.hpp"
#include "viper/physics_task_factory.hpp"
#include "viper/world_track_objects.hpp"

#include <algorithm>
#include <array>
#include <bit>
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
            {0x574F424CU, 0x34U},  // WOBL
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

        viper::PhysicsTaskRecordStreamSession session(0x100U);
        const auto session_first = session.append_record(car_record);
        require(session_first.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    session_first.records.size() == 1U &&
                    session_first.records[0].cursor_offset == 0x100U &&
                    session.final_cursor() == 0x28CU,
                "session stream must append a record at its current explicit cursor");
        const auto session_before_reject = session.snapshot();
        const auto session_unknown_record = make_physics_record(0x12345678U, 8U);
        const auto session_reject = session.append_record(session_unknown_record);
        require(session_reject.status ==
                    viper::PhysicsTaskRecordSerializationStatus::unknown_record_tag &&
                    session_reject.failed_record_index == 1U &&
                    session.final_cursor() == session_before_reject.final_cursor &&
                    session.snapshot().bytes == session_before_reject.bytes,
                "rejected session append must leave accumulated bytes and cursor unchanged");
        const auto session_second = session.append_record(static_record);
        const auto session_snapshot = session.snapshot();
        const auto session_decoded =
            viper::read_physics_task_record_stream(session_snapshot.bytes);
        require(session_second.status ==
                    viper::PhysicsTaskRecordSerializationStatus::complete &&
                    session_snapshot.records.size() == 2U &&
                    session_snapshot.final_cursor == 0x28CU &&
                    session_decoded.status ==
                        viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    session_decoded.records.size() == 2U &&
                    session_decoded.records[1].record_tag == 0x53544154U &&
                    session_decoded.records[1].cursor_offset == 0x28CU,
                "session snapshot must preserve appended record order and cursor framing");

        const auto decoded = viper::read_physics_task_record_stream(serialized.bytes);
        require(decoded.status == viper::PhysicsTaskRecordStreamReadStatus::complete &&
                    decoded.records.size() == 3U && decoded.consumed_bytes ==
                        serialized.bytes.size() && decoded.final_cursor == 0x2C0U &&
                    decoded.records[0].record_tag == 0x50434152U &&
                    decoded.records[0].cursor_offset == 0x100U &&
                    decoded.records[0].record_size == 0x1E4U &&
                    decoded.records[0].record_bytes_offset == 4U &&
                    decoded.records[1].cursor_offset == 0x28CU &&
                    decoded.records[2].cursor_offset == 0x28CU,
                "reader must recover serialized records and native cursor positions");
        require(viper::read_physics_task_record_stream({}).status ==
                    viper::PhysicsTaskRecordStreamReadStatus::end_of_stream,
                "empty byte span must be an empty stream");

        const std::array<std::byte, 2> partial_cursor{std::byte{0U}, std::byte{0U}};
        const auto incomplete_cursor =
            viper::read_physics_task_record_stream(partial_cursor);
        require(incomplete_cursor.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::incomplete_cursor &&
                    incomplete_cursor.failed_record_index == 0U,
                "partial cursor prefix must be reported as incomplete");
        const auto incomplete_header = viper::read_physics_task_record_stream(
            std::span<const std::byte>(serialized.bytes.data(), 8U));
        require(incomplete_header.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::incomplete_record_header &&
                    incomplete_header.final_cursor == 0x100U,
                "partial record header must retain its cursor for diagnosis");
        const auto incomplete_payload = viper::read_physics_task_record_stream(
            std::span<const std::byte>(serialized.bytes.data(), 4U + 0x1E4U - 1U));
        require(incomplete_payload.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::incomplete_record_payload,
                "short record payload must be rejected");

        auto cursor_mismatch_bytes = serialized.bytes;
        cursor_mismatch_bytes[static_entry + 4U + 0x48U] = std::byte{0x8DU};
        const auto cursor_mismatch =
            viper::read_physics_task_record_stream(cursor_mismatch_bytes);
        require(cursor_mismatch.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::cursor_mismatch &&
                    cursor_mismatch.failed_record_index == 2U,
                "subsequent record cursor must match the previous class advance");

        auto unknown_tag_bytes = serialized.bytes;
        unknown_tag_bytes[4U] = std::byte{0x78U};
        unknown_tag_bytes[5U] = std::byte{0x56U};
        unknown_tag_bytes[6U] = std::byte{0x34U};
        unknown_tag_bytes[7U] = std::byte{0x12U};
        const auto unknown_tag_read =
            viper::read_physics_task_record_stream(unknown_tag_bytes);
        require(unknown_tag_read.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::unknown_record_tag &&
                    unknown_tag_read.failed_record_index == 0U,
                "reader must reject classes without recovered cursor advances");

        auto eof_stream = serialized.bytes;
        eof_stream.insert(eof_stream.end(), {
            std::byte{0xFFU}, std::byte{0xFFU}, std::byte{0xFFU}, std::byte{0xFFU}});
        const auto eof_read = viper::read_physics_task_record_stream(eof_stream);
        require(eof_read.status ==
                    viper::PhysicsTaskRecordStreamReadStatus::end_of_stream &&
                    eof_read.records.size() == 3U &&
                    eof_read.consumed_bytes == eof_stream.size() &&
                    eof_read.final_cursor == 0x2C0U,
                "native negative cursor sentinel must end the decoded stream");
        eof_stream.push_back(std::byte{0U});
        require(viper::read_physics_task_record_stream(eof_stream).status ==
                    viper::PhysicsTaskRecordStreamReadStatus::
                        trailing_bytes_after_end_of_stream,
                "bytes after native end sentinel must be rejected");

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
        const auto signed_sentinel_cursor = viper::serialize_physics_task_records(
            std::span<const viper::PhysicsTaskRecordInput>(input_records.data(), 1U),
            0x80000000U);
        require(signed_sentinel_cursor.status ==
                    viper::PhysicsTaskRecordSerializationStatus::cursor_overflow &&
                    signed_sentinel_cursor.bytes.empty(),
                "writer must reject cursors interpreted as the native EOF sentinel");

        const auto bemidji_prefix = make_track_object_table({
            "out\\track.txt", "map test.map", "ai test.vf",
            "obj car 1, 1", "obj car 2, 2", "obj car 3, 3", "obj car 4, 4",
            "obj car 5, 5", "obj car 6, 6", "obj car 7, 7", "obj car 8, 8",
            "obj checkpoint finish 1,1 2,2"});
        const auto player_cursor =
            viper::plan_track_player_physics_cursor(bemidji_prefix, 7U);
        require(player_cursor.status == viper::TrackPhysicsCursorStatus::complete &&
                    player_cursor.serialized_object_stream_offset == 0xAD4U &&
                    player_cursor.preceding_car_count == 7U &&
                    player_cursor.preceding_records.size() == 7U &&
                    player_cursor.preceding_records.front().object_row == 3U &&
                    player_cursor.preceding_records.front().car_directive_index == 0U &&
                    player_cursor.preceding_records.front().record_tag == 0x41434152U &&
                    player_cursor.preceding_records.front().cursor_offset == 0U &&
                    player_cursor.preceding_records.back().object_row == 9U &&
                    player_cursor.preceding_records.back().car_directive_index == 6U &&
                    player_cursor.preceding_records.back().cursor_offset == 0x948U,
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
                    mapped_cursor.preceding_car_count == 1U &&
                    mapped_cursor.preceding_records.size() == 3U &&
                    mapped_cursor.preceding_records[0].object_row == 0U &&
                    !mapped_cursor.preceding_records[0].car_directive_index.has_value() &&
                    mapped_cursor.preceding_records[0].record_tag == 0x574F424CU &&
                    mapped_cursor.preceding_records[0].cursor_offset == 0U &&
                    mapped_cursor.preceding_records[0].cursor_advance == 0x34U &&
                    mapped_cursor.preceding_records[1].object_row == 1U &&
                    mapped_cursor.preceding_records[1].car_directive_index == 0U &&
                    mapped_cursor.preceding_records[1].record_tag == 0x41434152U &&
                    mapped_cursor.preceding_records[1].cursor_offset == 0x34U &&
                    mapped_cursor.preceding_records[2].object_row == 2U &&
                    !mapped_cursor.preceding_records[2].car_directive_index.has_value() &&
                    mapped_cursor.preceding_records[2].record_tag == 0x53544154U &&
                    mapped_cursor.preceding_records[2].cursor_offset == 0x1C0U &&
                    mapped_cursor.preceding_records[2].cursor_advance == 0U,
                "wobble and static rows must contribute their recovered cursor advances");

        const auto known_scene_objects = make_track_object_table({
            "obj obstacle wall", "obj wobble pole 0", "obj checkpoint finish",
            "obj static flag", "obj car 1, 1"});
        const auto known_scene_cursor =
            viper::plan_track_player_physics_cursor(known_scene_objects, 0U);
        require(known_scene_cursor.status ==
                    viper::TrackPhysicsCursorStatus::complete &&
                    known_scene_cursor.serialized_object_stream_offset == 0x9cU &&
                    known_scene_cursor.preceding_records.size() == 4U &&
                    known_scene_cursor.preceding_records[0].record_tag == 0x4F425354U &&
                    known_scene_cursor.preceding_records[1].record_tag == 0x574F424CU &&
                    known_scene_cursor.preceding_records[2].record_tag == 0x43484B50U &&
                    known_scene_cursor.preceding_records[3].record_tag == 0x53544154U &&
                    known_scene_cursor.preceding_records[3].cursor_offset == 0x9cU &&
                    known_scene_cursor.preceding_records[3].cursor_advance == 0U,
                "known obstacle, wobble, checkpoint, and static prefixes must fold safely");

        const auto unknown_ball_directive = make_track_object_table({
            "obj ball ball", "obj car 1, 1"});
        const auto unknown_ball_cursor =
            viper::plan_track_player_physics_cursor(unknown_ball_directive, 0U);
        require(unknown_ball_cursor.status ==
                    viper::TrackPhysicsCursorStatus::unsupported_prior_object &&
                    unknown_ball_cursor.unsupported_object_row == 0U &&
                    !unknown_ball_cursor.serialized_object_stream_offset.has_value(),
                "configured BALL effect must not be accepted as an obj directive");

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

        auto stat_record = make_physics_record(0x53544154U, 0x48U);
        const auto write_stat_word = [&stat_record](const std::size_t offset,
                                                     const std::uint32_t value) {
            for (std::size_t byte = 0U; byte < 4U; ++byte) {
                stat_record[offset + byte] = static_cast<std::byte>(
                    (value >> (byte * 8U)) & 0xffU);
            }
        };
        for (std::size_t index = 0U; index < 12U; ++index)
            write_stat_word(0x08U + index * 4U,
                0x100U + static_cast<std::uint32_t>(index));
        write_stat_word(0x38U, 0U);
        write_stat_word(0x3cU, std::bit_cast<std::uint32_t>(1.0F));
        write_stat_word(0x40U, std::bit_cast<std::uint32_t>(2.0F));
        write_stat_word(0x44U, std::bit_cast<std::uint32_t>(3.0F));
        const auto stat_state = viper::materialize_inactive_stat_task_runtime_state(
            stat_record, 0x80U);
        require(stat_state.status ==
                    viper::PhysicsStatTaskRuntimeMaterializationStatus::complete &&
                    stat_state.state.has_value() &&
                    stat_state.state->serialized_stream_cursor == 0x80U &&
                    stat_state.state->copied_record_words_0x38_to_0x64[0] == 0x100U &&
                    stat_state.state->copied_record_words_0x38_to_0x64[11] == 0x10bU &&
                    stat_state.state->child_center == std::array<float, 3>{1.0F, 2.0F, 3.0F} &&
                    stat_state.state->child_bound_0x28 == 3.0F &&
                    stat_state.state->child_count == 1U &&
                    !stat_state.state->active_gate &&
                    stat_state.state->update_callback_is_noop &&
                    stat_state.state->virtual_0x18_size == 0x12U,
                "STAT inactive runtime materialization lost constructor or vtable state");
        write_stat_word(0x38U, 1U);
        const auto unsupported_stat =
            viper::materialize_inactive_stat_task_runtime_state(stat_record, 0x80U);
        require(unsupported_stat.status ==
                    viper::PhysicsStatTaskRuntimeMaterializationStatus::
                        unsupported_collision_volume && !unsupported_stat.state.has_value(),
                "STAT collision-volume constructor was treated as supported");
        write_stat_word(4U, 0x47U);
        const auto malformed_stat =
            viper::materialize_inactive_stat_task_runtime_state(stat_record, 0x80U);
        require(malformed_stat.status ==
                    viper::PhysicsStatTaskRuntimeMaterializationStatus::malformed_record &&
                    !malformed_stat.state.has_value(),
                "STAT runtime materializer accepted an inconsistent declared size");

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
