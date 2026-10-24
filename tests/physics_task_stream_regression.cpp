#include "viper/physics_task_stream.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
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
