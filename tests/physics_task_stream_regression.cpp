#include "viper/physics_task_stream.hpp"
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
